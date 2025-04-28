#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>
#include <EEPROM.h>

// uncomment this to do approximation related things
//#define APPROX

#define EEPROM_SIZE 512
#define EEPROM_ADDR 0
#define RSSI_SAMPLES 5

#define UART_SERVICE_UUID      "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ============================
// 🔧 Constants & Global Flags
// ============================
const char* beaconUUID = "78563412-7856-3412-7856-341278563412";

const int buttonPin = 0; // Change to match your actual GPIO for the button
const bool useCallbackScanning = false;  // Change to true to use onResult()

bool calibrationMode = false;
bool calibrationComplete = false;
unsigned long calibrationStartTime = 0;

BLEScan* pScan; // for device scanning
BLECharacteristic *pTxCharacteristic; // tx characteristic results
BLECharacteristic *pRxCharacteristic;
bool deviceConnected = false; // set in class MyServerCallbacks

int rssiSamples[RSSI_SAMPLES];
int sampleIndex = 0;
int rssiCount = 0;  // Add at top
int smoothedRSSI = 0; 
int txPower = -59;

// ============================
// 📐 Beacon Structs
// ============================
#define MAX_BEACONS 5

// Struct for fixed beacon metadata
struct BeaconInfo {
  uint16_t major;
  float x;
  float y;
};

// Struct for scan results
struct BeaconReading {
  uint16_t major;
  float x;
  float y;
  int rssi;
  float distance;
};

// Temp structure used in calibration
typedef struct {
  uint16_t major;
  float distance;
  bool seen;
} TempCalib;

// ============================
// 🧠 Beacon State Variables
// ============================
BeaconInfo knownBeacons[MAX_BEACONS] = {
  {1, 0.0, 0.0},
  {2, 2.0, 0.0},
  {3, 1.0, 2.0},
  {4, 1.0, 0.0}
};

BeaconInfo knownBeaconsBackup[MAX_BEACONS];
BeaconReading activeBeacons[MAX_BEACONS];
int beaconCount = 0;

int lastRSSI = 0;
float lastDistance = 0.0;
unsigned long lastSeen = 0;
const uint16_t targetMajor = 1; // this is unused?

// ============================
// 💾 EEPROM Save/Load
// ============================
void saveCoordinatesToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_ADDR, knownBeacons);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("💾 Beacon coordinates saved to EEPROM.");
}

void loadCoordinatesFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR, knownBeacons);
  EEPROM.end();
  Serial.println("📥 Beacon coordinates loaded from EEPROM.");
}

/**
 * # Misc. utility functions
 */
#ifdef APPROX
float averageDistance(){
  if (rssiCount == 0) return 0;
  int sum = 0;
  for (int i = 0; i < rssiCount; i++) sum += rssiSamples[i];
  float avgRSSI = (float)sum / rssiCount;
  return estimateDistance(avgRSSI);
}
#endif

float estimateDistance(int rssi) {
  float ratio = rssi * 1.0 / txPower;
  if (ratio < 1.0) return pow(ratio, 10);
  else return 0.89976 * pow(ratio, 7.7095) + 0.111;
}

bool getBeaconCoordinates(uint16_t major, float &x, float &y) {
  for (int i = 0; i < MAX_BEACONS; i++) {
    if (knownBeacons[i].major == major) {
      x = knownBeacons[i].x;
      y = knownBeacons[i].y;
      return true;
    }
  }
  return false;
}

uint16_t readUInt16BE(const uint8_t* data, int offset) {
  return ((uint16_t)data[offset] << 8) | data[offset + 1];
}

// Small method for handling the button press
// @TODO maybe make this an interrupt?
bool buttonPressed() {
  static bool lastState = HIGH;
  bool currentState = digitalRead(buttonPin);
  if (lastState == HIGH && currentState == LOW) {
    delay(50); // debounce
    lastState = currentState;
    return true;
  }
  lastState = currentState;
  return false;
}

/**
 * # Callbacks for UART stuff
 * - `MyServerCallbacks` handles server connection stuff
 * - `UARTCallbacks` handles actual communication stuff, via the UART characteristic
 */
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("[UARTCallbacks] Device connected. :)");
    // pTxCharacteristic->setValue("Hello from ESP32!");
    // pTxCharacteristic->notify();
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("[UARTCallbacks] Device disconnected. :(");
  }
};

class UARTCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      Serial.print("Received: ");
      Serial.println(rxValue.c_str());
    }
  }
};

/**
 * # BLE Advertised Device Callback
 * This is the callback triggered when we initially reach out to a beacon - this is ran once.
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (!advertisedDevice.haveManufacturerData()) return;

    String rawStr = advertisedDevice.getManufacturerData();
    const uint8_t* payload = (const uint8_t*)rawStr.c_str();

    if (rawStr.length() == 25 && payload[0] == 0x4C && payload[1] == 0x00 &&
        payload[2] == 0x02 && payload[3] == 0x15) {

      char uuidStr[37];
      sprintf(uuidStr,
        "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        payload[4], payload[5], payload[6], payload[7],
        payload[8], payload[9], payload[10], payload[11],
        payload[12], payload[13], payload[14], payload[15],
        payload[16], payload[17], payload[18], payload[19]);

      uint16_t major = (payload[20] << 8) | payload[21];
      uint16_t minor = (payload[22] << 8) | payload[23];
      int rssi = advertisedDevice.getRSSI();
      float dist = estimateDistance(rssi);

      rssiSamples[sampleIndex % RSSI_SAMPLES] = rssi;
      sampleIndex++;
      if (rssiCount < RSSI_SAMPLES) {
        rssiCount++;
        smoothedRSSI = rssi;
      } else {
        smoothedRSSI = 0.8 * smoothedRSSI + 0.2 * rssi;
      }

      if (String(uuidStr) == beaconUUID) {
        lastRSSI = rssi;
        lastDistance = dist;
        lastSeen = millis();

        Serial.printf("[0x%04x] RSSI: %d, Est. Distance: %.2f\n", major, rssi, dist);
        if (deviceConnected) {
          char json[128];
          snprintf(json, sizeof json, "{'success':true,'data':{'id':0x%04x,'rssi':%d}}", major, rssi);
          pTxCharacteristic->setValue(json);
          pTxCharacteristic->notify();
        }
      }
    }
  }
};

/**
 * # Calibration Mode Logic
 */
void enterCalibrationMode() {
  calibrationMode = true;
  calibrationComplete = false;
  calibrationStartTime = millis();
  memcpy(knownBeaconsBackup, knownBeacons, sizeof(knownBeacons));
  Serial.println("\n🚧 Entering Calibration Mode - Place beacons in cross layout (X and Y axes)");

  TempCalib temp[MAX_BEACONS] = {};
  int seenCount = 0;
  const unsigned long CALIBRATION_TIMEOUT = 15000;

  while (calibrationMode) {
    if (buttonPressed()) {
      Serial.println("❌ Calibration interrupted, restoring previous coordinates.");
      memcpy(knownBeacons, knownBeaconsBackup, sizeof(knownBeacons));
      calibrationMode = false;
      return;
    }

    // a slight 2s delay is introduced here with pScan
    BLEScanResults* results = pScan->start(2, false);
    for (int i = 0; i < results->getCount(); i++) {
      BLEAdvertisedDevice d = results->getDevice(i);
      if (d.haveManufacturerData()) {
        String data = d.getManufacturerData();
        if (data.length() == 25 && 
            (uint8_t)data[0] == 0x4C && (uint8_t)data[1] == 0x00 &&
            (uint8_t)data[2] == 0x02 && (uint8_t)data[3] == 0x15) {

          BLEBeacon beacon;
          beacon.setData(data);
          if (beacon.getProximityUUID().toString() == beaconUUID) {
            uint16_t major = beacon.getMajor();
            float dist = estimateDistance(d.getRSSI());

            for (int j = 0; j < MAX_BEACONS; j++) {
              if (knownBeacons[j].major == major && !temp[j].seen) {
                temp[j].major = major;
                temp[j].distance = dist;
                temp[j].seen = true;
                seenCount++;
                Serial.printf("📌 Calibrated Beacon %d with estimated distance %.2f m\n", major, dist);
              }
            }
          }
        }
      }
    }
    pScan->clearResults();

    if (seenCount >= 4 || millis() - calibrationStartTime > CALIBRATION_TIMEOUT) {
      Serial.println("✅ Calibration complete. Assigning coordinates based on cross layout...");
      for (int i = 0; i < MAX_BEACONS; i++) {
        if (temp[i].seen) {
          switch (temp[i].major) {
            // in order: 0deg, 90deg, 180deg, 270deg
            case 1: knownBeacons[i] = {1, -temp[i].distance, 0}; break;
            case 2: knownBeacons[i] = {2,  temp[i].distance, 0}; break;
            case 3: knownBeacons[i] = {3,  0, temp[i].distance}; break;
            case 4: knownBeacons[i] = {4,  0, -temp[i].distance}; break;
            default: knownBeacons[i] = {temp[i].major, 0, 0}; break;
          }
        } else {
          knownBeacons[i].x = knownBeacons[i].x == 0 ? 1.0 : knownBeacons[i].x;
          knownBeacons[i].y = knownBeacons[i].y == 0 ? 0.0 : knownBeacons[i].y;
        }
      }
      calibrationMode = false;
      calibrationComplete = true;
      saveCoordinatesToEEPROM();
      Serial.println("📍 Beacon coordinates updated.");
    }
  }
}

/**
 * # Scanning routine
 */
void scanForBeacons() {
  BLEScanResults* results = pScan->start(1, false);
  int count = results->getCount();
  bool found = false;

  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice d = results->getDevice(i);
    if (d.haveManufacturerData()) {

      String rawStr = d.getManufacturerData();
      const uint8_t* payload = (const uint8_t*)rawStr.c_str();

      if (rawStr.length() == 25 && payload[0] == 0x4C && payload[1] == 0x00 && payload[2] == 0x02 && payload[3] == 0x15 ) {

        char uuidStr[37];
        sprintf(uuidStr,
          "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
          payload[4], payload[5], payload[6], payload[7],
          payload[8], payload[9],
          payload[10], payload[11],
          payload[12], payload[13],
          payload[14], payload[15], payload[16], payload[17], payload[18], payload[19]
        );

        uint16_t major = readUInt16BE(payload, 20);
        uint16_t minor = readUInt16BE(payload, 22);
        int rssi = d.getRSSI();
        float dist = estimateDistance(rssi);

        rssiSamples[sampleIndex % RSSI_SAMPLES] = rssi;
        sampleIndex++;
        if (rssiCount < RSSI_SAMPLES) {
          rssiCount++;
          smoothedRSSI = rssi;
        } else {
          smoothedRSSI = 0.8 * smoothedRSSI + 0.2 * rssi;
        }

        if (String(uuidStr) == beaconUUID) {
          lastRSSI = rssi;
          lastDistance = dist;
          lastSeen = millis();
          found = true;

          Serial.printf("[0x%04x] RSSI: %d, Est. Distance: %.2fm\n", major, rssi, dist);
          if (deviceConnected) {
            char json[128];
            snprintf(json, sizeof json, "{\"success\":true,\"data\":{\"id\":%d,\"rssi\":%d}}", major, rssi); // note: json does not support hex numbering
            pTxCharacteristic->setValue(json);
            pTxCharacteristic->notify();
          }
        }
      }
    }
  }
  
  pScan->clearResults();
}

/**
 * Setup and loop stuff
 */
void setup() {
  pinMode(buttonPin, INPUT_PULLUP); // set button pin to be pull up
  Serial.begin(115200);

  BLEDevice::init("Receiver");

  // Setup BLE server and UART service
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(UART_SERVICE_UUID);

  // Create TX characteristic (notify only)
  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );

  // Create RX characteristic (write only)
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new UARTCallbacks());

  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(UART_SERVICE_UUID);
  pAdvertising->start();

  Serial.println("📡 BLE UART Service is advertising...");

  // BLE Scan setup
  pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  if (useCallbackScanning) {
    pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    Serial.println("🧠 Callback scanning enabled.");
  } else {
    Serial.println("🧠 Manual scanning enabled.");
  }

  loadCoordinatesFromEEPROM();
  Serial.println("🔍 Starting scan loop...");
}

void loop() {
  if (buttonPressed() && !calibrationMode) enterCalibrationMode();
  if (!useCallbackScanning) scanForBeacons();  // Only call if not using onResult()

  if (millis() - lastSeen < 5000) {
    //Serial.printf("Live distance estimate: %.2fm (RSSI: %d)\n", lastDistance, lastRSSI);

    #ifdef APPROX
    Serial.print("📡 Average Distance: ");
    Serial.print(averageDistance());
    Serial.println(" (can be wonky)");
    #endif 

    Serial.print("SmoothedRSSI Distance: ");
    Serial.print(estimateDistance(smoothedRSSI));
    Serial.println("m (can be wonky)\n");
  } else {
    Serial.println("⚠️  Beacon not seen recently...");
    
    if (deviceConnected) {
      pTxCharacteristic->setValue("{\"success\":false,\"error\":\"no_beacon_seen\"}");
      pTxCharacteristic->notify();
    }
  }
  delay(100);
}

/*
// baby non-standard json schema
{
  "success": boolean,
  "error": string, present if success is false
  "data": object {
    "message": string,
    ...other stuff, as needed (depends on message string)
  }
}
*/


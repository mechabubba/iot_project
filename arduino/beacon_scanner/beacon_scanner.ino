#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>

const char* beaconUUID = "12345678-1234-5678-1234-567812345678";

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

// Define known beacons (up to 5 for example)
#define MAX_BEACONS 5
BeaconInfo knownBeacons[MAX_BEACONS] = {
  {1, 0.0, 0.0},
  {2, 2.0, 0.0},
  {3, 1.0, 2.0}
};

// Readings storage
BeaconReading activeBeacons[MAX_BEACONS];
int beaconCount = 0;

// Store latest beacon reading
int lastRSSI = 0;
float lastDistance = 0.0;
unsigned long lastSeen = 0;
const uint16_t targetMajor = 1;  // Optional: filter specific beacon


// RSSI-to-distance estimator
float estimateDistance(int rssi, int txPower = -59) {
  float ratio = rssi * 1.0 / txPower;
  if (ratio < 1.0) return pow(ratio, 10);
  else return 0.89976 * pow(ratio, 7.7095) + 0.111;
}

// Helper to get beacon coordinates by Major ID
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

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (!advertisedDevice.haveManufacturerData()) return;

    String data = advertisedDevice.getManufacturerData();
    if (data.length() != 25) return;

    // Check for Apple + iBeacon prefix
    if ((uint8_t)data[0] == 0x4C && (uint8_t)data[1] == 0x00 && (uint8_t)data[2] == 0x02 && (uint8_t)data[3] == 0x15) {
      BLEBeacon beacon;
      beacon.setData(data);

      // Check for your UUID
      String uuid = beacon.getProximityUUID().toString();
      if (uuid == "78563412-7856-3412-7856-341278563412") { // && beacon.getMajor() == targetMajor
        int rssi = advertisedDevice.getRSSI();
        float distance = estimateDistance(rssi);

        // Save last seen data
        lastRSSI = rssi;
        lastDistance = distance;
        lastSeen = millis();

        Serial.println("✅ Found our beacon!");
        Serial.printf("UUID: %s\n", uuid.c_str());
        Serial.printf("Major: %d, Minor: %d, RSSI: %d, Est. distance: %.2f m\n", beacon.getMajor(), beacon.getMinor(), rssi, distance);
        Serial.println("----------------------------");
      }
    }
  }
};


BLEScan* pScan;

void setup() {
  Serial.begin(115200);
  BLEDevice::init("Receiver");
  pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  Serial.println("🔍 Starting manual scan loop...");
}

void loop() {
  BLEScanResults* results = pScan->start(2, false);  // Scan for 2 seconds
  int count = results->getCount();  // ✅ correct pointer usage
  bool found = false;

  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice d = results->getDevice(i);
    if (d.haveManufacturerData()) {
      String data = d.getManufacturerData();
      if (data.length() == 25 && (uint8_t)data[0] == 0x4C && (uint8_t)data[1] == 0x00 && 
          (uint8_t)data[2] == 0x02 && (uint8_t)data[3] == 0x15) {
        
        BLEBeacon beacon;
        beacon.setData(data);

        if (beacon.getProximityUUID().toString() == "78563412-7856-3412-7856-341278563412" ) { //&& beacon.getMajor() == 1

          int rssi = d.getRSSI();
          float distance = estimateDistance(rssi);

          lastRSSI = rssi;
          lastDistance = distance;
          lastSeen = millis();
          found = true;

          Serial.println("✅ Found our beacon!");
          Serial.printf("UUID: %s\n", beacon.getProximityUUID().toString().c_str());
          Serial.printf("Major: %d, Minor: %d, RSSI: %d, Est. distance: %.2f m\n",
                        beacon.getMajor(), beacon.getMinor(), rssi, distance);
          Serial.println("----------------------------");
        }
      }
    }
  }

  pScan->clearResults();  // Always clean up after scan

  if (millis() - lastSeen < 5000) {
    Serial.printf("📡 Live Distance Estimate: %.2f m (RSSI: %d)\n", lastDistance, lastRSSI);
  } else {
    Serial.println("⚠️  Beacon not seen recently...");
  }

  delay(1000);
}




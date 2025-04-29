#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEBeacon.h>
#include "esp_mac.h"  // required - exposes esp_mac_type_t values


// 👉 Globals
uint16_t majorID = 0; // Global variable to hold parsed ID
#define MINOR_ID 0
#define LED_PIN 2
BLEBeacon beacon;
BLEAdvertisementData advData;


void setup() {
  delay(2000); // ⏳ Wait for serial monitor to catch up
  Serial.begin(115200);
  delay(100);  // Ensure port is ready
  Serial.println("✅ Serial started!");

  // set led to be on
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  String majStr = getMajID();       // Only call ONCE
  Serial.println(majStr);

  majorID = (uint16_t) strtol(majStr.c_str(), NULL, 16);
  Serial.println(majorID);

  // Initialize BLE
  BLEDevice::init("Beacon_" + String(majorID));
  
  // ✅ Set Tx Power to maximum
  BLEDevice::setPower(ESP_PWR_LVL_P6);  // +6 dBm


  // Setup beacon
  beacon.setManufacturerId(0x4C00);  // Apple's iBeacon ID
  beacon.setProximityUUID(BLEUUID("12345678-1234-5678-1234-567812345678"));
  beacon.setMajor(majorID);
  beacon.setMinor(MINOR_ID);
  beacon.setSignalPower(-59);

  advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  advData.setManufacturerData(beacon.getData());

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();


  Serial.printf("✅ Beacon %d started and broadcasting!\n", majorID);
}

void loop() {
  //simBeaconBroadcast();
  delay(10); // Keep alive
}

String getMajID() {
  String espMacBT = getInterfaceMacAddress(ESP_MAC_BT);
  Serial.println(espMacBT);

  String cleanMAC = "";

  // Remove colons from input
  for (int i = 0; i < espMacBT.length(); i++) {
    if (espMacBT.charAt(i) != ':') {
      cleanMAC += espMacBT.charAt(i);
    }
  }

  // Return last 4 characters
  return cleanMAC.substring(cleanMAC.length() - 4);
}

String getInterfaceMacAddress(esp_mac_type_t interface) {
  String mac = "";
  unsigned char mac_base[6] = {0};
  if (esp_read_mac(mac_base, interface) == ESP_OK) {
    char buffer[18];  // 6*2 characters for hex + 5 characters for colons + 1 character for null terminator
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1], mac_base[2], mac_base[3], mac_base[4], mac_base[5]);
    mac = buffer;
  }
  return mac;
}

/*
void simBeaconBroadcast() {
  String rawData = beacon.getData();  // Still safe to call this

  Serial.print("📡 Broadcasted Beacon - Major: ");
  Serial.print(majorID);
  Serial.print(", Minor: ");
  Serial.println(MINOR_ID);

  Serial.print("🧱 Raw Advertisement Packet: ");
  for (size_t i = 0; i < rawData.length(); i++) {
    char hexByte[3];
    sprintf(hexByte, "%02X", (uint8_t)rawData[i]);
    Serial.print(hexByte);
    Serial.print(" ");
  }
  Serial.println();
}
*/

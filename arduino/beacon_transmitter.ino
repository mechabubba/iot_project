#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEBeacon.h>

// 👉 CHANGE THIS ON EACH BEACON
#define MAJOR_ID 1
#define MINOR_ID 0

void setup() {
  Serial.begin(115200);

  // Initialize BLE
  BLEDevice::init("Beacon_" + String(MAJOR_ID));

  // Create iBeacon
  BLEBeacon beacon;
  beacon.setManufacturerId(0x4C00);  // Apple's iBeacon ID
  beacon.setProximityUUID(BLEUUID("12345678-1234-5678-1234-567812345678"));  // Shared UUID for all beacons
  beacon.setMajor(MAJOR_ID);  // Unique per beacon
  beacon.setMinor(MINOR_ID);
  beacon.setSignalPower(-59);  // Calibrated Tx Power at 1m

  // Advertising data
  BLEAdvertisementData advData;
  advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  advData.setManufacturerData(beacon.getData());

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();

  Serial.printf("✅ Beacon %d started and broadcasting!\n", MAJOR_ID);
}

void loop() {
  delay(1000); // Keep alive
}

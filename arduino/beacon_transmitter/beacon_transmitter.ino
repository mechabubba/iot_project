#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEBeacon.h>
#include "esp_bt_device.h"
#include "esp_mac.h" // required - exposes esp_mac_type_t values

// gets the last four bytes of the bluetooth mac uid
uint16_t esp_mac_uid() {
  uint16_t uid = 0x0000;
  unsigned char mac_base[6] = {0};
  if (esp_read_mac(mac_base, ESP_MAC_BT) == ESP_OK) {
    //Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1], mac_base[2], mac_base[3], mac_base[4], mac_base[5]);
    uid = ((uint16_t)mac_base[4] << 8) + mac_base[5];
  }
  return uid;
}

// the iBeacon
BLEBeacon beacon;

void setup() {
  Serial.begin(115200);

  // Initialize BLE
  uint16_t uid = esp_mac_uid();
  BLEDevice::init("Beacon_" + String(uid));

  beacon.setManufacturerId(0x4C00); // Apple's iBeacon ID
  beacon.setProximityUUID(BLEUUID("12345678-1234-5678-1234-567812345678"));  // Shared UUID for all beacons
  beacon.setMajor(uid);
  beacon.setMinor(0);
  beacon.setSignalPower(-59); // Calibrated Tx Power at 1m

  // Advertising data
  BLEAdvertisementData advData;
  advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  advData.setManufacturerData(beacon.getData());

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();

  Serial.printf("✅ Beacon %d started and broadcasting!\n", uid);
}

void loop() {
  //esp_mac_uid();
  Serial.printf("✅ Beacon %x started and broadcasting!\n", beacon.getMajor());
  delay(10000); // Keep alive
}

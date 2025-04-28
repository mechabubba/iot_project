const { createBluetooth } = require("node-ble");
const fetch = require("node-fetch");

// Replace with your Arduino's Bluetooth address
//const ARDUINO_BLUETOOTH_ADDR = '1B:91:AE:F6:85:53';
const ARDUINO_BLUETOOTH_ADDR = '40:4C:CA:57:7F:7E';

const UART_SERVICE_UUID = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E';
const TX_CHARACTERISTIC_UUID = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E';
const RX_CHARACTERISTIC_UUID = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E';
 
//const ESS_SERVICE_UUID = '0000181a-0000-1000-8000-00805f9b34fb';
//const TEMP_CHAR_UUID = '00002a6e-0000-1000-8000-00805f9b34fb';
//const HUMIDITY_CHAR_UUID = '00002a6F-0000-1000-8000-00805f9b34fb';

async function main() {
    const { bluetooth, destroy } = createBluetooth();
    const adapter = await bluetooth.defaultAdapter();
    await adapter.startDiscovery();
    
    console.log('discovering...');
    const device = await adapter.waitDevice(ARDUINO_BLUETOOTH_ADDR.toUpperCase());
    console.log('found device. attempting connection...');
    await device.connect();
    console.log('connected to device!');

    const gattServer = await device.gatt();
    const uartService = await gattServer.getPrimaryService(UART_SERVICE_UUID.toLowerCase());
    const txChar = await uartService.getCharacteristic(TX_CHARACTERISTIC_UUID.toLowerCase());
    const rxChar = await uartService.getCharacteristic(RX_CHARACTERISTIC_UUID.toLowerCase());
 
    //const essService = await gattServer.getPrimaryService(ESS_SERVICE_UUID.toLowerCase());
    //const tempChar = await essService.getCharacteristic(TEMP_CHAR_UUID.toLowerCase());

    // Start temperature notifications first
    //await tempChar.startNotifications();
    //tempChar.on('valuechanged', buffer => {
    //    // Buffer is 2 bytes: Little Endian int16
    //    const tempInt = buffer.readInt16LE(0);
    //    const temperature = tempInt / 100.0; // Convert back to Celsius
    //    console.log(`Temperature received: ${temperature} °C`);
    //});

    // Start RX notifications
    await rxChar.startNotifications();
    rxChar.on('valuechanged', buffer => {
        buffer = buffer.toString();
        console.log("Received from Arduino (RX): " + buffer);

        let json;
        try {
            json = JSON.parse(buffer)
        } catch(e) {
            console.error("An error occured while parsing the received buffer;");
            console.error(e);
            exit(1);
        }

        // idk do smth
        console.log(json);

    });
}
 
main().then((ret) => {
    if (ret) console.log(ret);
}).catch((err) => {
    if (err) console.error(err);
});


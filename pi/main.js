require("dotenv").config();
const { createBluetooth } = require("node-ble");
const fetch = require("node-fetch");

// Replace with your Arduino's Bluetooth address
const ARDUINO_BLUETOOTH_ADDR = "40:4C:CA:57:7F:7E";

const UART_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
const TX_CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
const RX_CHARACTERISTIC_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

if (!process.env.FLASK_SERVER) {
    console.error("No flask server configured in environment variables! Please create a \`.env\` file and launch again.");
    exit(1);
}

async function main() {
    const { bluetooth, destroy } = createBluetooth();
    const adapter = await bluetooth.defaultAdapter();
    await adapter.startDiscovery();
    
    console.log("Discovering receiver...");
    const device = await adapter.waitDevice(ARDUINO_BLUETOOTH_ADDR.toUpperCase());
    console.log("Found receiver! Attempting connection...");
    await device.connect();
    console.log("Connected to device!");

    const gattServer = await device.gatt();
    const uartService = await gattServer.getPrimaryService(UART_SERVICE_UUID.toLowerCase());
    const txChar = await uartService.getCharacteristic(TX_CHARACTERISTIC_UUID.toLowerCase());
    const rxChar = await uartService.getCharacteristic(RX_CHARACTERISTIC_UUID.toLowerCase());
 
    // Start RX notifications
    await rxChar.startNotifications();
    rxChar.on("valuechanged", buffer => {
        buffer = buffer.toString();
        let json;
        try {
            json = JSON.parse(buffer);
        } catch(e) {
            console.error("Could not parse the received buffer. :(");
            console.error(e);
            if (process.env.DEBUG) exit(1);
        }
        if (process.env.DEBUG) console.log(json);

        if (json.success) {
            switch(json.event) {
                case "transmission": {
                    const resp = await fetch(`${process.env.FLASK_SERVER}/api/posts`, {
                        method: "POST",
                        body: buffer
                    });
                    const data = await resp.json();
                    break;
                }
                default: {
                    console.error("Huh?");
                }
            }
        } else {
            console.error("Error from receiver;");
            console.error(json.error);
        }
    });
}
 
main().then((ret) => {
    if (ret) console.log(ret);
}).catch((err) => {
    if (err) console.error(err);
});


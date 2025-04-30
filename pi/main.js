require("dotenv").config();
const { createBluetooth } = require("node-ble");
const fetch = require("node-fetch");

// Replace with your Arduino's Bluetooth address
const ARDUINO_BLUETOOTH_ADDR = "40:4C:CA:57:7F:7E";

const UART_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
const TX_CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
const RX_CHARACTERISTIC_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

if (!process.env.FLASK_SERVER) {
    console.error("No flask server configured in environment! Exiting...");
    process.exit(1);
}

if (!process.env.FLASK_LOGIN_USERNAME || !process.env.FLASK_LOGIN_PASSWORD) {
    console.error("Missing login information in environment! Exiting...");
    process.exit(1);
}

/**
 * this setup is kind of ass but its built on top of infrastructure thats already there
 */
async function main() {
    // authenticate with the webserver
    console.log("Logging in...");
    const authResp = await fetch(`${process.env.FLASK_SERVER}/login`, {
        method: "POST",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded"
        },
        body: new URLSearchParams({
            email: process.env.FLASK_LOGIN_USERNAME,
            password: process.env.FLASK_LOGIN_PASSWORD
        }),
        redirect: "manual" // DO NOT REDIRECT. we miss the cookie if we do this.
    });

    const cookies = authResp.headers.raw()["set-cookie"];
    const session = (cookies[0] ?? "").match(/session=(.*?);/)[1]; 
    if (!session) {
        // we cant test if we 200 here because the redirect has the cookie we need and has a 301 code
        console.error("Session cookie not found. Here's the text;");
        console.error(await authResp.text());
        process.exit(1);
    }

    // initiate the bluetooth connection
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
    rxChar.on("valuechanged", async buffer => {
        buffer = buffer.toString();
        let json;
        try {
            json = JSON.parse(buffer);
        } catch(e) {
            console.error("Could not parse the received buffer (ignoring). :(");
            console.error(e);
            return;
        }
        console.log(json);

        if (json.success) {
            switch(json.event) {
                case "transmission": {
                    try {
                        await fetch(`${process.env.FLASK_SERVER}/api/receiver`, {
                            method: "POST",
                            headers: {
                                "Content-Type": "application/json",
                                "Cookie": `session=${session};`,
                            },
                            body: JSON.stringify(json.data)
                        });
                    } catch(e) {
                        console.error("Error POSTing to flask (ignoring);");
                        console.error(e);
                    }

                    break;
                }
                default: {
                    console.error("Huh?");
                }
            }
        } else {
            console.error("Error from receiver;");
            console.error(json.error);
            return;
        }
    });
}
 
main().then((ret) => {
    if (ret) console.log(ret);
}).catch((err) => {
    if (err) console.error(err);
});

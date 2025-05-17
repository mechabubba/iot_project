# iot\_project
Final project for ECE5550 Internet of Things.

There are four-ish components here;
- `arduino/beacon_scanner` is the receiver code - this is what takes Bluetooth communication from the beacons, determines the signal strength of those communications, and sends that data to the Pi over Bluetooth.
- `arduino/beacon_transmitter` is the beacon code.
- `pi` is the Raspberry Pi code that forwards the data to the web server. 
- `web` is the web server, the user-facing interface.

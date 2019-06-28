# sensaurus

The repository contains Arduino/ESP32 code for the [Sensaurus project](https://www.sensaurus.org/). These sketches/libraries are used for the hub board 
and sensaur/actuator devices that plug into the hub board.

The device boards connect to the hub using audio cables that carry power and serial data. The communication between the hub and the devices 
uses a single serial line for both transmitting and receiving (i.e. half-duplex serial).

## Owner/hub/device/component IDs

The owner ID corresponds to the person or organization who owns or manages a device. It is assigned by the server.

The hub ID is assigned using the hub configuration utility. (For ESP32 hubs, we expect to do configuration over BLE.)

The device ID is a 32-bit integer (displayed in hexidecimal) that is self-assigned by each device (sensor/actuator board). 
The device ID is stored on the device and preserved when it is moved from one hub to another. Device IDs are randomly generated, 
so they may not be globally unique, though it should be reasonable to assume they are unique within a single owner.

Each device can have multiple components (e.g. temperature and humidity). Components are identified by the device ID, followed by a dash, followed by the 
first five letters of component type. (It is up to the device designer to make sure the first five letters of the component types are not repeated on 
a single device.)

Together these form a hierarchy: one owner can have multiple hubs, one hub can have multiple devices, and one device can have multiple components.

## MQTT topics

The hub can communicate with an MQTT server. We use the following topics:

*   `[owner_id]/hub/[hub_id]/status` (JSON dictionary with wifi, firmware, etc. info from hub)
*   `[owner_id]/hub/[hub_id]/command` (JSON dictionary with command name and arguments)
*   `[owner_id]/hub/[hub_id]/devices` (JSON dictionary of device info by device_id)
*   `[owner_id]/hub/[hub_id]/sensors` (JSON dictionary with sensor values (from hub) by component_id)
*   `[owner_id]/hub/[hub_id]/actuators` (JSON dictionary with actuator values (for hub) by component_id)
*   `[owner_id]/device/[device_id]` (string value of current hub_id for this device)

## Installing ESP32 software

1.  Install Arduino IDE
2.  Go to Arduino IDE preferences; set `Additional Board Manager URLs` to `https://dl.espressif.com/dl/package_esp32_index.json`
3.  Go to `Tools` / `Board` / `Board Manager...`, search for `esp32`, select result and click `Install`

## Compiling the code

1.  Go to Arduino IDE preferences and set the `Sketchbook location` to the `sensaurus` folder. This allows the IDE to find the required libraries.
2.  Restart the Arduino IDE.
3.  Copy `sample_settings.h` to `settings.h` and edit it as needed. You'll need MQTT server information and WiFi network information.
4.  Select `DOIT ESP32 DEVKIT V1` from board list (or other board type if needed).
5.  Press Ctrl-R (or Command-R) to compile. It may take a little while given the various library dependencies.

## Uploading to ESP32

1.  Plug in the ESP32 board.
2.  Select `DOIT ESP32 DEVKIT V1` from board list (or other board type if needed).
3.  Select serial port.
4.  Press Ctrl-U (or Command-U) to upload.
5.  After IDE shows `Connecting...`, push `BOOT` button on ESP32 for several seconds (until upload starts).

## Running the hub simulator

The simulator is in the `hub-sim` folder. 

1.  Use pip to install `hjson` and `paho-mqtt`.
2.  Use the AWS IoT web interface to create a new "thing." Get the certificates and host name from the web interface.
3.  Create a sub-folder called `cert` in `hub-sim` containing the key and certificates for this hub.
4.  Copy `sample_config.hjson` to `config.hjson` and edit the host, certificate paths, etc. as needed.
    (You can leave the `owner_id` and `hub_id` as is for now.)
5.  Run `sim.py`.

## Running the remote test code

The remote test code is in the `hub-remote-test` folder. This script can be used to interact with a remote (or local) hub via MQTT messages.
It will send a configuration message to the hub and listen for messages from the hub. 

1.  Set up the simulator code as described above.
2.  Copy your config.hjson file to the `hub-remote-test` folder.
3.  Run `remote_test.py`.
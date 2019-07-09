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

### Building with Changed Partitions

In order to allow BLE to fit into sketch/firmware, the default partition table had to be modified.
As of July 2019, for experimental reasons, we allow compiling without BLE, with default partition table,
but that experimental feature will be disabled in the future.
To disable BLE compilation, uncomment #define ENABLE_BLE line in the main source file.

To build with changed partitions and be able to test BLE mode:

* create a new board definition in boards.txt that looks something like the one below
 (this assumes esp32 core source resides in sensaurus/hardware/expressive)

* create a new partition description file max.csv and place it in espressif/esp32/tools/partitions:

  nvs,      data, nvs,     0x9000,  0x5000,
  otadata,  data, ota,     0xe000,  0x2000,
  app0,     app,  ota_0,   0x10000, 0x280000,
  spiffs,   data, spiffs,  0x290000,0x170000,

 * restart Arduino IDE and select the new board
 * build and test

---
#### Board definition for  node32smax: Node32s Dev Module (Max Memory)

  node32smax.name=Node32s Dev Module (Max Memory)

  node32smax.upload.tool=esptool_py
  node32smax.upload.maximum_size=1703936
  node32smax.upload.maximum_data_size=327680
  node32smax.upload.wait_for_upload_port=true

  node32smax.serial.disableDTR=true
  node32smax.serial.disableRTS=true

  node32smax.build.mcu=esp32
  node32smax.build.core=esp32
  node32smax.build.variant=node32smax
  node32smax.build.board=Node32sMaxMem

  node32smax.build.f_cpu=240000000L
  node32smax.build.flash_mode=dio
  node32smax.build.flash_size=4MB
  node32smax.build.boot=dio
  node32smax.build.partitions=max
  node32smax.build.defines=

  node32smax.menu.FlashFreq.80=80MHz
  node32smax.menu.FlashFreq.80.build.flash_freq=80m
  node32smax.menu.FlashFreq.40=40MHz
  node32smax.menu.FlashFreq.40.build.flash_freq=40m

  node32smax.menu.UploadSpeed.921600=921600
  node32smax.menu.UploadSpeed.921600.upload.speed=921600
  node32smax.menu.UploadSpeed.115200=115200
  node32smax.menu.UploadSpeed.115200.upload.speed=115200
  node32smax.menu.UploadSpeed.256000.windows=256000
  node32smax.menu.UploadSpeed.256000.upload.speed=256000
  node32smax.menu.UploadSpeed.230400.windows.upload.speed=256000
  node32smax.menu.UploadSpeed.230400=230400
  node32smax.menu.UploadSpeed.230400.upload.speed=230400
  node32smax.menu.UploadSpeed.460800.linux=460800
  node32smax.menu.UploadSpeed.460800.macosx=460800
  node32smax.menu.UploadSpeed.460800.upload.speed=460800
  node32smax.menu.UploadSpeed.512000.windows=512000
  node32smax.menu.UploadSpeed.512000.upload.speed=512000

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

## Settings via BLE and Hub Dual Mode

Sensaurus allows configuration via Bluetooth Low Energy (BLE). But in order to do that we have to overcome memory
limitation of ESP32 where both BLE and AWS IOT doesn't fit into memory. Therefore, when in BLE mode, AWS IOT
will be disabled and switching modes will require reboot.

When in BLE mode, configuration can be changed using Web based Sensaurus BLE Configuration Client in ble-config directory.

When the hub starts up, it will normally start in AWS IOT mode.
If during the start up, hub can't connect to WIFI it will start in BLE mode.

There are 3 ways to switch between BLE mode and AWS IOT mode:

* send command "bleStart" (to switch to BLE mode) or "bleExit" (to exit BLE mode and switch to AWS IOT mode)
  via BLE command attribute or via AWS IOT/MQTT command
* press settings/configuration button on the hub (this will switch to BLE mode)
* press the restart button on the hub (this will switch to AWS IOT mode)

Note:
As of July 2019, the default button to use to force Hub that's running in AWS IOT mode is the boot button (GPIO0 T1 on ESP32 node32s).
This is temporary in order to allow those without external button and crumb board to test and will eventually be changed
to use GPIO2 T0 (pin 4).

See #define USE_BUTTON_BOOT in source code to change the behavior.

## Sensaurus BLE Configuration Client

Note: as of firmware version 2, settings of certificate and private key is not fully implemented in
Sensaurus BLE Configuration Client.

To run the configuration client, load ble-config/index.html into Chrome browser
and follow these steps:

* make sure a hub nearby is in BLE mode (e.g. by pressing the settings button on ESP32)
* click on "Select Hub" in the browser page
* select the hub and click "Pair" button
* the fields will populate with current values from the hub
* modify the fields to reflect the required new settings
* click on "Save" button. This will save settings to the hub, in hub's EEPROM and restart the hub

To repeat the settings procedure, switch the hub to BLE mode and click "Select Hub" or "Reconnect" button.

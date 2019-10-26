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
*   `[owner_id]/device/[device_id]` (JSON dictionary with current hub_id for this device)

## Installing ESP32 software

1.  Install Arduino IDE
2.  Go to Arduino IDE preferences; set `Additional Board Manager URLs` to `https://dl.espressif.com/dl/package_esp32_index.json`
3.  Go to `Tools` / `Board` / `Board Manager...`, search for `esp32`, select result and click `Install`
4.  Selection version 1.0.1 of the ESP32 core/board files (in the board manager).

## Compiling the code

1.  Go to Arduino IDE preferences and set the `Sketchbook location` to the `sensaurus` folder. This allows the IDE to find the required libraries.
2.  Restart the Arduino IDE.
3.  Copy `sample_settings.h` to `settings.h` and edit it as needed. You'll need MQTT server information and WiFi network information.
4.  If you are enabling AWS MQTT and BLE at the same time, you'll need to change the partitions for the program to fit on the ESP32. 
    Instructions for this are at the end of this document.
5.  Select `DOIT ESP32 DEVKIT V1` from board list (or other board type if needed).
6.  Press Ctrl-R (or Command-R) to compile. It may take a little while given the various library dependencies.

## Uploading to ESP32

1.  Plug in the ESP32 board.
2.  Select `DOIT ESP32 DEVKIT V1` from board list (or other board type if needed).
3.  Select serial port.
4.  Press Ctrl-U (or Command-U) to upload.
5.  After IDE shows `Connecting...`, push `BOOT` button on ESP32 for several seconds (until upload starts).

## Status LEDs

If startup fails, the blue LED will blink along with one of the yellow LEDs:

*   Blinking yellow 1 and blue: unable to connect to WiFi network (check WiFi name and password).
*   Blinking yellow 2 and blue: network time update failed (check that WiFi network has internet access).
*   Blinking yellow 3 and blue: unable to connect to MQTT server (check that server name is correct).
*   Blinking yellow 4 and blue: unable to subscribe to MQTT channels (check that hub ID is correct and server is configured).

After startup a succesful startup, the blue LED will be lit (with a medium brightness) and the yellow LEDs will indicate 
which plugs have devices connected. The yellow LED is turned on when meta-data is received from a device.
It is turned off if the device fails to respond to two consecutive polling messages (e.g. when it is unplugged).

During normal operation, the small blue LED on the ESP32 will blink make a brief blink each time an MQTT message is successfully published.

If the hub loses the WiFi connection during operation, the large blue LED will blink until the WiFi connection is restored. (It will attempt
to reconnect every 15 seconds.)

## Troubleshooting

If you have trouble compiling:

*   Make sure you have selected the correct board (`DOIT ESP32 DEVKIT V1`)
*   Make sure you have selected the correct version of the ESP32 core/board libraries (1.0.1).
*   Make sure you are using a recent Arduino IDE (we use 1.8.10).
*   Make sure you have prepared the `settings.h` file as described above.

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

## Hub Operation Modes, Settings via BLE

Sensaurus allows configuration via Bluetooth Low Energy (BLE). But in order to do that we have to overcome memory
limitation of ESP32 where both BLE and AWS IOT doesn't fit into memory. Therefore the hub may operate at any
time in one of two mode: BLE mode and AWS IOT mode.
When in BLE mode, AWS IOT will be disabled and switching modes will require a reboot.

When in BLE mode, settings can be changed using Web based Sensaurus BLE Configuration Client in ble-config directory.

When the hub starts up, it will normally start in AWS IOT mode.
If during the start up, hub can't connect to WIFI it will start in BLE mode.

There are 4 ways to switch between BLE mode and AWS IOT mode:

* when hub is started and in operation, send command "bleStart" (to switch to BLE mode) or "bleExit" (to exit BLE mode and switch to AWS IOT mode)
  via BLE command attribute or via AWS IOT/MQTT command
* press the restart button on the hub and keep pressing the configuration button, starting about a second after pressing reset
  and keep pressing for a few seconds. The configuration button press will be detected right after wifi is connected.
* when hub is started and in operation, in either mode, press settings/configuration button on the hub (this will reboot and switch to BLE mode)
* press the restart button on the hub (this will reboot and switch to AWS IOT mode)

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
* click on "Save" button. This will send settings via BLE to the hub, the hub will save them in EEPROM and restart the hub in AWS IOT mode

Note: if settings haven't changed, the hub will not save anything but will restart the hub in AWS IOT mode
To repeat the settings procedure, switch the hub to BLE mode and click "Select Hub" or "Reconnect" button.

## Testing OTA

As of July, 2019 OTA is implemented without HTTP support, via Arduino IDE, as described here:

https://diyprojects.io/arduinoota-esp32-wi-fi-ota-wireless-update-arduino-ide/
https://lastminuteengineers.com/esp8266-ota-updates-arduino-ide/

To test OTA:

* make sure a recent version with OTA support has been burned via serial port - that allows
  the unit to accept OTA updates
* in Arduino IDE, switch port to a network port corresponding to the unit IP
  If you don't see the network port, try restarting Adruino IDE
* perform update via IDE - you'll see progress "..." as the firmware is updated
* you can verify using test_remote.py tool that a status has been received from the hub with a recent
  built timestamp, e.g. like this:

  {'wifi_network': 'Fatra', 'version': 2, 'built': 'Tue Jul 16 09:46:03 2019', 'host': 'a1zmu4vkfpn2wo-ats.iot.us-west-2.amazonaws.com'}

Note: the hub will accept OTA updates in both BLE mode (as long as WIFI is connected) and AWS IOT mode.
If you don't see network port for your hub, make sure your development machine is on the same network or WIFI, and also
see known problems/troubleshooting tips for Arduino IDE here:
https://forum.arduino.cc/index.php?topic=575560.0

## Hardware Information

We use the follow ESP32 pins:

*   configuration button: 4
*   status LED: 5
*   device connection LEDs: 16, 17, 18, 19, 21, 22
*   device serial data: 23, 25, 26, 27, 32, 33

We communicate with the devices using 38400 baud half-duplex serial. The hub polls each device and the device has 50 ms to reply. 
(This allows about 190 bytes of reply data.)

## Uploading Device Code

To upload code to one of the prototype devices, you'll need an FTDI board 
(e.g. [https://www.sparkfun.com/products/9716](https://www.sparkfun.com/products/9716)).

1.  Install FTDI drivers if needed. 
2.  Connect the FTDI device to the Arduino Pro Mini with the correct orientation (possibly upside down). GND should connect to GND.
3.  In the Arduino IDE, select `Arduino Pro or Pro Mini` from the board menu.
4.  Select the serial port that shows up after connecting the FTDI board to your computer.
5.  Use the upload function in the Arduino IDE.

## Building with Changed Partitions

In order to allow BLE to fit into sketch/firmware, the default partition table had to be modified.
As of July 2019, for experimental reasons, we allow compiling without BLE, with default partition table,
but that experimental feature will be disabled in the future.
To disable BLE compilation, uncomment #define ENABLE_BLE line in the main source file.

To build with changed partitions and be able to test BLE mode:

* create a new board definition in boards.txt that looks something like the one below
 (this assumes esp32 core source resides in sensaurus/hardware/expressive)

* create a new partition description file max.csv and place it in espressif/esp32/tools/partitions:

  # Name,   Type, SubType, Offset,  Size, Flags
  nvs,      data, nvs,     0x9000,  0x5000,
  otadata,  data, ota,     0xe000,  0x2000,
  app0,     app,  ota_0,   0x10000, 0x1D0000,
  app1,     app,  ota_1,   0x1E0000,0x1D0000,
  spiffs,   data, spiffs,  0x3B0000,0x50000,

 * restart Arduino IDE and select the new board
 * build and test

---
### Board definition for  node32smax: Node32s Dev Module (Max Memory)

Note: maximum_size=1900544 corresponds to app0/app1 size 0x1D0000.

  node32smax.name=Node32s Dev Module (Max Memory)

  node32smax.upload.tool=esptool_py
  node32smax.upload.maximum_size=1900544
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

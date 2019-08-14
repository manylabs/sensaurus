/**

SensaurusBle

Manylabs Sensaurus Web Bluetooth (BLE) functionality.

SensaurusBle class encapsulates client access to Sensaurus service and
is based on some code from googlechrome and webbluetoothcg examples:

  https://googlechrome.github.io/samples/web-bluetooth/index.html
and
  https://webbluetoothcg.github.io/demos
  https://webbluetoothcg.github.io/demos/heart-rate-sensor/

*/

// core service
var BLE_SERVICE_UUID   = "9ec18803-e34a-4882-b61d-864247da821d";

// characteristics for core service
var WIFI_NETWORK_UUID  = "e2ccd120-412f-4a99-922b-aca100637e2a";
var WIFI_PASSWORD_UUID = "30db3cd0-8eb1-41ff-b56a-a2a818873c34";
var OWNER_ID_UUID      = "af74141f-3c60-425a-9402-62ec79b58c1a";
var HUB_ID_UUID        = "e4636699-367b-4838-a421-1904cf95f869";
var CONSOLE_ENABLED_UUID = "ead80ccd-47b6-406d-99f2-a67ec2783858";
var BLE_CMD_UUID       = "93311ce4-a1e4-11e9-a3dc-60f81dcdd3b6";

// mqtt service
var BLE_SERVICE_MQTT_UUID   = "9ec18803-e34b-4882-b61d-864247da821d";

// characteristics for mqtt service
var MQTT_USER_UUID     = "63f04721-b6b4-11e9-99fb-60f81dcdd3b6";
var MQTT_PASSWORD_UUID = "6617bdbd-b6b4-11e9-b75b-60f81dcdd3b6";
var MQTT_SERVER_UUID   = "6675cf75-b6b4-11e9-82af-60f81dcdd3b6";
var MQTT_PORT_UUID     = "66ebed11-b6b4-11e9-b607-60f81dcdd3b6";
var HUB_CERT_UUID      = "d1c4d088-fd9c-4881-8fc2-656441fa2cf4";
var HUB_KEY_UUID       = "f97fee16-f4c3-48ff-a315-38dc2b985770";

async function writeCharacteristicsInChunks(charUuid, value) {
  //let p0 = new Promise((resolve, reject) => {

    let promise = sensaurusBle._writeCharacteristicValue(charUuid, sensaurusBle._encodeString("clear"));
    let result = await promise;
    var index = 0;
    while(index < value.length) {
      var part = value.slice(index, index+100);
      //console.log("f _writeCharacteristicValue(...: part=", part);
      let result = await sensaurusBle._writeCharacteristicValue(charUuid, sensaurusBle._encodeString(part))
      // sometimes, next chunk doesn't get written, delay a bit
      //let promise = new Promise((resolve, reject) => {
      //  setTimeout(() => resolve("done!"), 10)
      //});
      //await promise;
      index += 100;
    }
    //resolve();
  //});
  //return p0;
}

async function writeChunkedCharacteristics() {
  // only write key/cert characteristics if values are non-empty
  if (sensaurusBle.thingCrt.length > 0) {
    await writeCharacteristicsInChunks(HUB_CERT_UUID, sensaurusBle.thingCrt);
  }
  if (sensaurusBle.thingPrivateKey.length > 0) {
    await writeCharacteristicsInChunks(HUB_KEY_UUID, sensaurusBle.thingPrivateKey);
  }
}
//*******************************************************************
// Utility functions
//*******************************************************************

// Converts int16 to array of 2 bytes, in Big Endian order
function toBytesInt16 (num) {
    arr = new ArrayBuffer(2); // an Int16 takes 2 bytes
    view = new DataView(arr);
    view.setUint16(0, num, false); // byteOffset = 0; litteEndian = false
    return arr;
}

(function() {
  'use strict';

  // used for decoding in helper functions
  let encoder = new TextEncoder('utf-8');
  let decoder = new TextDecoder('utf-8');

  // see hub-esp32 C code
  // Sensaurus

  // #define BLE_SERVICE_UUID "9ec18803-e34a-4882-b61d-864247da821d"
  // #define WIFI_NETWORK_UUID "e2ccd120-412f-4a99-922b-aca100637e2a"
  // ...

  // set to false to minimize console.log printing
  var verbose = true;

  //   optionalServices: ['ba42561b-b1d2-440a-8d04-0cefb43faece']
  function log(text) {
    if (verbose) {
      console.log(text);
    }
  }

  function _disconnectListener() {
    log('Device disconnected');
    // connect();
    sensaurusBle._connected = false;
    disconnectHandler();
  }

  /**
   * BLE client class for Sensaurus services.
  */
  class SensaurusBle {


    constructor() {

      this.device = null;
      this.server = null;
      // holds map for cached characteristics
      this._characteristics = new Map();
      this._connected = false;
    }



    // finalize connection by setting object properties etc.
    _finalizeConnection(server) {
      //this._characteristics = new Map();
      this._connected = true;
      log("_finalizeConnection: Calling getPrimaryService...")
      this.server = server;
      return this.getPrimaryService(server);
    }



    // gets services needed by this clients and caches characteristics
    getPrimaryService(server) {
      return Promise.all([
        server.getPrimaryService(BLE_SERVICE_UUID).then(service => {
          //console.log("deb3a", service);
          return Promise.all([
            this._cacheCharacteristic(service, WIFI_NETWORK_UUID),
            this._cacheCharacteristic(service, WIFI_PASSWORD_UUID),
            this._cacheCharacteristic(service, OWNER_ID_UUID),
            this._cacheCharacteristic(service, HUB_ID_UUID),
            this._cacheCharacteristic(service, CONSOLE_ENABLED_UUID),
            this._cacheCharacteristic(service, BLE_CMD_UUID),
          ])
        }),
        server.getPrimaryService(BLE_SERVICE_MQTT_UUID).then(service => {
          //console.log("deb3a", service);
          return Promise.all([
            this._cacheCharacteristic(service, MQTT_USER_UUID),
            this._cacheCharacteristic(service, MQTT_PASSWORD_UUID),
            this._cacheCharacteristic(service, MQTT_SERVER_UUID),
            this._cacheCharacteristic(service, MQTT_PORT_UUID),
            this._cacheCharacteristic(service, HUB_CERT_UUID),
            this._cacheCharacteristic(service, HUB_KEY_UUID),
          ])
        }),

      ]);
    }

    //*******************************************************************
    // Manylabs Sensaurus BLE Service high level access functions
    //*******************************************************************

    // Performs overall connect, including requestDevice, getPrimaryService
    connect(disconnectListener) {
      //return navigator.bluetooth.requestDevice({filters:[{services:[ BLE_SERVICE_UUID, BLE_SERVICE_MQTT_UUID ]}]})
      return navigator.bluetooth.requestDevice({filters:[{services:[ BLE_SERVICE_UUID ]}], optionalServices:[BLE_SERVICE_MQTT_UUID]})
      .then(device => {
        // remove event listener if this instance was connected before
        if (this.device) {
          this.device.removeEventListener('gattserverdisconnected', _disconnectListener, true);
        }
        //console.log("connect: device=", device);
        this.device = device;
        this.device.addEventListener('gattserverdisconnected', _disconnectListener);
        return device.gatt.connect();
      })
      .then(server => {
        this._connected = true;
        this.server = server;
        log("connect: Calling getPrimaryService...")
        return this.getPrimaryService(server);
      })
    }

    // Reconnect a previously connected/disconnected device, without
    //  the need to perform requestDevice.
    reconnect() {
      if (!this.device) {
        log("Can't reconnect device that was not previosly connected.");
        return;
      }
      if (this._connected) {
        log("Can't reconnect device that is already connected.");
        return;
      }
      return this.device.gatt.connect()
      .then( server => this._finalizeConnection(server));
    }

    // Get all read characteristics
    getAllCharacteristicValues() {
      console.log("getAllCharacteristicValues");
      /* this works on Chrome MACOSX, not on Chrome Android  */
      const p = new Promise((resolve, reject) => {
        this._readCharacteristicValue(WIFI_NETWORK_UUID)
        .then( value => {
          this.wifiNetwork = decoder.decode(value);
          //console.log("getAllCharacteristicValues.1", this.wifiNetwork);
          this._readCharacteristicValue(WIFI_PASSWORD_UUID)
          .then( value => {
            this.wifiPassword = decoder.decode(value);
            //console.log("getAllCharacteristicValues.2", this.wifiPassword);
            this._readCharacteristicValue(OWNER_ID_UUID)
            .then( value => {
              this.ownerId = decoder.decode(value);
              //console.log("getAllCharacteristicValues.3", this.ownerId);
              this._readCharacteristicValue(HUB_ID_UUID)
              .then( value => {
                this.hubId = decoder.decode(value);
                this._readCharacteristicValue(CONSOLE_ENABLED_UUID)
                .then( value => {
                  // unlike on the server where consoleEnabled is bool,
                  //   on the client it's integer 0 or 1
                  var ival = new Int16Array(value.buffer)[0];
                  this.consoleEnabled = ival;
                  this._readCharacteristicValue(MQTT_USER_UUID)
                  .then(value => {
                    this.mqttUser = decoder.decode(value);
                    this._readCharacteristicValue(MQTT_PASSWORD_UUID)
                    .then(value => {
                      this.mqttPassword = decoder.decode(value);
                      this._readCharacteristicValue(MQTT_SERVER_UUID)
                      .then(value => {
                        this.mqttServer = decoder.decode(value);
                        //resolve("done");
                        this._readCharacteristicValue(MQTT_PORT_UUID)
                        .then(value => {

                          // value.getUint16(): 17715
                          //   vs. Int16Array(value.buffer)[0] 13125
                          //this.mqttPort = value.getUint16();
                          // Int16Array works as a work-around and returns the correct value
                          // it's because of the endian:
                          //    getUint16 is using Big Endian
                          //    Int16Array is using Little Endian
                          // 51*256+69
                          // = 13125
                          // 51+69*256
                          // = 17715

                          this.mqttPort = new Int16Array(value.buffer)[0];
                          resolve("done");
                        });
                      });
                    });
                  });
                });
              });
            });
          });
        });
      });
      return p;


      // this parallel call works on Chrome MACOSX, not on Chrome Android
      // therefore using the serialized code above.
      /*
      return Promise.all([
        this._readCharacteristicValue(WIFI_NETWORK_UUID),
        this._readCharacteristicValue(WIFI_PASSWORD_UUID),
        this._readCharacteristicValue(OWNER_ID_UUID),
        this._readCharacteristicValue(HUB_ID_UUID),
      ])
      .then(values => {
        //console.log("values=", values);
        this.wifiNetwork = decoder.decode(values[0]);
        this.wifiPassword = decoder.decode(values[1]);
        this.ownerId = decoder.decode(values[2]);
        this.hubId = decoder.decode(values[3]);
        var pw = '*'.repeat(this.wifiPassword.length);
        console.log("getAllCharacteristicValues values=", this.wifiNetwork, pw, this.ownerId, this.hubId);
      });
      */

    }

    // Save all characteristics
    saveSettings() {
      var pw = '*'.repeat(this.wifiPassword.length);
      console.log("saveSettings values=", this.wifiNetwork, pw, this.ownerId, this.hubId);
      // promise to save core service characteristics
      const p0 = new Promise((resolve, reject) => {
        this._writeCharacteristicValue(WIFI_NETWORK_UUID, this._encodeString(this.wifiNetwork))
        .then(() => {
          this._writeCharacteristicValue(WIFI_PASSWORD_UUID, this._encodeString(this.wifiPassword))
          .then(() => {
            this._writeCharacteristicValue(OWNER_ID_UUID, this._encodeString(this.ownerId))
            .then(() => {
              this._writeCharacteristicValue(HUB_ID_UUID, this._encodeString(this.hubId))
              .then(() => {
                // boolean value encoded as int16
                var consoleEnabled = toBytesInt16(this.consoleEnabled);
                this._writeCharacteristicValue(CONSOLE_ENABLED_UUID, consoleEnabled)
                .then(() => {
                  writeChunkedCharacteristics()
                  .then(() => {
                    console.log("saveSettings done writeChunkedCharacteristics");
                    resolve();
                  })
                  .catch(error => {
                    console.log("saveSettings failed saving core service characteristics: ", error);
                    reject(error);
                  });
                });
              });
            });
          });
        });
      });
      // promise to save mqtt service characteristics

      const pret = new Promise((resolve, reject) => {
        p0.then(() => {
          this._writeCharacteristicValue(MQTT_USER_UUID, this._encodeString(this.mqttUser))
          .then(() => {
            this._writeCharacteristicValue(MQTT_PASSWORD_UUID, this._encodeString(this.mqttPassword))
            .then(() => {
              this._writeCharacteristicValue(MQTT_SERVER_UUID, this._encodeString(this.mqttServer))
              .then(() => {
                var val = toBytesInt16(this.mqttPort);
                this._writeCharacteristicValue(MQTT_PORT_UUID, val)
                .then(() => {
                  writeChunkedCharacteristics()
                  .then(() => {
                    console.log("saveSettings done writeChunkedCharacteristics");
                    resolve();
                  });
                  //.catch(error => {
                  //  console.log("saveSettings failed: ", error);
                  //  reject(error);
                  //});
                });
              });
            });
          })
          .catch(error => {
            console.log("saveSettings failed saving mqtt service characteristics: ", error);
            reject(error);
          });
        });
      });

      return pret;
    }

    // Send command (write command characteristic)
    sendCmd(cmd) {
      return this._writeCharacteristicValue(BLE_CMD_UUID, this._encodeString(cmd));
    }

    //*******************************************************************
    // Web Bluetooth Access Helper Functions and encoding/decoding functions
    //*******************************************************************

    _encodeString(data) {
      return encoder.encode(data);
    }
    _cacheCharacteristic(service, characteristicUuid) {
      return service.getCharacteristic(characteristicUuid)
      .then(characteristic => {
        this._characteristics.set(characteristicUuid, characteristic);
        log("_cacheCharacteristic: " + characteristicUuid);
      });
    }
    _readCharacteristicValue(characteristicUuid) {
      let characteristic = this._characteristics.get(characteristicUuid);
      return characteristic.readValue()
      .then(value => {
        // In Chrome 50+, a DataView is returned instead of an ArrayBuffer.
        value = value.buffer ? value : new DataView(value);
        return value;
      });
    }
    _writeCharacteristicValue(characteristicUuid, value) {
      let characteristic = this._characteristics.get(characteristicUuid);
      //return characteristic.writeValue(value);
      var ret = characteristic.writeValue(value);
      //console.log("_writeCharacteristicValue: ret=", ret)
      return ret;
    }
    _startNotifications(characteristicUuid) {
      let characteristic = this._characteristics.get(characteristicUuid);
      // Returns characteristic to set up characteristicvaluechanged event
      // handlers in the resolved promise.
      console.log("_startNotifications: " + characteristicUuid + ":" + JSON.stringify(characteristic))
      return characteristic.startNotifications()
      .then(() => characteristic);
    }
    _stopNotifications(characteristicUuid) {
      let characteristic = this._characteristics.get(characteristicUuid);
      // Returns characteristic to remove characteristicvaluechanged event
      // handlers in the resolved promise.
      return characteristic.stopNotifications()
      .then(() => characteristic);
    }
  }

  // assign global variable for access from app.js
  window.sensaurusBle = new SensaurusBle();

})();

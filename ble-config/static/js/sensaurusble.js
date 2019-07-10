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

var BLE_SERVICE_UUID = '9ec18803-e34a-4882-b61d-864247da821d';
var WIFI_NETWORK_UUID  = "e2ccd120-412f-4a99-922b-aca100637e2a";
var WIFI_PASSWORD_UUID  = "30db3cd0-8eb1-41ff-b56a-a2a818873c34";
var OWNER_ID_UUID  = "af74141f-3c60-425a-9402-62ec79b58c1a";
var HUB_ID_UUID  = "e4636699-367b-4838-a421-1904cf95f869";
var HUB_CERT_UUID  = "d1c4d088-fd9c-4881-8fc2-656441fa2cf4";
var HUB_KEY_UUID  = "f97fee16-f4c3-48ff-a315-38dc2b985770";
var BLE_CMD_UUID  = "93311ce4-a1e4-11e9-a3dc-60f81dcdd3b6";

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


(function() {
  'use strict';

  // used for decoding in helper functions
  let encoder = new TextEncoder('utf-8');
  let decoder = new TextDecoder('utf-8');

  // heart rate service: 180d
  // 0000180d-0000-1000-8000-00805f9b34fb

  // HR
  // https://www.bluetooth.com/specifications/gatt/services/
  // Heart Rate	org.bluetooth.service.heart_rate	0x180D

  // see hub-esp32
  // Sensaurus

  // #define BLE_SERVICE_UUID "9ec18803-e34a-4882-b61d-864247da821d"
  // #define WIFI_NETWORK_UUID "e2ccd120-412f-4a99-922b-aca100637e2a"
  // #define WIFI_PASSWORD_UUID "30db3cd0-8eb1-41ff-b56a-a2a818873c34"
  // #define OWNER_ID_UUID "af74141f-3c60-425a-9402-62ec79b58c1a"
  // #define HUB_ID_UUID "e4636699-367b-4838-a421-1904cf95f869"
  // #define HUB_CERT_UUID "d1c4d088-fd9c-4881-8fc2-656441fa2cf4"
  // #define HUB_KEY_UUID "f97fee16-f4c3-48ff-a315-38dc2b985770"

  //var HPS_SERVICE_UUID = '9ec18803-e34a-4882-b61d-864247da821d';

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
   *
  */
  class SensaurusBle {


    constructor() {

      this.device = null;
      this.server = null;
      // holds map for cached characteristics
      this._characteristics = new Map();
      this._connected = false;
    }

    // TODO: finish reconnect implementation
    // reconnect a previously connected/disconnected device
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
      //log("Reconnect: ret=", ret);
      //return ret;
    }

    _finalizeConnection(server) {
      this._connected = true;
      log("_finalizeConnection: Calling getPrimaryService...")
      this.server = server;
      console.log("deb3", server);
      return Promise.all([
        server.getPrimaryService(BLE_SERVICE_UUID).then(service => {
          //console.log("deb3a", service);
          return Promise.all([
            this._cacheCharacteristic(service, WIFI_NETWORK_UUID),
            this._cacheCharacteristic(service, WIFI_PASSWORD_UUID),
            this._cacheCharacteristic(service, OWNER_ID_UUID),
            this._cacheCharacteristic(service, HUB_ID_UUID),
            this._cacheCharacteristic(service, BLE_CMD_UUID),
          ])
        })
      ]);
    }


    connect(disconnectListener) {
      return navigator.bluetooth.requestDevice({filters:[{services:[ BLE_SERVICE_UUID ]}]})
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
        log("Calling getPrimaryService...")
        this.server = server;
        //console.log("getPrimaryService: ", server);
        return Promise.all([
          server.getPrimaryService(BLE_SERVICE_UUID).then(service => {
            //console.log("deb3a", service);
            return Promise.all([
              this._cacheCharacteristic(service, WIFI_NETWORK_UUID),
              this._cacheCharacteristic(service, WIFI_PASSWORD_UUID),
              this._cacheCharacteristic(service, OWNER_ID_UUID),
              this._cacheCharacteristic(service, HUB_ID_UUID),
              this._cacheCharacteristic(service, HUB_CERT_UUID),
              this._cacheCharacteristic(service, HUB_KEY_UUID),
              this._cacheCharacteristic(service, BLE_CMD_UUID),
            ])
          })
        ]);
      })
    }

    //*******************************************************************
    // Manylabs Sensaurus BLE Service high level access functions
    //*******************************************************************

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
                //console.log("getAllCharacteristicValues.4", this.hubId);
                resolve("done");
              });
            });
          });
        });
      });
      return p;


      /* this works on Chrome MACOSX, not on Chrome Android  */
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

      /*
      return this._readCharacteristicValue(WIFI_NETWORK_UUID)
      .then(value => {
        //var value = value.buffer ? value : new DataView(value);
        var decoded = decoder.decode(value);
        log("getAllCharacteristicValues: " + decoded);
      });
      */

    }

    // Save all characteristics
    saveSettings() {
      var pw = '*'.repeat(this.wifiPassword.length);
      console.log("saveSettings values=", this.wifiNetwork, pw, this.ownerId, this.hubId);
      const p0 = new Promise((resolve, reject) => {
        var p = this._writeCharacteristicValue(WIFI_NETWORK_UUID, this._encodeString(this.wifiNetwork));
        p.then(() => {
          p = this._writeCharacteristicValue(WIFI_PASSWORD_UUID, this._encodeString(this.wifiPassword));
          p.then(() => {
            p = this._writeCharacteristicValue(OWNER_ID_UUID, this._encodeString(this.ownerId));
            p.then(() => {
              this._writeCharacteristicValue(HUB_ID_UUID, this._encodeString(this.hubId))
              .then(() => {
                writeChunkedCharacteristics()
                .then(() => {
                  console.log("saveSettings done writeChunkedCharacteristics");
                  resolve();
                })
                .catch(error => {
                  console.log("saveSettings failed: ", error);
                  reject(error);
                });
              });
            });
          });
        });
      });
      return p0;
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

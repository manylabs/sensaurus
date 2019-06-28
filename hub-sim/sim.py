import paho.mqtt.client as paho
import ssl
import time
import json
import random
import hjson


class Hub(object):

    def __init__(self, config):
        self.mqttc = None
        self.connected = False
        self.devices = []
        self.owner_id = config['owner_id']
        self.config = config
        self.id = config['hub_id']
        self.send_interval = None
        self.status_sent = False

    # connect to the MQTT service (broker)
    def connect(self):
        self.mqttc = paho.Client()
        self.mqttc.on_connect = self.on_connect
        self.mqttc.on_message = self.on_message
        self.mqttc.tls_set(self.config['ca_path'], certfile=self.config['cert_path'], keyfile=self.config['key_path'], 
                      cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2, ciphers=None)
        self.mqttc.connect(self.config['host'], 8883, keepalive=60)
        self.mqttc.loop_start()
        self.mqttc.subscribe('%s/hub/%s/config' % (self.owner_id, self.id))
        self.mqttc.subscribe('%s/hub/%s/actuators' % (self.owner_id, self.id))

    def on_connect(self, client, userdata, flags, rc):
        self.connected = True
        print('connected (result: %s)' % rc)

    # handle incoming messages on subscribed topics
    def on_message(self, client, userdata, msg):
        if msg.topic.endswith('config'):
            message = json.loads(msg.payload)
            if 'send_interval' in message:
                self.send_interval = message['send_interval']
                print('send interval: %.2f seconds' % self.send_interval)
            if 'firmware_url' in message:
                print('firmware update: %s' % message['firmware_url'])
        elif msg.topic.endswith('actuators'):
            message = json.loads(msg.payload)
            for (k, v) in message.items():
                print('setting actuator %s to %s' % (k, v))

    def add_device(self, device):
        self.devices.append(device)
        if self.connected:
            self.send_device_info()

    # send info to status topic for this hub
    def send_status(self):
        message = {
            'wifi_network': 'abc',
            'wifi_password': '123',
            'host': self.config['host'],
        }
        topic_name = '%s/hub/%s/status' % (self.owner_id, self.id)
        self.mqttc.publish(topic_name, json.dumps(message))
        self.status_sent = True

    # send info about devices currently connected to this hub
    def send_device_info(self):
        device_infos = {}
        for d in self.devices:
            device_info = {
                'version': 1,
                'components': d.components,  # components is a list of dictionaries
            }
            device_infos[d.id] = device_info
            topic_name = '%s/device/%s' % (self.owner_id, d.id)
            self.mqttc.publish(topic_name, self.id, qos=1)  # send hub ID for this device
        topic_name = '%s/hub/%s/devices' % (self.owner_id, self.id)
        self.mqttc.publish(topic_name, json.dumps(device_infos), qos=1)  # send list of device info dictionaries

    # send sensor values from devices connected to this hub
    def send_sensor_values(self):
        values = {}
        values['time'] = int(time.time())
        for d in self.devices:
            for c in d.components:
                if c['dir'] == 'i':
                    value = random.uniform(10.0, 20.0)
                    values[c['id']] = '%.2f' % value
        topic_name = '%s/hub/%s/sensors' % (self.owner_id, self.id)
        self.mqttc.publish(topic_name, json.dumps(values), qos=1)
        print('sending %d sensor values to %s' % (len(values) - 1, topic_name))

    # send simulated data
    def run(self):
        while True:
            if self.send_interval:
                time.sleep(self.send_interval)  # not quite going to match send interval, but that's ok for this simulation
            else:
                time.sleep(0.5)
            if self.connected:
                if not self.status_sent:
                    self.send_status()
                    self.send_device_info()
                if self.send_interval:
                    self.send_sensor_values()
            else:
                print('waiting for connection...')


class Device(object):
    def __init__(self, id, components):  # components should be a list of dictionaries
        self.id = id
        self.components = components
        for c in self.components:
            c['id'] = '%x-%s' % (self.id, c['type'][:5])  # construct a component ID using device ID and component type
            print('loaded component %s' % c['id'])


# create hub and devices
config = hjson.loads(open('config.hjson').read())
hub = Hub(config)
for device_info in config['devices']:
    device = Device(device_info['id'], device_info['components'])
    hub.add_device(device)
if 'send_interval' in config:
    hub.send_interval = int(config['send_interval'])


# connect to the MQTT service (broker)
hub.connect()


# start generating sensor data
hub.run()

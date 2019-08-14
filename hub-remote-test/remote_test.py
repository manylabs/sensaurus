#!/usr/bin/env python
import paho.mqtt.client as paho
import ssl
import os
import time
import json
import hjson
import random
import threading


class Tester(object):

    def __init__(self, config):
        self.mqttc = None
        self.connected = False
        self.config = config
        self.config_sent = False
        self.log_file = open('sensor-data.csv', 'a')
        self.test_component_ids = []

    # connect to the MQTT service (broker)
    def connect(self):
        self.mqttc = paho.Client("remote_test-%d" % os.getpid())
        self.mqttc.on_connect = self.on_connect
        self.mqttc.on_message = self.on_message
        if not self.config.get('use_mqtt'):
            # use aws iot
            self.mqttc.tls_set(self.config['ca_path'], certfile=self.config['cert_path'], keyfile=self.config['key_path'],
                          cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2, ciphers=None)
            self.mqttc.connect(self.config['host'], 8883, keepalive=60)
        else:
            # use plain mqtt
            mqtt_user = self.config['mqtt_user']
            mqtt_password = self.config['mqtt_password']
            self.mqttc.username_pw_set(mqtt_user, mqtt_password)
            self.mqttc.connect(self.config['mqtt_server'], int(self.config['mqtt_port']), keepalive=60)

        self.mqttc.loop_start()
        self.mqttc.subscribe('%s/hub/%s/status' % (self.config['owner_id'], self.config['hub_id']))
        self.mqttc.subscribe('%s/hub/%s/sensors' % (self.config['owner_id'], self.config['hub_id']))
        self.mqttc.subscribe('%s/hub/%s/devices' % (self.config['owner_id'], self.config['hub_id']))

    def on_connect(self, client, userdata, flags, rc):
        self.connected = True
        print('connected (result: %s)' % rc)

    # handle incoming messages on subscribed topics
    def on_message(self, client, userdata, msg):
        if msg.topic.endswith('status'):
            message = json.loads(msg.payload)
            if not 'wifi_network' in message:
                print('**** wifi_network missing from status')
            if not 'host' in message:
                print('**** host missing from status')
            print("%s status:" % (time.asctime()))
            print(message)
        elif msg.topic.endswith('sensors'):
            message = json.loads(msg.payload)
            message_time = float(message['time'])
            if abs(message_time - time.time()) > 10:
                print('**** bad message time')
            parts = [str(message_time)]
            for (k, v) in message.items():
                if k != 'time':
                    parts.append(v)
            self.log_file.write(','.join(parts) + '\n')
            print('%s: received %d sensor values with timestamp %d' % \
                (time.asctime(), len(parts) - 1, message_time))
        elif msg.topic.endswith('devices'):
            message = json.loads(msg.payload)
            print("%s devices:" % (time.asctime()))
            self.test_component_ids = []
            for device_id, device_info in message.items():
                comp_types = []
                for comp_info in device_info['components']:
                    comp_type = comp_info['type']
                    comp_types.append(comp_type)
                    if comp_type.startswith('out') and comp_type.endswith('test'):  # check for test components
                        comp_id = device_id + '-' + comp_info['type'][:5]
                        print('        found test component: %s' % comp_id)
                        self.test_component_ids.append(comp_id)
                print('    id: %s, ver: %d, plug: %d, comps: %d (%s)' % (device_id, device_info['version'], device_info['plug'], len(comp_types), ', '.join(comp_types)))

    # send a command message to the hub's command topic; args can be a dictionary of additional command arguments
    def send_command(self, command, args=None):
        message = args or {}
        message['command'] = command
        topic_name = '%s/hub/%s/command' % (self.config['owner_id'], self.config['hub_id'])
        print('sending command %s to %s' % (command, topic_name))
        self.mqttc.publish(topic_name, json.dumps(message))

    # send a command message to the hub's command topic; args can be a dictionary of additional command arguments
    def set_actuators(self, actuator_values):
        topic_name = '%s/hub/%s/actuators' % (self.config['owner_id'], self.config['hub_id'])
        print('sending %d actuator values to %s' % (len(actuator_values), topic_name))
        self.mqttc.publish(topic_name, json.dumps(actuator_values))

    def send_req_status(self):
        interval = self.config.get('req_status_interval')
        if interval:
            t = threading.Timer(interval, self.send_req_status)
            t.daemon = True
            t.start()
        if self.connected:
            self.send_command('req_status')

    # simulate a polling loop
    def run(self):

        while True:
            time.sleep(1.0)
            if self.connected:
                if not self.config_sent:
                    self.config_sent = True
                    send_interval = self.config.get('send_interval')
                    if not send_interval:
                        send_interval = 1.0
                    self.send_command('set_send_interval', {'send_interval': send_interval})
                    #self.send_command('req_status')
                    self.send_req_status()
                    self.send_command('req_devices')
                #if self.test_component_ids:
                if not self.config.get('no_actuator_send') and self.test_component_ids:
                    actuator_values = {comp_id: random.randint(0, 1) for comp_id in self.test_component_ids}
                    self.set_actuators(actuator_values)
            else:
                print('waiting for connection...')




# load config and connect to the MQTT service (broker)
config = hjson.loads(open('config.hjson').read())
tester = Tester(config)
tester.connect()


# request sending sensor data
tester.run()

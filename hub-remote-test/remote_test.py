import paho.mqtt.client as paho
import ssl
import time
import json
import hjson


class Tester(object):

    def __init__(self, config):
        self.mqttc = None
        self.connected = False
        self.config = config
        self.config_sent = False
        self.log_file = open('sensor-data.csv', 'a')

    # connect to the MQTT service (broker)
    def connect(self):
        self.mqttc = paho.Client()
        self.mqttc.on_connect = self.on_connect
        self.mqttc.on_message = self.on_message
        self.mqttc.tls_set(self.config['ca_path'], certfile=self.config['cert_path'], keyfile=self.config['key_path'], 
                      cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2, ciphers=None)
        self.mqttc.connect(self.config['host'], 8883, keepalive=60)
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
            print('status:')
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
            print('received %d sensor values with timestamp %d' % (len(parts) - 1, message_time))
        elif msg.topic.endswith('devices'):
            message = json.loads(msg.payload)
            print('devices:')
            print(message)

    # send a command message to the hub's command topic; args can be a dictionary of additional command arguments
    def send_command(self, command, args=None):
        message = args or {}
        message['command'] = command
        topic_name = '%s/hub/%s/command' % (self.config['owner_id'], self.config['hub_id'])
        print('sending command %s to %s' % (command, topic_name))
        self.mqttc.publish(topic_name, json.dumps(message))

    # simulate a polling loop
    def run(self):
        while True:
            time.sleep(1.0)
            if self.connected:
                if not self.config_sent:
                    self.config_sent = True
                    self.send_command('set_send_interval', {'send_interval': 1.0})
                    self.send_command('req_status')
                    self.send_command('req_devices')
            else:
                print('waiting for connection...')


# load config and connect to the MQTT service (broker)
config = hjson.loads(open('config.hjson').read())
tester = Tester(config)
tester.connect()


# request sending sensor data
tester.run()

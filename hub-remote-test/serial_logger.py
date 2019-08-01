import serial
import hjson


config = hjson.loads(open('config.hjson').read())
serial_port = serial.Serial(config['serial_port'], baudrate=9600, timeout=0.05)
log_file = open('serial-log.txt', 'a')

while True:
    line = serial_port.readline()
    line = line.strip()
    if line:
        print(line)
        log_file.write(line + '\n')

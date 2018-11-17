#!/usr/bin/env python
import time
import math
import Adafruit_BBIO.GPIO as GPIO
import Adafruit_GPIO.I2C as I2C

import json
import datetime
import paho.mqtt.client as paho

I2C_ADDR_ADC121 = 0x50 # ADC121 has a default address of 0x50. Check with i2cdetect -r 2
I2C_BUS_ADC121 = 0x02 # The I2C grove connector is wired to I2C bus 2
# ADC constants, from spec sheet at https://raw.githubusercontent.com/SeeedDocument/Grove-I2C_ADC/master/res/ADC121C021_Datasheet.pdf
ADC_REG_ADDR_CONFIG = 0x02 # 
ADC_REG_ADDR_RESULT = 0x00

# Set up the ADC on the I2C bus
i2c = I2C.get_i2c_device(I2C_ADDR_ADC121, I2C_BUS_ADC121)
i2c.write8(ADC_REG_ADDR_CONFIG, 0x20)

def read_adc():
    data_list = i2c.readList(ADC_REG_ADDR_RESULT, 2)
    data = ((data_list[0] & 0x0f) << 8 | data_list[1]) & 0xff
    return data

def read_temperature(model = 'v1.2'):
    if model == 'v1.2':
        bValue = 4250
    elif model == 'v1.1':
        bValue = 4250
    else:
        bValue = 3975

    total_value = 0
    for index in range(20):
        sensor_value = read_adc()
        total_value += sensor_value
        time.sleep(0.05)
    average_value = float(total_value / 20)

    sensor_value_tmp = (float)(average_value / 4095 * 2.95 * 2 / 3.3 * 1023)
    resistance = (float)(1023 - sensor_value_tmp) * 10000 / sensor_value_tmp

    temperature = round((float)(1 / (math.log(resistance / 10000) / bValue + 1 /298.15) - 273.15), 2)
    return temperature

if __name__ == "__main__":
    with open('config.json') as configFile:
        config = json.load(configFile)["mqtt.py"]

    client = paho.Client("luxsit temperature client")
    client.username_pw_set(config['mqttUserName'], config['mqttPassword'])
    client.tls_set()
    client.connect(config["mqttServer"], config["mqttPort"], 60)
    client.loop_start()
    while True:

            temperature = read_temperature('v1.2')
            client.publish("generator/temperature", json.dumps({"temperature":temperature, "timestamp": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")}))
            print("temperatur %02f" %temperature) 
            time.sleep(60)



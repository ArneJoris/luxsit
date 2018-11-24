#!/usr/bin/env python
import time
import socket
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
        B = 4275
    elif model == 'v1.1':
        B = 4250
    else:
        B = 3975

    total_value = 0
    for index in range(20):
        sensor_value = read_adc()
        total_value += sensor_value
        time.sleep(0.10)
    average_value = float(total_value / 20)

    Vmeasured = 850 # Voltage measured with multi meter in mV
    ADCreadingWhenMeasured = 245 # ADC reading when voltage was measured
    ADCresolution = 2**12 # 12 bit ADC

    R0 = 100000 # 100k ohm at 25 Celcius
    T0 = 298.15 # Kelvin = 25 celcius
    Vref = 3300 # beaglebone voltage to the ADC in mV

    #fudgeFactor = ((ADCresolution - 1) * Vmeasured / Vref)
    fudgeFactor = 7
    voltage = (Vref * average_value * fudgeFactor ) / (ADCresolution - 1)
    resistance = R0 * (( Vref / voltage) - 1.0)
    print("value=%d, voltage=%02f mV, resistance = %02f ohm" %(average_value, voltage, resistance))

    # Steinhart-Hart NTC equation for Celcius
    temperature = round((float)(1.0 / (math.log(resistance / R0) / B + 1 / T0) - 273.15), 2)
    return temperature

if __name__ == "__main__":
    with open('config.json') as configFile:
        config = json.load(configFile)["mqtt.py"]

    client = paho.Client("luxsit temperature client")
    client.username_pw_set(config['mqttUserName'], config['mqttPassword'])
    client.tls_set()

    connected = False
    while True:
        if (not connected):
            try:
                client.connect(config["mqttServer"], config["mqttPort"], 60)
                client.loop_start()
                connected = True
            except socket.gaierror:
                connected = False

        temperature = read_temperature('v1.2')
        try:
            client.publish("generator/temperature", json.dumps({"temperature":temperature, "timestamp": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")}))
        except socket.gaierror:
            connected = False

        print("temperature %02f" %temperature) 
        time.sleep(60)




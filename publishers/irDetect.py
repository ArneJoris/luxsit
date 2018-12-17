#!/usr/bin/env python
import paho.mqtt.client as paho
import json
import Adafruit_BBIO.ADC as ADC
import time
import datetime

ADC.setup()


with open('/home/debian/luxsit/config.json') as configFile:
    config = json.load(configFile)["mqtt.py"]

client = paho.Client("Infrared alarm driveway")
client.username_pw_set(config["mqttUserName"], config["mqttPassword"])
client.tls_set()
client.connect(config["mqttServer"], config["mqttPort"], 60)

while True:
    motion = False
    for index in range(20):
        value = ADC.read("P9_40")
        voltage = value * 1.8
        if voltage > 1:
            motion = True
        time.sleep(0.50)

    message = { "motion": motion, "timestamp": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S") }
    client.publish("/driveway/motion", json.dumps(message))

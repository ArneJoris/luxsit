#!/usr/bin/env python
import paho.mqtt.client as paho
import json
import Adafruit_BBIO.ADC as ADC
import time
import datetime
import subprocess
import logging

logging.basicConfig(format='%(asctime)s %(message)s', level=logging.INFO)

def on_connect(client, userdata, flags, rc):
    if rc==0:
        client.connected=True
        logging.info("Connected with code %d" %rc)
    else:
        logging.warning("Bad connection, code %d" %rc)

def on_disconnect(client, userdata, rc):
    logging.info("Disconnected, reconnecting.")
    client.connected=False

ADC.setup()


with open('/home/debian/luxsit/config.json') as configFile:
    config = json.load(configFile)["mqtt.py"]

client = paho.Client("Infrared alarm driveway")
client.username_pw_set(config["mqttUserName"], config["mqttPassword"])
client.tls_set()
client.connected = False
client.on_connect = on_connect
client.on_disconnect = on_disconnect
client.connect(config["mqttServer"], config["mqttPort"], 60)
client.loop_start() # Starts a new thread, deals with re-connecting

while True:
    while not client.connected:
        logging.info("Waiting for connection from MQTT broker...")
        time.sleep(5)

    motion = False
    for index in range(20):
        value = ADC.read("P9_40")
        voltage = value * 1.8
        if voltage > 1:
            motion = True
        time.sleep(0.50)

    if motion:
        logging.info("Motion detected, triggering light")
        subprocess.Popen(["/bin/bash","/home/debian/bin/light.sh"])

    message = { "motion": motion, "timestamp": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S") }
    client.publish("driveway/motion", json.dumps(message))

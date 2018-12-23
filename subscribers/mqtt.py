#!/usr/bin/env python3
from twilio.rest import Client
import paho.mqtt.client as paho
import json


def on_message(client, userdata, message):
    #  message.payload: {"batteryVoltage":293,"motion":false,"signalStrength":-39,"timestamp":"2018-11-04 14:20:07"}\n'
    topic=message.topic
    payload = json.loads(message.payload.decode('utf-8'))
    if (payload["motion"]):
        sendSMS("Motion @ " + topic + " at " + payload["timestamp"])
    else:
        print("NO MOTION @ " + topic + " at " + payload["timestamp"])

def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))
    result, mid = client.subscribe("/driveway/motion", qos=1)
    print("subscribe to /drivewway/motion " + str(result))
    result, mid = client.subscribe("/greenhouse/motion", qos=1)
    print("subscribe to /greenhouse/motion " + str(result))

def on_disconnect(client, userdata, rc):
    print("Disconnect with result code " + str(rc))

def on_subscribe(client, userdata, mid, granted_qos):
    print("Subscribed: " + str(mid) + " " + str(granted_qos))

def sendSMS(message):
    client = Client(twilioAccountSid, twilioAuthToken)
    message = client.messages.create(from_ = twilioPhoneNumber,
                                 body = message,
                                 to = smsRecipient)

with open('/home/arne/luxsit/config.json') as configFile:
    config = json.load(configFile)["mqtt.py"]

smsRecipient = config["smsRecipient"]
twilioAccountSid = config["twilioAccountSid"]
twilioAuthToken = config["twilioAuthToken"]
twilioPhoneNumber = config["twilioPhoneNumber"]

client = paho.Client("luxsit SMS client")
client.on_connect = on_connect
client.on_disconnect = on_disconnect
client.on_message = on_message
client.on_subscribe = on_subscribe
client.username_pw_set(config["mqttUserName"], config["mqttPassword"])
client.tls_set()
client.connect(config["mqttServer"], config["mqttPort"], 60)

client.loop_forever()


import paho.mqtt.client as mqtt
import time

# TOPICS
BASE_TOPIC = 'uark/csce5013/embrugge'
# SONG_TOPIC = BASE_TOPIC + '/song'
# INFO_TOPIC = BASE_TOPIC + '/info'
# NOTES_TOPIC = BASE_TOPIC + '/notes'
REMINDER_TOPIC = BASE_TOPIC + '/reminder'

PORT = 1883

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))
    
# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    print(msg.topic + " " + str(msg.payload))

#Get an instance of the MQTT Client object
client = mqtt.Client()
#Set the function to run when the client connects
client.on_connect = on_connect
#Set the function to run when a message is received
client.on_message = on_message
#Connect to the broker.mqtt-dashboard.com server on port 1883
client.connect("broker.hivemq.com", PORT, 60)

# Subscribe to the topics
client.subscribe(REMINDER_TOPIC)

# Loop continuously until the user quits
while True:
    client.loop()
    time.sleep(1)

# Sample 17 Character Long Message
# Hello World! Byte 

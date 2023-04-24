import paho.mqtt.client as mqtt

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

# Loop continuously until the user quits
while True:
    # Prompt the user to enter a message
    message = input("Enter your message (or 'quit' to exit): ")
    
    # Check if the user wants to quit
    if message.lower() == "quit":
        break
    
    # Publish the message to the MQTT topic
    client.publish(REMINDER_TOPIC, message)

# Disconnect from the MQTT broker
client.disconnect()
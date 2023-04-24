/**
 * @file Assignment3.ino
 * @author Ethan Brugger
 * @brief This program is a simple reminder system that uses a piezo buzzer, LCD, and LEDs to remind the user of a task.
 * Over MQTT, a reminder will be sent to the device. The device will then display the reminder on the LCD and play a song alerting the user.
 * The MQTT message will be sent in the format `reminder:duration`, where `reminder` is the reminder message and `duration` is the duration of the reminder.
 * The duration is in seconds and will be split up into 3 sections.
 * The first will be a Green LED for the first 1/3 of the duration, the second will be a Yellow LED for the second 1/3 of the duration,
 *  and the last will be a Red LED for the last 1/3 of the duration.
 * At each LED change, the buzzer will play a song, the next LED will turn on, and the LCD will display the reminder message.
 * If the user presses the button, the reminder will be stopped and the buzzer, LEDs, and LCD will be turned off.
 *
 * @version 0.1
 * @date 2023-04-23
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <LiquidCrystal.h>
#include <WiFi101.h>
#include <PubSubClient.h>
#include <AbleButtons.h>

#include "credentials.h"

#define MQTT_TASK_PERIOD 2000
#define PIEZO_TASK_PERIOD 50
#define BUTTON_TASK_PERIOD 250
#define LCD_TASK_PERIOD 2000
#define LED_TASK_PERIOD 1000
#define REMINDER_TASK_PERIOD 100

#define LCD_TIME 0
#define PIEZO_TIME 0
#define DEV_FLAG 0

#define MQTT_REMINDER_TOPIC "uark/csce5013/embrugge/reminder"

#define PIEZO_PIN 5
#define BUTTON_PIN 4

#define RED_LED 1
#define YELLOW_LED 2
#define GREEN_LED 3

typedef enum
{
  A,
  B,
  C,
  D,
  E,
  F,
  G
} PITCH_T;

// typedef struct
// {
//   PITCH_T pitch;
//   int len;
// } NOTE_T;
typedef struct
{
  int freq;
  int len;
} NOTE_T;

const int tones[7] = {262, 294, 330, 349, 392, 440, 494};

// Green Alert
const int greenAlert[10] = {659, 250, 0, 50, 523, 250, 0, 50, 784, 250}; // E5, 250ms, pause 50ms, C5, 250ms, pause 50ms, G5, 250ms

// Yellow Alert
const int yellowAlert[18] = {659, 150, 0, 30, 523, 150, 0, 30, 784, 150, 0, 30, 659, 150, 0, 30, 523, 150}; // E5, 150ms, pause 30ms, C5, 150ms, pause 30ms, G5, 150ms, pause 30ms, E5, 150ms, pause 30ms, C5, 150ms

// Red Alert
const int redAlert[30] = {659, 100, 0, 20, 523, 100, 0, 20, 784, 100, 0, 20, 659, 100, 0, 20, 523, 100, 0, 20, 784, 100, 0, 20, 659, 100, 0, 20, 523, 100}; // E5, 100ms, pause 20ms, C5, 100ms, pause 20ms, G5, 100ms, pause 20ms, E5, 100ms, pause 20ms, C5, 100ms, pause 20ms, G5, 100ms, pause 20ms, E5, 100ms, pause 20ms, C5, 100ms

// Replace with your network credentials
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Replace with your MQTT broker address
const char *mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

// Initialize the WiFi client and MQTT client
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Initialize the AbleButton
using Button = AblePulldownClickerButton;
Button button(BUTTON_PIN);

// Initialize the LCD
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);

char buf[100] = {0};
char durationBuf[100] = {0};
char infoBuf[100] = {0};
int notesBuf[100] = {0};
NOTE_T notes[49] = {NOTE_T{0, 0}};
int songLength = 0;

int duration = 0;
int greenDuration = 0;
int yellowDuration = 0;
int redDuration = 0;

/**
 * Flags used to define if tasks are in operation
 */
int lcdFlag = 0;
int piezoFlag = 0;
int buttonFlag = 0;

int redLedFlag = 0;
int yellowLedFlag = 0;
int greenLedFlag = 0;

static int songPlaying = 0;

byte ledLoadingPattern = 0b001;
int leds[3] = {RED_LED, YELLOW_LED, GREEN_LED};

void setup()
{
  Serial.begin(115200);
  Serial.println("Connecting...");

  pinMode(PIEZO_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Set the LEDs to LOW
  digitalWrite(RED_LED, HIGH);
  digitalWrite(YELLOW_LED, HIGH);
  digitalWrite(GREEN_LED, HIGH);

  lcd.begin(16, 2);
  // you can now interact with the LCD, e.g.:
  lcd.print("No Reminder");

  button.begin();

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
    // Chase LEDs to indicate WiFi connection

    delay(500);
  }
  Serial.println("WiFi connected");

  // Connect to MQTT broker
  mqttClient.setServer(mqttServer, mqttPort);
  while (!mqttClient.connected())
  {
    if (mqttClient.connect("embrugge"))
    {
      Serial.println("Connected to MQTT broker");
    }
    else
    {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }

  // Chase LEDs to indicate WiFi connection
  for (int j = 0; j < 20; j++)
  {
    for (int i = 0; i < 3; i++)
    {
      digitalWrite(leds[i], ledLoadingPattern & (1 << i)); // set LED state based on bit in ledLoadingPattern
    }

    ledLoadingPattern = (ledLoadingPattern << 1) | (ledLoadingPattern >> 2); // shift bits to create LED chaser ledLoadingPattern
    delay(100);
  }

  // Subscribe to MQTT topics
  mqttClient.subscribe(MQTT_REMINDER_TOPIC);

  // Publish "START" message to MQTT_SONG_TOPIC topic
  // mqttClient.publish(MQTT_SONG_TOPIC, "START");

  mqttClient.setCallback(handleOnMQTTMessage);

  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
}

void handleOnMQTTMessage(char *topic, byte *payload, unsigned int length)
{
  if (length > 0)
  {
    // Convert payload to char array
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0'; // Add null terminator

    if (strcmp(topic, MQTT_REMINDER_TOPIC) == 0)
    {
      Serial.println("Received reminder");
      Serial.print("Message: ");
      Serial.println(message);

      // Capture the reminder and look for the colon to split the string into the message and the duration
      int colonIndex = 0;
      for (int i = 0; i < length; i++)
      {
        if (message[i] == ':')
        {
          colonIndex = i;
          break;
        }
      }

      // Copy the message into the info buffer
      for (int i = 0; i < colonIndex; i++)
      {
        infoBuf[i] = message[i];
      }
      infoBuf[colonIndex] = 0;

      // Copy the duration into the duration buffer
      for (int i = colonIndex + 1; i < length; i++)
      {
        durationBuf[i - colonIndex - 1] = message[i];
      }
      durationBuf[length - colonIndex - 1] = 0;

      // Convert the duration to an integer
      duration = atoi(durationBuf);

      Serial.print("Duration: ");
      Serial.println(duration);

      // Set the greenDuration to 1/3 of the duration, and round it to the nearest second
      greenDuration = (int)round(duration / 3.0);

      // Set the yellowDuration to 1/3 of the duration, and round it to the nearest second
      yellowDuration = (int)round(duration / 3.0);

      // Set the redDuration to 1/3 of the duration, and round it to the nearest second
      redDuration = (int)round(duration / 3.0);

      // Set the Green Alert Song
      for (int i = 0; i < 10; i++)
      {
        notesBuf[i] = greenAlert[i];
      }

      notesBuf[10] = 0;

      piezoFlag = 1;
      lcdFlag = 1;
      greenLedFlag = 1;
    }
    else
    {
      Serial.print("Message from Unknown topic: ");
      Serial.println(topic);

      Serial.print("Message: ");
      Serial.println(message);
    }
  }
}

/**
  A UTF8 String in read from a MQTT broker

*/
void handleMQTT()
{

  static long lastMQTTRun = 0;
  long currentTime = millis();

  if (currentTime - lastMQTTRun > MQTT_TASK_PERIOD)
  {
    mqttClient.loop();
    lastMQTTRun = currentTime;
  }
}

/**

*/
void handleLCD()
{
  static long lastLCDRun = 0;
  static int loopAutoscrollFlag = 0;
  long currentTime = millis();

  if (currentTime - lastLCDRun > LCD_TASK_PERIOD)
  {
    if (lcdFlag == 1)
    {
      lastLCDRun = currentTime;

      // Reset Flag
      loopAutoscrollFlag = 0;
      lcdFlag = 0;
      lcd.clear();
      lcd.setCursor(0, 0);

      // check if the infoBuf contains a newline character
      int newline_pos = -1;
      for (int i = 0; i < strlen(infoBuf) - 1; i++)
      {
        if (infoBuf[i] == '\\' && infoBuf[i + 1] == 'n')
        {
          newline_pos = i;
          break;
        }
      }

      if (newline_pos >= 0)
      {
        Serial.println("Newline Message");
        // split the infoBuf into two lines
        char line1[newline_pos + 1];
        memcpy(line1, infoBuf, newline_pos);
        line1[newline_pos] = '\0';

        char line2[strlen(infoBuf) - newline_pos];
        memcpy(line2, infoBuf + newline_pos + 2, strlen(infoBuf) - newline_pos - 1);
        line2[strlen(infoBuf) - newline_pos - 1] = '\0';

        lcd.noAutoscroll();
        // clear the LCD screen and set the cursor to the first column of the first row
        lcd.setCursor(0, 0);

        // print the first line of the infoBuf on the first row of the LCD
        lcd.print(line1);

        // set the cursor to the first column of the second row
        lcd.setCursor(0, 1);

        // print the second line of the infoBuf on the second row of the LCD
        lcd.print(line2);
      }
      else
      {
        if (strlen(infoBuf) > 16)
        {
          loopAutoscrollFlag = 1;
          Serial.println("Long Message");

          lcd.clear();
          lcd.setCursor(0, 0);
          // Print the first 16 characters
          for (int i = 0; i < 16; i++)
          {
            lcd.print(infoBuf[i]);
            // delay(500);
          }
          // enable autoscroll
          lcd.autoscroll();
          delay(1000);

          lcd.setCursor(16, 0);
          // Print the rest of the characters
          for (int i = 16; i < strlen(infoBuf); i++)
          {
            lcd.print(infoBuf[i]);
            delay(100);
          }
          lcd.noAutoscroll();
        }
        else
        {
          Serial.println("Standard Message");
          lcd.setCursor(0, 0);

          // disable autoscroll and print the message
          lcd.noAutoscroll();
          lcd.print(infoBuf);
        }
      }
    }
    else
    {
      if (loopAutoscrollFlag == 1 && songPlaying == 0)
      {
        lcd.setCursor(0, 0);
        lcd.clear();
        // Print the first 16 characters
        for (int i = 0; i < 16; i++)
        {
          lcd.print(infoBuf[i]);
          // delay(500);
        }
        // enable autoscroll
        lcd.autoscroll();
        delay(1000);

        lcd.setCursor(16, 0);
        // Print the rest of the characters
        for (int i = 16; i < strlen(infoBuf); i++)
        {
          lcd.print(infoBuf[i]);
          delay(100);
        }
        lcd.noAutoscroll();
      }
    }
  }
}

/**

*/
void parsePiezoMessage()
{
  int i = 0;
  while (notesBuf[i] != 0)
  {
    if (notesBuf[i] == '\n')
    {
      break;
    }
    int freq = notesBuf[i];
    // PITCH_T pitch = static_cast<PITCH_T>(toupper(notesBuf[i]) - 65);
    int len = notesBuf[i + 1] - 48;
    int noteIndex = i / 2;
    notes[noteIndex].freq = freq;
    notes[noteIndex].len = len;
    i += 2;
  }
  songLength = i / 2;
}

/**
 * Function to handle playing MIDI
 */
void handlePiezo()
{
  static long lastPiezoRun = 0;
  static int currentNote = 0;
  static int firstNote = 0;

  if (piezoFlag == 1)
  {
    piezoFlag = 0;
    parsePiezoMessage();
    songPlaying = 1;
    currentNote = 0;
    firstNote = 1;
  }

  long currentTime = millis();
  if (songPlaying == 1)
  {
    if (currentTime - lastPiezoRun > PIEZO_TASK_PERIOD)
    {
      lastPiezoRun = currentTime;
      if (firstNote == 1)
      {
        firstNote = 0;
        tone(PIEZO_PIN, notes[currentNote].freq, notes[currentNote].len);
      }
      if (notes[currentNote].len > 0)
      {
        notes[currentNote].len -= 1;
      }
      else
      {
        tone(PIEZO_PIN, notes[currentNote].freq, notes[currentNote].len);
        currentNote++;
        if (currentNote == songLength)
        {
          songPlaying = 0;
          noTone(PIEZO_PIN);
        }
      }
    }
  }
}
/**
 * @brief Function to check and see if the button is pressed.
 *   If it is, then publish a message on the MQTT_SONG_TOPIC
 */
void handleButton()
{
  button.handle();

  // Check if button has been pressed
  if (button.resetClicked())
  {
    Serial.println("Button Pressed!");
    // mqttClient.publish(MQTT_SONG_TOPIC, "START");

    // Clear the reminder
  }
}

/**
 * @brief Function to handle the reminder state variables
 */
void handleReminder()
{
  static long lastReminderRun = 0;
  long currentTime = millis();
  if (currentTime - lastReminderRun > REMINDER_TASK_PERIOD)
  {
    lastReminderRun = currentTime;
    if (greenDuration > 0)
    {
      greenDuration -= REMINDER_TASK_PERIOD;
    }
    else if (yellowDuration > 0)
    {
      if (greenLedFlag == 1)
      {
        greenLedFlag = 0;
        // Play the song for the yellow led
        for (int i = 0; i < 18; i++)
        {
          notesBuf[i] = yellowAlert[i];
        }
        notesBuf[18] = 0;
        piezoFlag = 1;
      }
      yellowDuration -= REMINDER_TASK_PERIOD;
      yellowLedFlag = 1;
    }
    else if (redDuration > 0)
    {
      redDuration -= REMINDER_TASK_PERIOD;
      if (yellowLedFlag == 1)
      {
        yellowLedFlag = 0;
        // Play the song for the red led
        for (int i = 0; i < 30; i++)
        {
          notesBuf[i] = redAlert[i];
        }
        notesBuf[30] = 0;
        piezoFlag = 1;
      }
      redLedFlag = 1;
    }
    else
    {
      redLedFlag = 0;
    }
  }
}

/**
 * @brief Function to handle the LED
 */
void handleLED()
{
  static long lastLEDRun = 0;
  long currentTime = millis();
  if (currentTime - lastLEDRun > LED_TASK_PERIOD)
  {

    lastLEDRun = currentTime;
    if (greenLedFlag == 1)
    {
      digitalWrite(GREEN_LED, HIGH);
    }
    else
    {
      digitalWrite(GREEN_LED, LOW);
    }

    if (redLedFlag == 1)
    {
      digitalWrite(RED_LED, HIGH);
    }
    else
    {
      digitalWrite(RED_LED, LOW);
    }

    if (yellowLedFlag == 1)
    {
      digitalWrite(YELLOW_LED, HIGH);
    }
    else
    {
      digitalWrite(YELLOW_LED, LOW);
    }
  }
}

/**
 * @brief Function to run the main loop of the system.
 */
void loop()
{
  // Handle MQTT
  handleMQTT();
  // Handle Reminder State Machine
  handleReminder();
  // LCD Task
  handleLCD();
  // Piezo Task
  handlePiezo();
  // LED Task
  handleLED();
  // Button Task
  handleButton();
}
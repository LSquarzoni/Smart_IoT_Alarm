// LIBRARIES // -------------------------------------------------------------------------------------------------------------------------
#include <WiFi.h>
#include "time.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>
#include <stdlib.h>
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <AsyncTelegram2.h>

// FUNCTION DECLARATION // -------------------------------------------------------------------------------------------------------------
void setupDisplay();
void displayCenter(String text);
void setupWiFi();
void reconnectWiFi();
void handleWiFi();
void setupTime();
void setupAlarms();
void handleTimeAndAlarms();
void checkAlarms(uint8_t currentHour, uint8_t currentMinute, uint8_t currentSecond, int currentWeekday);
void handleAlarmTrigger();
void activateAlarm(int track);
void stopAlarm();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void setupMQTT();
void handleMQTT();
void setupHTTP();
void publishSensorData(int pressureReading);
void setupSpeaker();
void handlePressureSensor();
void resetIfNeeded();
int determineTrack(String weatherCondition);
String fetchWeather();
void setupTelegram();
void handleTelegram();
int countTotalAlarms();
void handleCreateAlarm(TBMessage msg, String text);
void handleWaitingForRemove(TBMessage msg, String text);
void handleDefaultCommands(TBMessage msg, String text);
void sendWelcomeMessage(TBMessage msg);
void handleWaitingForActivate(TBMessage msg, String text);
void handleWaitingForDeactivate(TBMessage msg, String text);

// ALARM // ------------------------------------------------------------------------------------------------------------------------------
struct Alarm {
  int hour;
  int minute;
  bool active; // Whether the alarm is active or not
  bool daysOfWeek[7]; // Array representing days of the week (0 = Sunday, 1 = Monday, ..., 6 = Saturday)
};

#define MAX_ALARMS 5
Alarm alarms[MAX_ALARMS];
int alarm_flag = 0; // Alarm flag, tells if any alarm is ringing or not
unsigned long previousMillis = 0;  // Store the last time the sampling was done

unsigned long previousMillis_alarm = 0;
const long check_alarm_interval = 5000;
int PRESSURE_THRESHOLD = 2250;

// TIME SERVER // ------------------------------------------------------------------------------------------------------------------------
char ntpServer[] = "pool.ntp.org";
long gmtOffset_sec = 3600; //UTF+1 time zone
int daylightOffset_sec = 3600;

// PIN DECLARATION // -------------------------------------------------------------------------------------------------------------------
#define FORCE_SENSOR_PIN 36  // ESP32 pin GIOP36 (ADC0): the FSR and 10K pulldown are connected to A0
#define ONBOARD_LED 2 // On-board LED GPIO 2

// SCREEN // --------------------------------------------------------------------------------------------------------------------------------
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// MQTT // --------------------------------------------------------------------------------------------------------------------------------
long SAMPLING_RATE = 60000;  // Interval at which to sample (Default: 60 seconds)
int TRIGGER_ALARM = 0; // External way of triggering the alarm, 1 -> trigger (Default: 0)
int STOP_ALARM = 0; // External way of stopping the alarm, 1 -> stop (Default: 0)

const char* mqtt_server PROGMEM = "server_IP"; // Substitute with the server IP
const int mqtt_port PROGMEM = 1883;
const char* mqtt_topic1 PROGMEM = "topic/SAMPLING_RATE";
const char* mqtt_topic2 PROGMEM = "topic/TRIGGER_ALARM";
const char* mqtt_topic3 PROGMEM = "topic/STOP_ALARM";
const char* mqtt_topic4 PROGMEM = "topic/NEW_ALARM";
const char* mqtt_username PROGMEM = "user";
const char* mqtt_password PROGMEM = "pass"; // Substitute with the actual values
WiFiClient clientMQTT;
PubSubClient client_MQTT(clientMQTT);

// HTTP // --------------------------------------------------------------------------------------------------------------------------------
char http_server[] = "http://server_IP/index.php"; // Address to reach the php file on the server (data proxy)
HTTPClient client_HTTP;

// WI-FI // -----------------------------------------------------------------------------------------------------------------------------
char ssid[] = "ssid";
char password[] = "password"; // Substitute with the actual values

// SPEAKER // -------------------------------------------------------------------------------------------------------------------------------
#define PIN_MP3_TX 17 // Serial2 TX: 17
#define PIN_MP3_RX 16 // Serial2 RX: 16
SoftwareSerial softwareSerial(PIN_MP3_RX, PIN_MP3_TX);
DFRobotDFPlayerMini player;

// WEATHER // ------------------------------------------------------------------------------------------------------------------------------
char api_key[] = "open_Weather_API_key"; // Substitute with the open Weather API key
float latitude =  44.49;
float longitude = 11.34;
int TRACK_SELECTION = 5; // Select between tracks present in the sd card (Default: 5) (5: Zelda OST, 4: Song of Storms, 3: Zelda's Lullaby, 2: Gerudo Valley, 1: Ballad of the Goddess)
HTTPClient client2_HTTP;

// TELEGRAM BOT // ---------------------------------------------------------------------------------------------------------------------------
char Bot_Token[] = "telegram_token"; // Substitute with the token for the Telegram BOT
char Telegram_ID[] = "telegram_ID";
WiFiClientSecure client_telegram;
AsyncTelegram2 bot(client_telegram);

enum BotState {
  NONE,
  WAITING_FOR_REMOVE,
  WAITING_FOR_ACTIVATE,
  WAITING_FOR_DEACTIVATE
};
BotState currentBotState = NONE;
int alarmIndex = -1; // To keep track of the new alarm index

void sendCurrentAlarms(TBMessage msg, BotState nextState);





void setup() {
  
  Serial.begin(115200);
  softwareSerial.begin(9600);
  pinMode(ONBOARD_LED, OUTPUT);

  setupDisplay(); 
  setupWiFi();
  setupTime();
  setupMQTT();
  setupHTTP();
  setupSpeaker();
  setupAlarms();
  setupTelegram();

  display.setTextSize(3);
  
}




void loop() {

  handleWiFi();
  handleTimeAndAlarms();
  handleMQTT();
  handlePressureSensor();
  handleTelegram();

}



// Functions:

// DISPLAY //-------------------------------------------------------------------------------------------------------------
void setupDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    if (Serial) Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
}

void displayCenter(String text) {
  int16_t x1;
  int16_t y1;
  uint16_t width;
  uint16_t height;

  display.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);

  // display on horizontal and vertical center
  display.clearDisplay(); // clear display
  display.setCursor((SCREEN_WIDTH - width) / 2, (SCREEN_HEIGHT - height) / 2);
  display.println(text); // text to display
  display.display();
}

// WI-FI // -----------------------------------------------------------------------------------------------------------------
void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (Serial) Serial.println("Connecting to WiFi...");
  }
  if (Serial) Serial.println("Connected to WiFi");
  displayCenter("WiFi ok");
}

void reconnectWiFi() {
  if (Serial) Serial.println("Attempting to reconnect to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(10000);
    WiFi.reconnect();
  }
  if (Serial) Serial.println("Connected to WiFi");
}

void handleWiFi() {
  while (!WiFi.isConnected()) {
    reconnectWiFi();
  }
}

// TIME & ALARMS // -------------------------------------------------------------------------------------------------------------------------
void setupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void setupAlarms() {
  alarms[0] = {7, 0, true, {false, true, true, true, true, true, false}}; // Weekdays at 7:00 AM
  alarms[1] = {8, 0, true, {true, false, false, false, false, false, true}}; // Weekend at 8:00 AM
}

void handleTimeAndAlarms() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    if (Serial) Serial.println("Failed to obtain time");
    return;
  }

  uint8_t currentHour = timeinfo.tm_hour;
  uint8_t currentMinute = timeinfo.tm_min;
  uint8_t currentSecond = timeinfo.tm_sec;
  int currentWeekday = timeinfo.tm_wday;

  char timeString[12];
  sprintf(timeString, "%02d:%02d", currentHour, currentMinute);
  displayCenter(timeString);

  checkAlarms(currentHour, currentMinute, currentSecond, currentWeekday);
}

void checkAlarms(uint8_t currentHour, uint8_t currentMinute, uint8_t currentSecond, int currentWeekday) {
  for (int i = 0; i < MAX_ALARMS; ++i) {
    if (alarms[i].active && alarms[i].daysOfWeek[currentWeekday]) {
      if (currentHour == alarms[i].hour && currentMinute == alarms[i].minute && currentSecond <= 10) {
        int pressureReading = analogRead(FORCE_SENSOR_PIN);
        if (pressureReading > PRESSURE_THRESHOLD && !alarm_flag) {
          
          // Weather retrieval:
          TRACK_SELECTION = determineTrack(fetchWeather());
          
          activateAlarm(TRACK_SELECTION);
          if (Serial) Serial.println("Get up, come on!");
          alarm_flag = 1;
          TRIGGER_ALARM = 0;
        }
      }
    }
  }

  handleAlarmTrigger();
}

void handleAlarmTrigger() {
  int pressureReading = analogRead(FORCE_SENSOR_PIN);

  if (TRIGGER_ALARM && pressureReading > PRESSURE_THRESHOLD && !alarm_flag) {
    
    // Weather retrieval:
    TRACK_SELECTION = determineTrack(fetchWeather());
    
    activateAlarm(TRACK_SELECTION);
    if(Serial) Serial.println("The alarm has been triggered from MQTT command or Telegram bot.");
    alarm_flag = 1;
    TRIGGER_ALARM = 0;
  }

  unsigned long currentMillis = millis();
  
  if (pressureReading < PRESSURE_THRESHOLD && alarm_flag && !STOP_ALARM) {
    if (previousMillis_alarm == 0) {
      previousMillis_alarm = currentMillis; // Start the delay counter
    }
    
    // Check if 5 seconds have passed
    if (currentMillis - previousMillis_alarm >= check_alarm_interval) {
      if (analogRead(FORCE_SENSOR_PIN) < PRESSURE_THRESHOLD) {
        stopAlarm();
        if (Serial) Serial.println("Very good, you got up!");
        alarm_flag = 0;
        STOP_ALARM = 0;
      }
      previousMillis_alarm = 0; // Reset the delay counter
    }
  } else if (pressureReading >= PRESSURE_THRESHOLD && alarm_flag && !STOP_ALARM) {
    // Reset the delay counter if the pressure goes back up
    previousMillis_alarm = 0;
  } else if (STOP_ALARM && alarm_flag) {
    stopAlarm();
    if (Serial) Serial.println("The alarm has been stopped from MQTT command or Telegram bot.");
    alarm_flag = 0;
    STOP_ALARM = 0;
  } else if (TRIGGER_ALARM && !alarm_flag) {
    TRIGGER_ALARM = 0; // Bring back the trigger to 0 if nobody was on bed
  } else if (STOP_ALARM && !alarm_flag) {
    STOP_ALARM = 0; // Bring back the stop to 0 if nobody was on bed
  }
}

void activateAlarm(int track) {
  player.loop(track);
}

void stopAlarm() {
  player.stop();
}

// MQTT // -------------------------------------------------------------------------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  payload[length] = '\0';
  String message = String((char*)payload);

  if (Serial) Serial.print("Message received on topic: ");
  if (Serial) Serial.println(topic);
  if (Serial) Serial.print("Payload: ");
  if (Serial) Serial.println(message);

  if (strcmp(topic, mqtt_topic1) == 0) {
    long samplingRate = message.toInt();
    if (Serial) Serial.print("New Sampling Rate: ");
    if (Serial) Serial.println(samplingRate);
    SAMPLING_RATE = samplingRate; // New sampling rate
  } else if (strcmp(topic, mqtt_topic2) == 0) {
    int triggerAlarm = message.toInt();
    if (Serial) Serial.print("Trigger Alarm: ");
    if (Serial) Serial.println(triggerAlarm);
    TRIGGER_ALARM = triggerAlarm; // New alarm song
  } else if (strcmp(topic, mqtt_topic3) == 0) {
    int stopAlarm = message.toInt();
    if (Serial) Serial.print("Stop Alarm: ");
    if (Serial) Serial.println(stopAlarm);
    STOP_ALARM = stopAlarm;
  } else if (strcmp(topic, mqtt_topic4) == 0) {
    String newAlarm = message;
    if (Serial) Serial.print("New Alarm: ");
    if (Serial) Serial.println(newAlarm);
    if (countTotalAlarms() < MAX_ALARMS) {
      for (int i = 0; i < MAX_ALARMS; i++) {
        if (alarms[i].hour == 0 && alarms[i].minute == 0 && !alarms[i].active) {
          alarmIndex = i;
          break;
        }
      }
      if (alarmIndex < MAX_ALARMS) {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
          if (Serial) Serial.println("Failed to obtain time");
          return;
        }

        uint8_t currentHour = timeinfo.tm_hour;
        uint8_t currentMinute = timeinfo.tm_min;   
        int currentWeekday = timeinfo.tm_wday;     
        int newHour, newMinute;
        sscanf(newAlarm.c_str(), "%d:%d", &newHour, &newMinute);
            
        if (newHour >= 0 && newHour <= 23 && newMinute >= 0 && newMinute <= 59) {
          alarms[alarmIndex].hour = newHour;
          alarms[alarmIndex].minute = newMinute;
                
          if (currentHour < alarms[alarmIndex].hour || (currentHour == alarms[alarmIndex].hour && currentMinute < alarms[alarmIndex].minute)) {
            alarms[alarmIndex].daysOfWeek[currentWeekday] = true; // Alarm set for today
          } else {
            alarms[alarmIndex].daysOfWeek[(currentWeekday + 1) % 7] = true; // Alarm set for tomorrow
          }
                
          alarms[alarmIndex].active = true;
                
          if (Serial) Serial.println("Alarm successfully set");
        } else {
          if (Serial) Serial.println("Invalid time format. Please use HH:MM format.");
        }
      } else {
        if (Serial) Serial.println("Maximum number of alarms already set");
      }
    }   
  }
}

void setupMQTT() {
  client_MQTT.setServer(mqtt_server, mqtt_port);
  client_MQTT.setCallback(mqttCallback);
  while (!client_MQTT.connected()) {
    if (client_MQTT.connect("ESP32Client", mqtt_username, mqtt_password)) {
      if (Serial) Serial.println("Connected to MQTT broker");
      displayCenter("MQTT ok");
      client_MQTT.subscribe(mqtt_topic1);
      client_MQTT.subscribe(mqtt_topic2);
      client_MQTT.subscribe(mqtt_topic3);
      client_MQTT.subscribe(mqtt_topic4);
    } else {
      if (Serial) Serial.print("Failed, rc=");
      if (Serial) Serial.print(client_MQTT.state());
      if (Serial) Serial.println(" Retrying...");
      delay(2000);
    }
  }
}

void handleMQTT() {
  client_MQTT.loop();
}

// HTTP // -----------------------------------------------------------------------------------------------------------------------
void setupHTTP() {
  client_HTTP.begin(http_server);
}

void publishSensorData(int pressureReading) {
  // Check if pressureReading is an integer
  if (pressureReading < 0 || pressureReading > 5000) {
    if (Serial) Serial.println("Error: pressureReading is not correct.");
    return;
  }

  if (Serial) Serial.print("The force sensor value = ");
  if (Serial) Serial.println(pressureReading);  // print the raw analog reading

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    if (Serial) Serial.println("Failed to obtain time");
    return;
  }
  uint8_t currentHour = timeinfo.tm_hour;
  uint8_t currentMinute = timeinfo.tm_min;
  uint8_t currentSecond = timeinfo.tm_sec;

  char timeString[12];
  sprintf(timeString, "%02d:%02d:%02d", currentHour, currentMinute, currentSecond);
  if (Serial) Serial.println(timeString);
  
  char reading[50];
  snprintf(reading, sizeof(reading), "%d", pressureReading); // Casting to char string

  // HTTP publishing
  client_HTTP.begin(http_server);
  client_HTTP.addHeader("Content-Type", "text/plain");
  char queryString[60]; // Allocate memory for the query string
  snprintf(queryString, sizeof(queryString), "%s", reading);

  unsigned long startTime = millis();  // Record the start time
  int httpCode = client_HTTP.POST(queryString);

  unsigned long endTime = millis();  // Record the end time
  unsigned long transmissionDelay = endTime - startTime;  // Calculate the delay

  // httpCode will be negative on error
  if (httpCode > 0) {
    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = client_HTTP.getString();
      if (Serial) Serial.println(payload);
    } else {
      // HTTP header has been send and Server response header has been handled
      if (Serial) Serial.printf("[HTTP] POST code: %d\n", httpCode);
    }
  } else {
    if (Serial) Serial.printf("[HTTP] POST failed, error: %s\n", client_HTTP.errorToString(httpCode).c_str());
  }

  if (Serial) Serial.printf("Data transmission delay: %lu ms\n", transmissionDelay);

  client_HTTP.end();
}

// SPEAKER // ----------------------------------------------------------------------------------------------------------------------
void setupSpeaker() {
  player.begin(softwareSerial);
  player.volume(20); // 0-30 volume
}

// PRESSURE SENSOR // --------------------------------------------------------------------------------------------------------------
void handlePressureSensor() {
  int pressureReading = analogRead(FORCE_SENSOR_PIN);
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= SAMPLING_RATE) {
    previousMillis = currentMillis;
    resetIfNeeded();
    publishSensorData(pressureReading);
  }
}

void resetIfNeeded() {
    // If free heap memory is below a threshold, reset the device
    if (ESP.getFreeHeap() < 5000) {
        if (Serial) Serial.println("Low memory, restarting...");
        ESP.restart();
    }
}

// WEATHER // ---------------------------------------------------------------------------------------------------------------------
int determineTrack(String weatherCondition) { // (5: Zelda OST, 4: Song of Storms, 3: Zelda's Lullaby, 2: Gerudo Valley, 1: Ballad of the Goddess)
  if (weatherCondition != "") {
    int track;
    if (weatherCondition.equals("Clear")) {
      track = 1;  // Ballad of the Goddess
    } else if (weatherCondition.equals("Clouds") || weatherCondition.equals("Drizzle")) {
      track = 3;  // Zelda's Lullaby
    } else if (weatherCondition.equals("Rain") || weatherCondition.equals("Thunderstorm")) {
      track = 4;  // Song of Storms
    } else {
      track = 5;  // Zelda OST (default)
    }
    return track;
  } else {
    if (Serial) Serial.println("Weather not found.\n");
    return 2;
  }
}

String fetchWeather() {
  String serverPath = "http://api.openweathermap.org/data/3.0/onecall?lat=" + String(latitude) + "&lon=" + String(longitude) + "&exclude=minutely,hourly,daily,alerts&appid=" + api_key;

  client2_HTTP.begin(serverPath.c_str());
  int httpResponseCode = client2_HTTP.GET();

  if (httpResponseCode > 0) {
    String payload = client2_HTTP.getString();

    StaticJsonDocument<0> filter;
    StaticJsonDocument<768> doc;
    filter.set(true);
    
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
    }

    const char* weatherCondition = doc["current"]["weather"][0]["main"];
    client2_HTTP.end();
    Serial.println(String(weatherCondition));
    return String(weatherCondition);

  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpResponseCode);
    client2_HTTP.end();
    return "";
  }
}

// TELEGRAM // ---------------------------------------------------------------------------------------------------------
void setupTelegram() {
  client_telegram.setCACert(telegram_cert);
  bot.setUpdateTime(30000); // Check for new messages each half minute, to avoid reaching the rate limit imposed by Telegram
  bot.setTelegramToken(Bot_Token);
  if(bot.begin()) {
    if (Serial) Serial.println("Telegram ok\n"); 
    displayCenter("BOT ok"); 
  } else {
    if (Serial) Serial.println("Telegram NOT ok\n");
  }
}

void handleTelegram() {
  TBMessage msg;
  if (bot.getNewMessage(msg)) {
    String chat_id = String(msg.chatId);
    if (chat_id != Telegram_ID) {
      bot.sendMessage(msg, "Unauthorized user");
      return;
    }

    String text = msg.text;
    if (Serial) Serial.println("Received message: " + text);

    if (currentBotState == WAITING_FOR_REMOVE) {
      handleWaitingForRemove(msg, text);
    } else if (currentBotState == WAITING_FOR_ACTIVATE) {
      handleWaitingForActivate(msg, text);
    } else if (currentBotState == WAITING_FOR_DEACTIVATE) {
      handleWaitingForDeactivate(msg, text);
    } else {
      handleDefaultCommands(msg, text);
    }
  }
}
    
int countTotalAlarms() {
  int totalAlarms = 0;
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarms[i].hour != 0 || alarms[i].minute != 0 || alarms[i].active) {
      totalAlarms++;
    }
  }
  return totalAlarms;
}

void handleCreateAlarm(TBMessage msg, String text) {
  int hour, minute;
  char daysBuffer[50];

  if (sscanf(text.c_str(), "%d:%d %49[^\n]", &hour, &minute, daysBuffer) == 3) {
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
      bot.sendMessage(msg, "Invalid time. Please use HH:MM format.");
      return;
    }

    const char* daysOfWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    bool validDays = false;
    bool daysSelected[7] = {false}; // Track selected days

    for (int j = 0; j < 7; j++) {
      if (strstr(daysBuffer, daysOfWeek[j]) != NULL) {
        daysSelected[j] = true;
        validDays = true;
      }
    }

    if (!validDays) {
      bot.sendMessage(msg, "Invalid days. Please use a comma-separated list of days (e.g., Mon,Wed,Fri).");
      return;
    }

    for (int i = 0; i < MAX_ALARMS; i++) {
      if (alarms[i].hour == 0 && alarms[i].minute == 0 && !alarms[i].active) {
        alarms[i].hour = hour;
        alarms[i].minute = minute;
        memcpy(alarms[i].daysOfWeek, daysSelected, sizeof(daysSelected));
        alarms[i].active = true;
        bot.sendMessage(msg, "Alarm set successfully.");
        return;
      }
    }

    bot.sendMessage(msg, "Failed to set the alarm. Please try again.");
  } else {
    bot.sendMessage(msg, "Invalid format. Please use: HH:MM days (e.g., 08:30 Mon,Wed,Fri).");
  }
}

void handleWaitingForRemove(TBMessage msg, String text) {
  int alarmToRemove = text.toInt() - 1;
  if (alarmToRemove >= 0 && alarmToRemove < MAX_ALARMS) {
    alarms[alarmToRemove].hour = 0;
    alarms[alarmToRemove].minute = 0;
    alarms[alarmToRemove].active = false;
    memset(alarms[alarmToRemove].daysOfWeek, 0, sizeof(alarms[alarmToRemove].daysOfWeek));
    bot.sendMessage(msg, "Alarm removed successfully.");
  } else {
    bot.sendMessage(msg, "Invalid alarm number. Please send a valid alarm number to remove.");
  }
  currentBotState = NONE;
}

void handleDefaultCommands(TBMessage msg, String text) {
  if (text == "/start") {
    sendWelcomeMessage(msg);
  } else if (text.startsWith("/remove_alarm")) {
    sendCurrentAlarms(msg, WAITING_FOR_REMOVE);
  } else if (text.startsWith("/stop_alarm")) {
    bot.sendMessage(msg, "Alarm stopped.");
    STOP_ALARM = 1;
  } else if (text.startsWith("/trigger_alarm")) {
    bot.sendMessage(msg, "Alarm triggered.");
    TRIGGER_ALARM = 1;
  } else if (text.startsWith("/alarms_state")) {
    sendCurrentAlarms(msg, NONE);
  } else if (text.startsWith("/create_alarm")) { // Example: "/create_alarm 08:30 Mon,Wed,Fri"
    if (countTotalAlarms() >= MAX_ALARMS) {
      bot.sendMessage(msg, "Maximum number of alarms reached.");
    } else {
      String commandArgs = text.substring(13); // Remove "/create_alarm " from the text
      handleCreateAlarm(msg, commandArgs);
    }    
  } else if (text.startsWith("/activate_alarm")) {
    sendCurrentAlarms(msg, WAITING_FOR_ACTIVATE);
  } else if (text.startsWith("/deactivate_alarm")) {
    sendCurrentAlarms(msg, WAITING_FOR_DEACTIVATE);
  } else {
    bot.sendMessage(msg, "Unrecognized command. Type /start for available commands.");
  }
}

void sendWelcomeMessage(TBMessage msg) {
  String welcome = "Welcome! Use the following commands to manage your alarms:\n\n";
  welcome += "/create_alarm - Set a new alarm, ex: create_alarm 08:30 Mon,Wed,Fri\n";
  welcome += "/remove_alarm - Remove an existing alarm\n";
  welcome += "/alarms_state - View existing alarms\n";
  welcome += "/trigger_alarm - Trigger an alarm\n";
  welcome += "/stop_alarm - Stop an alarm that is playing\n";
  welcome += "/activate_alarm - Activate an existing inactive alarm\n";
  welcome += "/deactivate_alarm - Deactivate an existing active alarm\n";
  bot.sendMessage(msg, welcome);
}

void sendCurrentAlarms(TBMessage msg, BotState nextState) {
  String alarmState = "Current alarms:\n";
  for (int j = 0; j < MAX_ALARMS; j++) {
    alarmState += String(j + 1) + ": ";
    alarmState += (alarms[j].active ? "Active" : "Inactive");
    alarmState += " - Time: " + String(alarms[j].hour) + ":" + String(alarms[j].minute) + " - Days: ";

    const char* daysOfWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    bool first = true;
    for (int k = 0; k < 7; k++) {
      if (alarms[j].daysOfWeek[k]) {
        if (!first) {
          alarmState += ", ";
        }
        alarmState += daysOfWeek[k];
        first = false;
      }
    }

    if (first) {
      alarmState += "None"; // No days active
    }

    alarmState += "\n";
  }
  
  switch (nextState) {
    case WAITING_FOR_REMOVE:
      alarmState += "\nPlease send the number of the alarm you want to remove.";
      break;
    case WAITING_FOR_ACTIVATE:
      alarmState += "\nPlease send the number of the alarm you want to activate.";
      break;
    case WAITING_FOR_DEACTIVATE:
      alarmState += "\nPlease send the number of the alarm you want to deactivate.";
      break;
    default:
      break;
  }
  
  bot.sendMessage(msg, alarmState);
  currentBotState = nextState;
}

void handleWaitingForActivate(TBMessage msg, String text) {
  int alarmToActivate = text.toInt() - 1;
  if (alarmToActivate >= 0 && alarmToActivate < MAX_ALARMS) {
    alarms[alarmToActivate].active = true;
    bot.sendMessage(msg, "Alarm activated successfully.");
  } else {
    bot.sendMessage(msg, "Invalid alarm number. Please send a valid alarm number to activate.");
  }
  currentBotState = NONE;
}

void handleWaitingForDeactivate(TBMessage msg, String text) {
  int alarmToDeactivate = text.toInt() - 1;
  if (alarmToDeactivate >= 0 && alarmToDeactivate < MAX_ALARMS) {
    alarms[alarmToDeactivate].active = false;
    bot.sendMessage(msg, "Alarm deactivated successfully.");
  } else {
    bot.sendMessage(msg, "Invalid alarm number. Please send a valid alarm number to deactivate.");
  }
  currentBotState = NONE;
}

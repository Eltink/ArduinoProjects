#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <LiquidCrystal_I2C.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <HTTPClient.h>

// Sensor and Display Instances
Adafruit_SHT4x sht40 = Adafruit_SHT4x();
LiquidCrystal_I2C lcd(0x27, 16, 2);
SPIClass mySPI(VSPI);

// Timing and Logging Variables
unsigned long previousMillis = 0;
const float logs_per_min = 1;
const long interval = 60000 / logs_per_min;
int counter = 1;

// WiFi and NTP Client
const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = 3600;
WiFiUDP udp;
NTPClient timeClient(udp, ntpServer, utcOffsetInSeconds);

// File Handling
File dataFile;
String fileName;

// Button Variables
const int buttonPin = 17;
bool umluftActive = false;
unsigned long umluftStartTime = 0;
int lastState = HIGH;
int currentState ;
String umluft_str = "";

String getCurrentTimestamp() {
  // return String(year()) + String(month()) + String(day()) + " " + String(hour()) + String(minute()) + String(second());
  String yearStr   = String(year());
  String monthStr  = (month()  < 10) ? "0" + String(month())  : String(month());
  String dayStr    = (day()    < 10) ? "0" + String(day())    : String(day());
  String hourStr   = (hour()   < 10) ? "0" + String(hour())   : String(hour());
  String minuteStr = (minute() < 10) ? "0" + String(minute()) : String(minute());
  String secondStr = (second() < 10) ? "0" + String(second()) : String(second());
  
  return yearStr + "/" + monthStr + "/" + dayStr + " " + hourStr + ":" + minuteStr + ":" + secondStr;
}

void myLCDprint(String text, int line) {
  lcd.setCursor(0, line); //coluna 0, linha "line"
  lcd.print("                ");
  lcd.setCursor(0, line);
  lcd.print(text);
}

void initializeSensor() {
  if (!sht40.begin()) {
    Serial.println("Couldn't find SHT40 sensor!");
    while (1) delay(10);
  }
  sht40.setPrecision(SHT4X_HIGH_PRECISION);
}

void initializeSDCard() {
  mySPI.begin(18, 19, 23, 5);
  if (!SD.begin(5, mySPI)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SD Card Error");
    Serial.println("SD card initialization failed!");
    while (1);
  }
}

void initializeWiFi() {
  WiFi.begin("RageCage", "edelstoff");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi!");
}

void initializeNTPClient() {
  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  setTime(timeClient.getEpochTime());
}

void createLogFile() {
  fileName = getCurrentTimestamp();
  fileName.replace(' ', '_');
  fileName.replace('/', '_');
  fileName.replace(':', '_');
  fileName = "/data_from_" + fileName + ".txt";
  dataFile = SD.open(fileName.c_str(), FILE_WRITE);
  if (dataFile) {
    dataFile.println(";temp;RelHum;umluftStr"); // File header
    dataFile.close();
    Serial.print("New file created: ");
    Serial.println(fileName);
  } else {
    Serial.println("Error opening new data file");
  }
}

void updateNTPTime() {
  timeClient.update();
  setTime(timeClient.getEpochTime());
}

void checkButton() {
  currentState = digitalRead(buttonPin);
  umluft_str = "";
  if (umluftActive) {
    umluft_str = "umlufting";
  }
  if(lastState == LOW && currentState == HIGH) {
    if (!umluftActive) {
      umluftStartTime = millis();
      umluft_str = "Started umluft";
      myLCDprint(umluft_str, 0);
    } else {
      umluft_str = "umluft lasted " + String((millis() - umluftStartTime) / 60000.0) + " min";
      myLCDprint("Umluft lasted", 0);
      myLCDprint((String((millis() - umluftStartTime) / 60000.0) + " min"), 1);
    }
    umluftActive = !umluftActive;
    delay(2000); // To read LCD info
    lcd.clear();
  }
  lastState = currentState; // Save last button state
}

void logSensorData() {
  sensors_event_t humEvent, tempEvent;
  sht40.getEvent(&humEvent, &tempEvent);

  // Raw numeric values (use DOT for JSON)
  float temperature = tempEvent.temperature;
  float humidity    = humEvent.relative_humidity;

  // --- Write CSV to SD with localized comma ---
  String tempCsv = String(temperature, 2);
  String humCsv  = String(humidity, 2);
  tempCsv.replace('.', ',');
  humCsv.replace('.', ',');
  String logData = getCurrentTimestamp() + ";" + tempCsv + ";" + humCsv + ";" + umluft_str;

  File f = SD.open(fileName.c_str(), FILE_APPEND);
  if (f) { f.println(logData); f.close(); }
  
  // --- Build VALID JSON (numbers with dot; strings quoted & escaped) ---
  String umluftEsc = umluft_str;          // make a copy to escape quotes/backslashes if needed
  umluftEsc.replace("\\", "\\\\");
  umluftEsc.replace("\"", "\\\"");
  String payload = String("{\"temp\":") + String(temperature, 2) +
                   ",\"hum\":" + String(humidity, 2) +
                   ",\"umluft\":\"" + umluftEsc + "\"," +
                   "\"device\":\"esp32-dev\"}";

  // --- POST to GAS ---
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);    // follow 302 -> googleusercontent
  http.setTimeout(15000);                                    // robust against slow TLS handshakes
  http.begin("https://script.google.com/macros/s/AKfycbwAv9NI1TeACC83YM9mMPCGmyKamKy-3tMEYmlFpHmmGAvGKPJu0KbDd_bMGTWNRfeA3A/exec");
  http.addHeader("Content-Type", "application/json");
  // Optional: http.addHeader("Accept", "application/json");

  int code = http.POST(payload);
  Serial.printf("HTTP %d\n", code);
  Serial.println(http.getString());
  http.end();
}

void displayDataOnLCD() {
  sensors_event_t humEvent, tempEvent;
  sht40.getEvent(&humEvent, &tempEvent);
  float temperature = tempEvent.temperature;
  float humidity = humEvent.relative_humidity;

  lcd.setCursor(0, 0);
  lcd.print("T: ");
  lcd.print(temperature, 2);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("H: ");
  lcd.print(humidity, 2);
  lcd.print(" %");
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP); // Initialize button pin
  initializeSensor();
  lcd.begin(16, 2);
  lcd.backlight();
  initializeSDCard();
  initializeWiFi();
  initializeNTPClient();
  createLogFile();
}

void loop() {
  updateNTPTime();
  checkButton();
  displayDataOnLCD();
  if (millis() > previousMillis + interval) {
    logSensorData();
    previousMillis = millis();
  }
  delay(100);
  counter++;
}

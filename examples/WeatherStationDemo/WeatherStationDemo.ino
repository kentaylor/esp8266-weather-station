/**The MIT License (MIT)

Copyright (c) 2015 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch
*/
#include "DHT.h" //https://github.com/adafruit/DHT-sensor-library 
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <JsonListener.h>
#include "SSD1306.h"
#include "SSD1306Ui.h"
#include "Wire.h"
#include "WundergroundClient.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "TimeClient.h"
#include "ThingspeakClient.h"

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define DHTPIN D2     // what digital pin we're connected to


/***************************
 * Begin Settings
 **************************/
// WIFI
const char* WIFI_SSID = "YourSSID"; 
const char* WIFI_PWD = "YourPassword";

WiFiClient client;


// Setup
const int UPDATE_INTERVAL_SECS = 10 * 60; // Update every 10 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D3;
const int SDC_PIN = D4;

// TimeClient settings
const float UTC_OFFSET = 10;

// Wunderground Settings
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "YourAPI_Key";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_COUNTRY = "AU";
const String WUNDERGROUND_CITY = "Canberra";

//Thingspeak Settings
const String THINGSPEAK_CHANNEL_ID = "YourCHANNEL_ID";
const String THINGSPEAK_API_READ_KEY = "YourAPI_READ_KEY";

bool drawFrame1(SSD1306 *display, SSD1306UiState* state, int x, int y);
bool drawFrame2(SSD1306 *display, SSD1306UiState* state, int x, int y);
bool drawFrame3(SSD1306 *display, SSD1306UiState* state, int x, int y);
bool drawFrame4(SSD1306 *display, SSD1306UiState* state, int x, int y);
bool drawFrame5(SSD1306 *display, SSD1306UiState* state, int x, int y);

void updateData(SSD1306 *display);
void setReadyForWeatherUpdate();
void drawProgress(SSD1306 *display, int percentage, String label);
void drawForecast(SSD1306 *display, int x, int y, int dayIndex);
void updateThingSpeak(String);

// Initialize the oled display for address 0x3c
// sda-pin=14 and sdc-pin=12
SSD1306   display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
SSD1306Ui ui     ( &display );
DHT dht(DHTPIN, DHTTYPE);

/***************************
 * End Settings
 **************************/

TimeClient timeClient(UTC_OFFSET);

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClient wunderground(IS_METRIC);

ThingspeakClient thingspeak;

// ThingSpeak Settings
char thingSpeakAddress[] = "api.thingspeak.com";
String writeAPIKey = "Your_writeAPIKey";
const int updateThingSpeakInterval = 120 * 1000;      // Time interval in milliseconds to update ThingSpeak (number of seconds * 1000 = interval)
// Variable Setup
long lastConnectionTime = -10000; 
int failedCounter = 0;

// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
bool (*frames[])(SSD1306 *display, SSD1306UiState* state, int x, int y) = { drawFrame1, drawFrame2, drawFrame3, drawFrame4, drawFrame5 };
int numberOfFrames = 5;

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";
String localTemperature = "";
String localHumidity = "";

Ticker ticker;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // initialize display
  display.init();
  display.clear();
  display.display();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  WiFi.begin(WIFI_SSID, WIFI_PWD);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(WiFi.status());
    Serial.print(" . ");
    Serial.println(counter);
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();

    counter++;
  }

  ui.setTargetFPS(30);

  ui.setActiveSymbole(activeSymbole);
  ui.setInactiveSymbole(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  // Add frames
  ui.setFrames(frames, numberOfFrames);
  ui.setTimePerFrame(5000); //5 seconds 
  // Inital UI takes care of initalising the display too.
  ui.init();

  Serial.println("");
  dht.begin();

  updateData(&display);

  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);
  display.flipScreenVertically();

}

void loop() {

  if (readyForWeatherUpdate && ui.getUiState().frameState == FIXED) {
    updateData(&display);
    Serial.println("update display");   
  }

  int remainingTimeBudget = ui.update();
    
  if ( millis()-lastConnectionTime>updateThingSpeakInterval ) {
    Serial.println("upload temperature/humidity to thingspeak");
    float humidity = dht.readHumidity();  
    float temperature = dht.readTemperature();
    String tt(temperature, 2);
    String hh(humidity, 2);   
    localTemperature = tt;
    localHumidity = hh;
    updateThingSpeak("field1="+localTemperature+"&field2="+localHumidity);  
  }  

  //if ( remainingTimeBudget>0 ) {

    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.    
    //delay(remainingTimeBudget);
        // Update ThingSpeak
  //}

}

void updateData(SSD1306 *display) {
    
  display->flipScreenVertically();
  drawProgress(display, 10, "Updating time...");
  timeClient.updateTime();
  drawProgress(display, 30, "Updating conditions...");
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 50, "Updating forecasts...");
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 80, "Updating thingspeak...");
  thingspeak.getLastChannelItem(THINGSPEAK_CHANNEL_ID, THINGSPEAK_API_READ_KEY);
  lastUpdate = timeClient.getFormattedTime();
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(1000);
}

void drawProgress(SSD1306 *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawRect(10, 28, 108, 12);
  display->fillRect(12, 30, 104 * percentage / 100 , 9);
  display->display();
}


bool drawFrame1(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = wunderground.getDate();
  int textWidth = display->getStringWidth(date);
  display->drawString(64 + x, 10 + y, date);
  display->setFont(ArialMT_Plain_24);
  String time = timeClient.getFormattedTime();
  textWidth = display->getStringWidth(time);
  display->drawString(64 + x, 20 + y, time);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

bool drawFrame2(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  display->setFont(ArialMT_Plain_10);
  display->drawString(60 + x, 10 + y, wunderground.getWeatherText());

  display->setFont(ArialMT_Plain_24);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(60 + x, 20 + y, temp);
  int tempWidth = display->getStringWidth(temp);

  display->setFont(Meteocons_0_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 10 + y, weatherIcon);
}

bool drawFrame3(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(32 + x, 0 + y, "Humidity");
  display->drawString(96 + x, 0 + y, "Pressure");
  display->drawString(32 + x, 28 + y, "Precipit.");

  display->setFont(ArialMT_Plain_16);
  display->drawString(32 + x, 10 + y, wunderground.getHumidity());
  display->drawString(96 + x, 10 + y, wunderground.getPressure());
  display->drawString(32 + x, 38 + y, wunderground.getPrecipitationToday());
}

bool drawFrame4(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  drawForecast(display, x, y, 0);
  drawForecast(display, x + 44, y, 2);
  drawForecast(display, x + 88, y, 4);
}

bool drawFrame5(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0 + y, "Indoor"); //outdoor if read from thingspeak
  display->setFont(ArialMT_Plain_16);
  //display->drawString(64 + x, 10 + y, thingspeak.getFieldValue(0) + "°C");
  //display->drawString(64 + x, 30 + y, thingspeak.getFieldValue(1) + "%");
  display->drawString(64 + x, 10 + y, localTemperature + "°C");
  display->drawString(64 + x, 30 + y, localHumidity + "%"); 
  
}

void drawForecast(SSD1306 *display, int x, int y, int dayIndex) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  display->drawString(x + 20, y, day);

  display->setFont(Meteocons_0_21);
  display->drawString(x + 20, y + 15, wunderground.getForecastIcon(dayIndex));

  display->setFont(ArialMT_Plain_16);
  display->drawString(x + 20, y + 37, wunderground.getForecastLowTemp(dayIndex) + "/" + wunderground.getForecastHighTemp(dayIndex));
  //display.drawString(x + 20, y + 51, );
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

void updateThingSpeak(String tsData) {
  
    if (client.connect(thingSpeakAddress, 80)) {    
           
        client.print("POST /update HTTP/1.1\n");
        client.print("Host: api.thingspeak.com\n");
        client.print("Connection: close\n");
        client.print("X-THINGSPEAKAPIKEY: "+writeAPIKey+"\n");
        client.print("Content-Type: application/x-www-form-urlencoded\n");
        client.print("Content-Length: ");
        client.print(tsData.length());
        client.print("\n\n");

        client.print(tsData);
        lastConnectionTime = millis();

        if (client.connected()) {
            Serial.println("Connecting to ThingSpeak...");
            Serial.println();
            failedCounter = 0;
        } else {
            failedCounter++;
            Serial.println("Connection to ThingSpeak failed ("+String(failedCounter, DEC)+")");   
            Serial.println();
        }
    } else {
        failedCounter++;
        Serial.println("Connection to ThingSpeak Failed ("+String(failedCounter, DEC)+")");   
        Serial.println();
        lastConnectionTime = millis(); 
    }
}



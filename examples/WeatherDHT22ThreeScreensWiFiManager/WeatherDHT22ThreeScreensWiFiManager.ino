/**The MIT License (MIT)
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

Author Ken Taylor
Derived from software by Daniel Eichhorn http://blog.squix.ch
Derived from software by Jason Chen

This demo includes WiFiManager so that you can set up WiFi configuration without 
configuring the WiFi credentials in code and recompiling. WiFi credentials are 
entered using a web browser. You must use the version of WiFiManager from 
https://github.com/kentaylor/WiFiManager.

Add your own configuration parameters at each location labelled "Yours".
The configuration parameters are obtained from Weather Underground and ThingSpeak.
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
#include <OneWire.h> // Used for talking to DS18B20 temp sensor. 
// Include the libraries needed for DS18B20 temperature measurement from https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DallasTemperature.h> 
#include <WiFiManager.h>          //https://github.com/kentaylor/WiFiManager

/*Trigger for inititating config mode is Pin D3 and also flash button on NodeMCU
 * Flash button is convenient to use but if it is pressed it will stuff up the serial port device driver 
 * until the computer is rebooted on windows machines.
 */
const int TRIGGER_PIN = D0; // Wake up pin for deep sleep mode NodeMCU and WeMos.
// Indicates whether ESP has WiFi credentials saved from previous session
bool initialConfig = false;

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define DHTPIN D2     // what digital pin we're connected to
// GPIO pin which DS18B20 is plugged into. Port 5 on the NodeMCU is pin D1
#define ONE_WIRE_BUS 5
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress Thermometer;


/***************************
 * Begin Settings
 **************************/

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
const String WUNDERGRROUND_API_KEY = ""; //Yours
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_COUNTRY = "AU";
const String WUNDERGROUND_CITY = "Canberra";

//Thingspeak Settings
const String THINGSPEAK_CHANNEL_ID = ""; //Yours
const String THINGSPEAK_API_READ_KEY = ""; //Yours

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
String writeAPIKey = ""; //Yours
const int updateThingSpeakInterval = 120 * 1000;      // Time interval in milliseconds to update ThingSpeak (number of seconds * 1000 = interval)
// Variable Setup
long lastConnectionTime = -10000000; 
int failedCounter = 0;

// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
bool (*frames[])(SSD1306 *display, SSD1306UiState* state, int x, int y) = {  drawFrame2, drawFrame3, drawFrame4 };
int numberOfFrames = 3;

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";
String localTemperature = "";
String localHumidity = "";
String DS18B20Temperature = "";
unsigned long SensorReadTime = 50000;
float humidity = NAN;  
float temperature = NAN; 
float temperatureDS18b20 = NAN;

Ticker ticker;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");
  pinMode(TRIGGER_PIN, INPUT_PULLUP); // Set up WiFi configuration button
  if (WiFi.SSID()==""){
    Serial.println("We haven't got any access point credentials, so get them now");   
    initialConfig = true;
  }
  else{
    WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
  }
  dht.begin();

  // locate DS18B20 devices on the bus. Internal pullup resistor works but is much larger than than the external 4.7K specified. Risky!
  sensors.begin();
  sensors.setResolution(12);
  int SensorCount = sensors.getDeviceCount(); 
  sensors.requestTemperatures(); // Send the command to get temperatures leave as 12 bit by default
  Serial.print("Found ");
  Serial.print(SensorCount);
  Serial.println(" temperature sensors.");
  for (int i =0; i<SensorCount; i++)
    {
    float temp = sensors.getTempCByIndex(i);
    Serial.print("Temperature ");
    Serial.print(i);
    Serial.print(" ");
    Serial.println(temp);
    }
  sensors.setWaitForConversion(FALSE); //This must be used with caution. See comments on this command in Dallas Temperature.cpp
  /*  sensors.requestTemperatures() will become a non blocking call but it will take
   *  about 750ms for the temp sensor to complete the reading. sensors.getTempCByIndex should not be called until the sensor
   *  has completed taking the reading i.e wait at least 1 second.
   */
   
  // initialize display
  display.init();
  display.clear();
  display.display();

  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);
  
  //Wait for WiFi to get connected
  int counter = 0;
   unsigned long startedAt = millis();
  while (int status = WiFi.status() != WL_CONNECTED) {
    Serial.print(status);
    Serial.print(" . ");
    Serial.println(counter); 
    delay(500);
    display.clear();
    switch (status)
   {
      // WL_NO_SHIELD 255
       //WL_SCAN_COMPLETED 2
       //WL_CONNECTION_LOST 5
      case WL_IDLE_STATUS: { //0
        display.drawString(64, 10, "Device WiFi Failed");
        break;
      }
      case WL_NO_SSID_AVAIL: { // 1
        display.drawString(64, 10, "Connecting to WiFi");
        String text = WiFi.SSID();
        if (counter>60) {
          display.drawString(64, 40,text);
          display.drawString(64, 54, "Push button to configure");
        }
        else {
          display.drawString(64, 45,text);
        }
        break;
        }
      case WL_DISCONNECTED: {  // 6
        display.drawString(64, 10, "Connecting to WiFi");
        String text = WiFi.SSID();
        text = text + " not visible";
        display.drawString(64, 40,text);
        display.drawString(64, 54, "Push button to configure WiFi");
        break;
      }     
      case WL_CONNECTED: break; // 3
      case WL_CONNECT_FAILED: { //4
        display.drawString(64, 10, "Connecting to WiFi");
        display.drawString(64, 40, "Connect failed. Push button ");
        display.drawString(64, 54, "to configure WiFi");
        break;
      }
      default: display.drawString(64, 10, "Connecting to WiFi");
    }   
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();
    counter++;
     if ((digitalRead(TRIGGER_PIN)) == LOW) {
       initialConfig = TRUE;
       break;
     }
  }
  Serial.print("After waiting ");
  float waited = (millis()- startedAt); 
  Serial.print(waited/1000);
  Serial.print(" secs in setup() connection result is ");
  Serial.println(WiFi.status());
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
  ui.setTimePerFrame(6000); //6 seconds 
  // Inital UI takes care of initalising the display too.
  ui.init();

  Serial.println("");
  if (!initialConfig){
    updateData(&display);

    ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);
    //display.flipScreenVertically();
  }
}

void loop() {
  // is configuration portal requested?
  if ((digitalRead(TRIGGER_PIN) == LOW) || (initialConfig)) {
     Serial.println("Configuration portal requested.");
    //Local intialization. Once its business is done, there is no need to keep it around
     display.clear();
     display.setTextAlignment(TEXT_ALIGN_CENTER);
     display.setFont(ArialMT_Plain_10);
     display.drawString(64, 5, "WiFi Configuration mode");
     display.drawString(64, 20, "Go to http://192.168.1.4");
     display.drawString(64, 35, "after connecting computer");
     String text = "ESP" + String(ESP.getChipId());
     text = "to " + text + " Wifi Network";
     display.drawString(64, 50, text);
     
     display.display();
     WiFiManager wifiManager;

    //sets timeout in seconds until configuration portal gets turned off.
    //If not specified device will remain in configuration mode until
    //switched off via webserver or device is restarted.
    //wifiManager.setConfigPortalTimeout(600);

    //it starts an access point 
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.startConfigPortal()) {
      Serial.println("Not connected to WiFi but continuing anyway.");
    } else {
      //if you get here you have connected to the WiFi
      Serial.println("connected...yeey :)");
    }
    ESP.reset(); // This is a bit crude. For some unknown reason webserver can only be started once per boot up 
    // so resetting the device allows to go back into config mode again when it reboots.
    delay(5000);
  }
  
  if (readyForWeatherUpdate && ui.getUiState().frameState == FIXED) {
    updateData(&display);
    Serial.println("update display");   
  }
  int remainingTimeBudget = ui.update(); 
    //If less than remaining time budget which is a few milliseconds then animations will be smooth.
   //if ( remainingTimeBudget>0 ) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.}
  unsigned long TimeInterval = abs( millis() - SensorReadTime);
  if (TimeInterval>15000 ) {
    SensorReadTime = millis();
    humidity = dht.readHumidity();  
    temperature = dht.readTemperature(); 
    float temp = sensors.getTempCByIndex(0);
    if (temp != 85.0) temperatureDS18b20 = temp; //Read 85 at start up. If starting up keep previous temperature 
    Serial.print("temperature = ");
    Serial.print(temperature);
    Serial.print("  DS18B20temperature = ");
    Serial.print(temp);
    Serial.print("  humidity = ");
    Serial.println(humidity);
    }
  if ((humidity>0) && (temperature>-100)&& (temperatureDS18b20>-100)) { //Checking readings were real i.e. not ISNAN
    sensors.requestTemperatures(); //initiate ds18B20 read
    if ( millis()-lastConnectionTime>updateThingSpeakInterval ) {
      Serial.println("upload temperature/humidity to thingspeak");
      String tt(temperature, 1);
      String hh(humidity, 1);
      String DS18B20(temperatureDS18b20, 1);   
      localTemperature = tt;
      localHumidity = hh;
      DS18B20Temperature = DS18B20;
      updateThingSpeak("field1="+localTemperature+"&field2="+localHumidity+"&field3="+DS18B20Temperature);  
    }  
    localHumidity = String(humidity,0);
  }
  delay(500); // Make this less to get a sideways scrolling display, longer for a less precise screen update time
}

void updateData(SSD1306 *display) {
    
  //display->flipScreenVertically();
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
//Current temperatures. Outside and inside.
bool drawFrame3(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  display->setFont(ArialMT_Plain_10);
  String text = wunderground.getWeatherText();
  int space = text.indexOf(" "); //show only first word as second is always cloud..
  if (space>0) text.remove(space);
  display->drawString(15 + x - text.length()/2, 5 + y, text);

  display->setFont(ArialMT_Plain_24);
  text = wunderground.getCurrentTemp() + "°";
  display->drawString(60 + x, 5 + y, text);
  int textWidth = display->getStringWidth(text);
  display->setFont(ArialMT_Plain_16);
  display->drawString(60 + x + textWidth, 5 + y + 8, "C");

   display->setFont(ArialMT_Plain_24);
  text = localTemperature + "°";
  display->drawString(60 + x, 30 + y, text);
  textWidth = display->getStringWidth(text);
  display->setFont(ArialMT_Plain_16);
  display->drawString(60 + x+ textWidth, 30 + y + 8, "C");
  
  display->setFont(Meteocons_0_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(30 + x - weatherIconWidth / 2, 20 + y, weatherIcon);
}
//Current humidities. Outside and inside.
bool drawFrame4(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  display->setFont(ArialMT_Plain_10);
  display->drawString(5 + x, 25 + y, "Humidity");
  display->drawString(23 + x, 40 + y, "In");
  String rain = wunderground.getPrecipitation1Hour();
  String humid = wunderground.getHumidity();
  rain.remove(rain.length()-2);
  float rainNumber = rain.toFloat();
  display->setFont(ArialMT_Plain_10);
  int textWidth;
  if (rainNumber>0) { //If rain in last hour display rain otherwise humidity
    display->drawString(18 + x, y, "Rain");
    display->drawString(3 + x, 11 + y, "Last Hour");
    display->setFont(ArialMT_Plain_24);
    display->drawString(60 + x, 5 + y, rain);
    textWidth = display->getStringWidth(rain);
    display->setFont(ArialMT_Plain_10);
    display->drawString(60 + x + textWidth, 5 + y + 11, " mm"); 
     
  }
  else {
    display->drawString(20 + x, 10 + y, "Out");
    display->setFont(ArialMT_Plain_24);
    humid.remove((humid.length()-1));
    display->drawString(60 + x, 5 + y, humid);
    textWidth = display->getStringWidth(humid);
    display->setFont(ArialMT_Plain_16);
    display->drawString(60 + x + textWidth, 5 + y + 8, " %");
  }

   display->setFont(ArialMT_Plain_24);
   humid = localHumidity;
  display->drawString(60 + x, 30 + y, humid);
  textWidth = display->getStringWidth(humid);
  display->setFont(ArialMT_Plain_16);
  display->drawString(60 + x + textWidth, 30 + y + 8, " %");
}

/*bool drawFrame3(SSD1306 *display, SSD1306UiState* state, int x, int y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(32 + x, 0 + y, "Humidity");
  display->drawString(96 + x, 0 + y, "Pressure");
  display->drawString(32 + x, 28 + y, "Precipit.");

  display->setFont(ArialMT_Plain_16);
  display->drawString(32 + x, 10 + y, wunderground.getHumidity());
  display->drawString(96 + x, 10 + y, wunderground.getPressure());
  display->drawString(32 + x, 38 + y, wunderground.getPrecipitationToday());
}*/
// 3 day forecast
bool drawFrame2(SSD1306 *display, SSD1306UiState* state, int x, int y) {
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



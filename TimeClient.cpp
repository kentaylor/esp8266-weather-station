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

#include "TimeClient.h"

TimeClient::TimeClient(float utcOffset) {
  myUtcOffset = utcOffset;
}

void TimeClient::updateTime() {
  HTTPClient http;

  String url = "http://www.google.com/";
  const char * headerkeys[] = {"Date"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  // Based on Arduino core BasicHttpClient.ino example
  // https://github.com/esp8266/Arduino/blob/1588b45a8a15e4d3f1b42f052fc41590e9bec0bb/libraries/ESP8266HTTPClient/examples/BasicHttpClient/BasicHttpClient.ino
  // configure http client and url
  http.begin(url); //HTTP
  http.collectHeaders(headerkeys,headerkeyssize);
  // start connection and send HTTP header
  int httpCode = http.GET();
  // httpCode will be negative on error
  if(httpCode > 0) {
	  // HTTP header has been send and Server response header has been handled
	  USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
	  // file found at server
	  if ((httpCode == HTTP_CODE_OK)|| (httpCode == HTTP_CODE_FOUND)) {
		String payload = http.header("Date");
		USE_SERIAL.println(payload);
		http.end();
        payload.toUpperCase();
        // example:
      	// date: Thu, 19 Nov 2015 20:25:40 GMT
      	if (payload!=NULL) {
          Serial.println(payload.substring(17, 19) + ":" + payload.substring(20, 22) + ":" +payload.substring(23, 25));
          int parsedHours = payload.substring(17, 19).toInt();
          int parsedMinutes = payload.substring(20, 22).toInt();
          int parsedSeconds = payload.substring(23, 25).toInt();
          Serial.println(String(parsedHours) + ":" + String(parsedMinutes) + ":" + String(parsedSeconds));
          localEpoc = (parsedHours * 60 * 60 + parsedMinutes * 60 + parsedSeconds);
          Serial.println(localEpoc);
          localMillisAtUpdate = millis();
        }
      }
  }
}

String TimeClient::getHours() {
    if (localEpoc == 0) {
      return "--";
    }
    int hours = ((getCurrentEpochWithUtcOffset()  % 86400L) / 3600) % 24;
    if (hours < 10) {
      return "0" + String(hours);
    }
    return String(hours); // print the hour (86400 equals secs per day)

}
String TimeClient::getMinutes() {
    if (localEpoc == 0) {
      return "--";
    }
    int minutes = ((getCurrentEpochWithUtcOffset() % 3600) / 60);
    if (minutes < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      return "0" + String(minutes);
    }
    return String(minutes);
}
String TimeClient::getSeconds() {
    if (localEpoc == 0) {
      return "--";
    }
    int seconds = getCurrentEpochWithUtcOffset() % 60;
    if ( seconds < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      return "0" + String(seconds);
    }
    return String(seconds);
}

String TimeClient::getFormattedTime() {
  return getHours() + ":" + getMinutes() + ":" + getSeconds();
}

long TimeClient::getCurrentEpoch() {
  return localEpoc + ((millis() - localMillisAtUpdate) / 1000);
}

long TimeClient::getCurrentEpochWithUtcOffset() {
  return round(getCurrentEpoch() + 3600 * myUtcOffset + 86400L) % 86400L;
}



#include "ThingspeakClient.h"


ThingspeakClient::ThingspeakClient() {

}

void ThingspeakClient::getLastChannelItem(String channelId, String readApiKey) {
  JsonStreamingParser parser;
  parser.setListener(this);
  HTTPClient http;

  // http://api.thingspeak.com/channels/CHANNEL_ID/feeds.json?results=2&api_key=API_KEY
  String url = "http://api.thingspeak.com/channels/" + channelId +"/feeds.json?results=1&api_key=" + readApiKey;
  // Based on Arduino core BasicHttpClient.ino example
  // https://github.com/esp8266/Arduino/blob/1588b45a8a15e4d3f1b42f052fc41590e9bec0bb/libraries/ESP8266HTTPClient/examples/BasicHttpClient/BasicHttpClient.ino
  // configure http client and url
  http.begin(url); //HTTP
  // start connection and send HTTP header
  int httpCode = http.GET();
  // httpCode will be negative on error
  if(httpCode > 0) {
	  // HTTP header has been send and Server response header has been handled
	  USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
	  // file found at server
	  if(httpCode == HTTP_CODE_OK) {
		  String payload = http.getString();
		  USE_SERIAL.println(payload);
		  http.end();
		  int i = 0;
		  char c;
		  boolean isBody = false;
		  int length = payload.length();
		  while ((i)<length) {
		    c = payload.charAt(i);
		    if (c == '{' || c == '[') {
			  isBody = true;
		    }
		    if (isBody) {
			  parser.parse(c);
		    }
		  i=i+1;
		  }
      } else {
          USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
   }
}

void ThingspeakClient::whitespace(char c) {

}

void ThingspeakClient::startDocument() {

}

void ThingspeakClient::key(String key) {
  if (key == "channel") {
    isHeader = true;
  } else if (key == "feeds") {
    isHeader = false;
  }
  currentKey = key;
}

void ThingspeakClient::value(String value) {
    //Serial.println(currentKey +": " + value);

      for (int i = 1; i < 9; i++) {
        String fieldKey = "field" + String(i);

        if (currentKey == fieldKey) {
          if (isHeader) {
            fieldLabels[i-1] = value;
          } else {
            lastFields[i-1] = value;
            Serial.println(fieldKey + ": " + value);
          }

        }
      }


}


String ThingspeakClient::getFieldLabel(int index) {
  return fieldLabels[index];
}

String ThingspeakClient::getFieldValue(int index) {
  return lastFields[index];
}

String ThingspeakClient::getCreatedAt() {
  return createdAt;
}

void ThingspeakClient::endArray() {

}

void ThingspeakClient::endObject() {

}

void ThingspeakClient::endDocument() {

}

void ThingspeakClient::startArray() {

}

void ThingspeakClient::startObject() {

}

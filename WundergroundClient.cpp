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

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define USE_SERIAL Serial
#include "WundergroundClient.h"

WundergroundClient::WundergroundClient(boolean _isMetric) {
  isMetric = _isMetric;
}

void WundergroundClient::updateConditions(String apiKey, String language, String country, String city) {
  isForecast = false;
  doUpdate("http://api.wunderground.com/api/" + apiKey + "/conditions/lang:" + language + "/q/" + country + "/" + city + ".json");
}

void WundergroundClient::updateForecast(String apiKey, String language, String country, String city) {
  isForecast = true;
  doUpdate("http://api.wunderground.com/api/" + apiKey + "/forecast10day/lang:" + language + "/q/" + country + "/" + city + ".json");
}

void WundergroundClient::doUpdate(String url) {
  JsonStreamingParser parser;
  parser.setListener(this);
  HTTPClient http;
//Based on streaming example at https://github.com/esp8266/Arduino/blob/1588b45a8a15e4d3f1b42f052fc41590e9bec0bb/libraries/ESP8266HTTPClient/examples/StreamHttpClient/StreamHttpClient.ino
//Main difference is that characters are read one at time then parsed.
//Must use stream as Wunderground documents can be too big to hold in memory.
	Serial.print("Requesting URL: ");
	Serial.println(url);
	http.begin(url); //HTTP
	int httpCode = http.GET();
	if(httpCode > 0) {
		// HTTP header has been send and Server response header has been handled
		USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

		// file found at server
		if(httpCode == HTTP_CODE_OK) {

			// get length of document (is -1 when Server sends no Content-Length header)
			int len = http.getSize();

			// create buffer for read
			char buff;
			char *buffer = &buff;

			// get tcp stream
			WiFiClient * stream = http.getStreamPtr();
            bool isBody = false;
			// read all data from server
			while(http.connected() && (len > 0 || len == -1)) {
				// get available data size
				size_t size = stream->available();

				if(size) {
					// read 1 byte
					int c = stream->readBytes(buffer, 1);//((size > sizeof(buffer)) ? sizeof(buffer) : size));

					// write it to Serial
					//USE_SERIAL.print(buff);
					if (buff == '{' || buff == '[') {
					  isBody = true;
		   		    }
					if (isBody) {
					  parser.parse(buff);
  		            }
					if(len > 0) {
						len -= c;
					}
				}
				delay(1);
			}

			USE_SERIAL.println();
			USE_SERIAL.print("[HTTP] connection closed or file end.\n");

		}
	} else {
		USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
	}

	http.end();
}

void WundergroundClient::whitespace(char c) {
  Serial.println("whitespace");
}

void WundergroundClient::startDocument() {
  Serial.println("start document");
}

void WundergroundClient::key(String key) {
  currentKey = String(key);
  if (currentKey == "txt_forecast") {
    isSimpleForecast = false;
  }
  if (currentKey == "simpleforecast") {
    isSimpleForecast = true;
  }
}

void WundergroundClient::value(String value) {
  if (currentKey == "local_epoch") {
    localEpoc = value.toInt();
    localMillisAtUpdate = millis();
  }
  if (currentKey == "observation_time_rfc822") {
    date = value.substring(0, 16);
  }
  if (currentKey == "temp_f" && !isMetric) {
    currentTemp = value;
  }
  if (currentKey == "temp_c" && isMetric) {
    currentTemp = value;
  }
  if (currentKey == "icon") {
    if (isForecast && !isSimpleForecast && currentForecastPeriod < MAX_FORECAST_PERIODS) {
      Serial.println(String(currentForecastPeriod) + ": " + value + ":" + currentParent);
      forecastIcon[currentForecastPeriod] = value;
    }
    if (!isForecast) {
      weatherIcon = value;
    }
  }
  if (currentKey == "weather") {
    weatherText = value;
  }
  if (currentKey == "relative_humidity") {
    humidity = value;
  }
  if (currentKey == "pressure_mb" && isMetric) {
    pressure = value + "mb";
  }
  if (currentKey == "pressure_in" && !isMetric) {
    pressure = value + "in";
  }
  if (currentKey == "precip_today_metric" && isMetric) {
    precipitationToday = value + "mm";
  }
  if (currentKey == "precip_today_in" && !isMetric) {
    precipitationToday = value + "in";
  }
  if (currentKey == "precip_1hr_metric" && isMetric) {
      precipitation1Hour = value + "mm";
    }
    if (currentKey == "precip_1hr_in" && !isMetric) {
      precipitation1Hour = value + "in";
  }
  if (currentKey == "period") {
    currentForecastPeriod = value.toInt();
  }
  if (currentKey == "title" && currentForecastPeriod < MAX_FORECAST_PERIODS) {
      Serial.println(String(currentForecastPeriod) + ": " + value);
      forecastTitle[currentForecastPeriod] = value;
  }
  // The detailed forecast period has only one forecast per day with low/high for both
  // night and day, starting at index 1.
  int dailyForecastPeriod = (currentForecastPeriod - 1) * 2;

  if (currentKey == "fahrenheit" && !isMetric && dailyForecastPeriod < MAX_FORECAST_PERIODS) {

      if (currentParent == "high") {
        forecastHighTemp[dailyForecastPeriod] = value;
      }
      if (currentParent == "low") {
        forecastLowTemp[dailyForecastPeriod] = value;
      }
  }
  if (currentKey == "celsius" && isMetric && dailyForecastPeriod < MAX_FORECAST_PERIODS) {

      if (currentParent == "high") {
        Serial.println(String(currentForecastPeriod)+ ": " + value);
        forecastHighTemp[dailyForecastPeriod] = value;
      }
      if (currentParent == "low") {
        forecastLowTemp[dailyForecastPeriod] = value;
      }
  }
}

void WundergroundClient::endArray() {

}


void WundergroundClient::startObject() {
  currentParent = currentKey;
}

void WundergroundClient::endObject() {
  currentParent = "";
}

void WundergroundClient::endDocument() {

}

void WundergroundClient::startArray() {

}


String WundergroundClient::getHours() {
    if (localEpoc == 0) {
      return "--";
    }
    int hours = (getCurrentEpoch()  % 86400L) / 3600 + gmtOffset;
    if (hours < 10) {
      return "0" + String(hours);
    }
    return String(hours); // print the hour (86400 equals secs per day)

}
String WundergroundClient::getMinutes() {
    if (localEpoc == 0) {
      return "--";
    }
    int minutes = ((getCurrentEpoch() % 3600) / 60);
    if (minutes < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      return "0" + String(minutes);
    }
    return String(minutes);
}
String WundergroundClient::getSeconds() {
    if (localEpoc == 0) {
      return "--";
    }
    int seconds = getCurrentEpoch() % 60;
    if ( seconds < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      return "0" + String(seconds);
    }
    return String(seconds);
}
String WundergroundClient::getDate() {
  return date;
}
long WundergroundClient::getCurrentEpoch() {
  return localEpoc + ((millis() - localMillisAtUpdate) / 1000);
}

String WundergroundClient::getCurrentTemp() {
  return currentTemp;
}

String WundergroundClient::getWeatherText() {
  return weatherText;
}

String WundergroundClient::getHumidity() {
  return humidity;
}

String WundergroundClient::getPressure() {
  return pressure;
}

String WundergroundClient::getPrecipitationToday() {
  return precipitationToday;
}

String WundergroundClient::getPrecipitation1Hour() {
  return precipitation1Hour;
}

String WundergroundClient::getTodayIcon() {
  return getMeteoconIcon(weatherIcon);
}

String WundergroundClient::getForecastIcon(int period) {
  return getMeteoconIcon(forecastIcon[period]);
}

String WundergroundClient::getForecastTitle(int period) {
  return forecastTitle[period];
}

String WundergroundClient::getForecastLowTemp(int period) {
  return forecastLowTemp[period];
}

String WundergroundClient::getForecastHighTemp(int period) {
  return forecastHighTemp[period];
}

String WundergroundClient::getMeteoconIcon(String iconText) {
  if (iconText == "chanceflurries") return "F";
  if (iconText == "chancerain") return "Q";
  if (iconText == "chancesleet") return "W";
  if (iconText == "chancesnow") return "V";
  if (iconText == "chancetstorms") return "S";
  if (iconText == "clear") return "B";
  if (iconText == "cloudy") return "Y";
  if (iconText == "flurries") return "F";
  if (iconText == "fog") return "M";
  if (iconText == "hazy") return "E";
  if (iconText == "mostlycloudy") return "Y";
  if (iconText == "mostlysunny") return "H";
  if (iconText == "partlycloudy") return "H";
  if (iconText == "partlysunny") return "J";
  if (iconText == "sleet") return "W";
  if (iconText == "rain") return "R";
  if (iconText == "snow") return "W";
  if (iconText == "sunny") return "B";
  if (iconText == "tstorms") return "0";

  if (iconText == "nt_chanceflurries") return "F";
  if (iconText == "nt_chancerain") return "7";
  if (iconText == "nt_chancesleet") return "#";
  if (iconText == "nt_chancesnow") return "#";
  if (iconText == "nt_chancetstorms") return "&";
  if (iconText == "nt_clear") return "2";
  if (iconText == "nt_cloudy") return "Y";
  if (iconText == "nt_flurries") return "9";
  if (iconText == "nt_fog") return "M";
  if (iconText == "nt_hazy") return "E";
  if (iconText == "nt_mostlycloudy") return "5";
  if (iconText == "nt_mostlysunny") return "3";
  if (iconText == "nt_partlycloudy") return "4";
  if (iconText == "nt_partlysunny") return "4";
  if (iconText == "nt_sleet") return "9";
  if (iconText == "nt_rain") return "7";
  if (iconText == "nt_snow") return "#";
  if (iconText == "nt_sunny") return "4";
  if (iconText == "nt_tstorms") return "&";

  return ")";
}

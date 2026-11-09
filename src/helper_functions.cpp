/* contains all scripts to get weather data and write data to watch screen */
#include "helper_functions.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "RTClib.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <secrets.h>
 
unsigned long weather_load_time; // records the last time the weather data was downloaded

unsigned long get_entries(RTC_DS3231& rtc) {

  //begin wifi connection
  WiFi.begin(ssid, wifi_pass);
  int attempts = 0;
  bool connected = false;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting...");
    delay(500);
    attempts ++;

    if (attempts > 10) {
      Serial.println("could not connect");
      return 0;
    }
  }
  Serial.println("Wifi connected");

  // connect http
  HTTPClient http;
  http.begin(api_link);
  int http_code = http.GET();

  if (http_code == 200) {
    String json_data = http.getString();
    JsonDocument doc;
    deserializeJson(doc, json_data);
    JsonArray list = doc["list"];

    forecastCount = 0;
    for (JsonObject entry : list) {
      if (forecastCount >= 40) break;

      forecast[forecastCount].dt = entry["dt"];
      forecast[forecastCount].temp = entry["main"]["temp"];
      forecast[forecastCount].feelsLike = entry["main"]["feels_like"];
      forecast[forecastCount].humidity = entry["main"]["humidity"];
      forecast[forecastCount].pop = entry["pop"];
      forecast[forecastCount].windSpeed = entry["wind"]["speed"];
      strlcpy(forecast[forecastCount].desc, entry["weather"][0]["main"] | "N/A", 20);
      strlcpy(forecast[forecastCount].icon, entry["weather"][0]["icon"] | "01d", 5);

      forecastCount++;
    }
    weatherLoaded = true;
    Serial.printf("Stored %d forecast entries\n", forecastCount);
  } else {
    Serial.printf("HTTP error: %d\n", http_code);
  }
  
  http.end();
  WiFi.disconnect(true);

  //get time of the first set of data loaded
  DateTime loaded_at = rtc.now();
  return loaded_at.unixtime();

}

// Get closest forecast to a given time
ForecastEntry* getClosestForecast(unsigned long targetTime) {

  if (forecastCount == 0) return nullptr;
  
  // go through all entries in the forecast and find the closest one
  for (int i = 0; i < forecastCount - 1; i++) {
    if (forecast[i + 1].dt > targetTime) {
      return &forecast[i];
    }
  }

  return &forecast[forecastCount - 1];

}

// function to write the current time to the screen
int display_time(TFT_eSprite& timeSprite, DateTime now) {

    // draw the string to the sprite
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", now.hour(), now.minute());

    timeSprite.fillSprite(TFT_GREEN);
    timeSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    timeSprite.setTextSize(5);
    timeSprite.setTextDatum(MC_DATUM);
    timeSprite.drawString(timeStr, 75, 20);
    timeSprite.pushSprite(45, 45);

    // return the time when the time was last drawn
    return now.minute();

}

// draw seconds right underneath
unsigned long display_sec(TFT_eSprite& secSprite, DateTime now) {

  // get current sec and write it to the screen 
  char secStr[3];
  snprintf(secStr, sizeof(secStr), "%02d", now.second());

  // secSprite.fillSprite(TFT_GREEN);
  secSprite.setTextColor(TFT_WHITE, TFT_BLACK);
  secSprite.setTextSize(3);
  secSprite.setTextDatum(MC_DATUM);
  secSprite.drawString(secStr, 17, 12);
  secSprite.pushSprite(160, 90);

  // return the time when the seconds were last drawn
  return now.unixtime();

}

// draw the date to the screen
int display_date(TFT_eSprite& secSprite, DateTime now, int xSpriteSize, int ySpriteSize) {

  // get current sec and write it to the screen 
  char dateStr[11];
  snprintf(dateStr, sizeof(dateStr), "%02d/%02d", now.month(), now.day());

  // secSprite.fillSprite(TFT_GREEN);
  secSprite.setTextColor(TFT_WHITE, TFT_BLACK);
  secSprite.setTextSize(3);
  secSprite.setTextDatum(MC_DATUM);
  secSprite.drawString(dateStr, xSpriteSize, ySpriteSize);
  secSprite.pushSprite(45, 80);

  // return the time when the date were last drawn
  return now.day();

}

// draw the current temp in the top left corner
void display_temp(TFT_eSprite& tempSprite, float high, int xSpriteSize, int ySpriteSize) {

  char hiStr[8];
  snprintf(hiStr, sizeof(hiStr), "%.1f F", high);

  tempSprite.fillSprite(TFT_BLACK);
  tempSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
  tempSprite.setTextSize(3);
  tempSprite.setTextDatum(MC_DATUM);
  tempSprite.drawString(hiStr, xSpriteSize/2, ySpriteSize/2);
  tempSprite.pushSprite(10, 0);
  
}
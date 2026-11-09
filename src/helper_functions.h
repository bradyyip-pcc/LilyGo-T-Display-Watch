#pragma once
#include <TFT_eSPI.h>
#include "RTClib.h"


// struct that contains forecast data
struct ForecastEntry {
  unsigned long dt;
  float temp;
  float feelsLike;
  int humidity;
  float pop;
  float windSpeed;
  char desc[20];
  char icon[5];
};

// globals defined in main.cpp
extern ForecastEntry forecast[40]; // array for future forecasts
extern int forecastCount ;
extern bool weatherLoaded; // weather loaded flag
extern float mainWeather[3]; // main weather stats to be displayed on the main watch screen
// mainWeather = {daily high, daily low, max chance of rain}

// helper function prototypes
unsigned long get_entries(RTC_DS3231& rtc);
ForecastEntry* getClosestForecast(unsigned long targetTime);
int display_time(TFT_eSprite& timeSprite, DateTime now);
unsigned long display_sec(TFT_eSprite& timeSprite, DateTime now);
int display_date(TFT_eSprite& secSprite, DateTime now, int xSpriteSize, int ySpriteSize);
void display_temp(TFT_eSprite& tempSprite, float high, int xSpriteSize, int ySpriteSize);
#pragma once
#include <TFT_eSPI.h>
#include "RTClib.h"
#include "helper_functions.h"

#define backlight 4

extern int lastTime;
extern unsigned long lastSec;
extern int lastDate;
extern float lastTemp;
extern const long TZ_OFFSET;
extern float mainWeather[3];
extern TFT_eSPI tft;
extern ForecastEntry forecast[40];
extern TFT_eSprite timeSprite;
extern TFT_eSprite secSprite;
extern TFT_eSprite dateSprite;
extern TFT_eSprite tempSprite;

void screenSleep(TFT_eSPI& tft);
void mainScreen(DateTime now, int today, int minute, unsigned long now_unix);
void forecastScreen(unsigned long now_unix, TFT_eSprite& day1, TFT_eSprite& day2, TFT_eSprite& day3);

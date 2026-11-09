#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "RTClib.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "helper_functions.h"
#include "screen_behavior.h"

RTC_DS3231 rtc;
TFT_eSPI tft = TFT_eSPI();

ForecastEntry forecast[40]; // array for future forecasts
int forecastCount = 0;
bool weatherLoaded = false; // weather loaded flag
float mainWeather[3]; // main weather stats to be displayed
// mainWeather = {daily high, daily low, max chance of rain}

// sprites: can build like a buffer and be pushed to screen
TFT_eSprite timeSprite = TFT_eSprite(&tft);
TFT_eSprite secSprite = TFT_eSprite(&tft);
TFT_eSprite dateSprite = TFT_eSprite(&tft);
TFT_eSprite tempSprite = TFT_eSprite(&tft);
TFT_eSprite day1Sprite = TFT_eSprite(&tft);
TFT_eSprite day2Sprite = TFT_eSprite(&tft);
TFT_eSprite day3Sprite = TFT_eSprite(&tft);

// keep track of the last time data was drawn
int lastTime = -1;
unsigned long lastSec = -1;
int lastDate = -1;
unsigned long now_unix;
unsigned long lastForecast = -1;
float lastTemp = -999;
unsigned long lastScreenOn = 0;
const unsigned long SCREEN_TIMEOUT = 10000;   // 10 seconds
unsigned long lastActivity = 0;

DateTime now;

const unsigned long FORECAST_REFRESH = 86400;  // refresh daily
const unsigned long FORECAST_RETRY = 600;      // retry after 10 min on failure
const long TZ_OFFSET = 28800;                  // Pacific (PST) = UTC-8; RTC holds local time

// fetch forecast; on failure schedule a retry instead of hammering WiFi every loop
unsigned long refreshForecast() {
  unsigned long t = get_entries(rtc);
  if (t == 0) return rtc.now().unixtime() - FORECAST_REFRESH + FORECAST_RETRY;
  return t;
}

void setup() {

  //initialize i2c connection
  Wire.begin(21, 22); //SDA=21, SCL=22 

  //serial enable
  Serial.begin(9600);
  delay(2000);
  Serial.println("Starting...");

  //initialize tft display
  tft.init();
  tft.setRotation(1); //rotates so its landscape
  tft.setSwapBytes(true);

  //set tft colors
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);

  // if rtc does begin
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
     while (1) delay(10);
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println("this is platformio");

  // get forecast entries
  lastForecast = refreshForecast();

  // backlight
  pinMode(backlight, OUTPUT);
  digitalWrite(backlight, HIGH);

  // buttons for waking
  pinMode(35, INPUT);            // button 2 (external pull-up)
  pinMode(0, INPUT_PULLUP);      // button 1
  lastActivity = millis();

  // create sprites
  timeSprite.createSprite(145, 35);   // just big enough to support the txt
  secSprite.createSprite(37, 26);
  dateSprite.createSprite(90, 40);
  tempSprite.createSprite(110, 30);
  day1Sprite.createSprite(60, 135);
  day2Sprite.createSprite(60, 135);
  day3Sprite.createSprite(60, 135);
  
}



void loop() {

  now = rtc.now();
  now_unix = now.unixtime();
  int today = now.day();
  int minute = now.minute();

  // get new forecast every day
  if (lastForecast + FORECAST_REFRESH < now_unix) {
    lastForecast = refreshForecast();
  }

  // check button presses
  if (digitalRead(35) == LOW) {
    forecastScreen(now_unix, day1Sprite, day2Sprite, day3Sprite);
  }
  else {
    mainScreen(now, today, minute, now_unix);
  }

  Serial.println(now_unix);

  delay(100);

  // any button press while awake keeps the screen on
  if (digitalRead(35) == LOW || digitalRead(0) == LOW) {
    lastActivity = millis();
  }

  // ...your existing time/date/seconds/weather drawing...
  // timeout: sleep until a button wakes it
  if (millis() - lastActivity > SCREEN_TIMEOUT) {
    screenSleep(tft);            // returns after the button wakes it
    lastActivity = millis();     // restart the 10 s countdown
  }

}
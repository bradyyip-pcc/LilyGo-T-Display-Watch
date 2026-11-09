/* contains all functions to control screen behavior like sleep and screen switching */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "RTClib.h"
#include "screen_behavior.h"
#include "helper_functions.h"
#include "weatherSprites.h"   // only include this in one .cpp, it holds ~180 KB of arrays

// if icon colors come out wrong (swapped red/blue), flip this
#define ICON_BYTE_SWAP 0

// go into light sleep; the button on GPIO35 wakes it back up
void screenSleep(TFT_eSPI& tft) {

    // turn screen off
    digitalWrite(backlight, LOW);
    tft.writecommand(TFT_DISPOFF);
    tft.writecommand(TFT_SLPIN);

    // don't sleep while the button is still held, or it wakes instantly
    while (digitalRead(35) == LOW || digitalRead(0) == LOW) delay(10);

    gpio_wakeup_enable(GPIO_NUM_35, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);   // button 1 too
    esp_sleep_enable_gpio_wakeup();

    Serial.flush();
    esp_light_sleep_start();   // execution pauses here until the button is pressed

    // ---- everything below runs after waking ----
    // always bring the screen back, whatever the wake cause was
    gpio_wakeup_disable(GPIO_NUM_35);
    gpio_wakeup_disable(GPIO_NUM_0);

    tft.writecommand(TFT_SLPOUT);
    delay(120);                      // ST7789 needs ~120 ms after sleep-out
    tft.writecommand(TFT_DISPON);
    digitalWrite(backlight, HIGH);

    lastTime = lastDate = -1;        // force a full redraw
    lastSec = -1;
    lastTemp = -999;

    // wait for release so the same press isn't read again
    while (digitalRead(35) == LOW || digitalRead(0) == LOW) delay(10);

}

// which screen is currently showing, so each only clears the display when switched to
enum ScreenId { SCREEN_NONE, SCREEN_MAIN, SCREEN_FORECAST };
static ScreenId currentScreen = SCREEN_NONE;

// main screen
void mainScreen(DateTime now, int today, int minute, unsigned long now_unix) {

    // clear once on switching to this screen and force every sprite to redraw;
    // clearing every loop would wipe sprites that are only pushed when their value changes
    if (currentScreen != SCREEN_MAIN) {
        tft.fillScreen(TFT_BLACK);
        lastTime = lastDate = -1;
        lastSec = -1;
        lastTemp = -999;
        currentScreen = SCREEN_MAIN;
    }

    // check if you need to draw new data to the screen
  if (today != lastDate) {
    lastDate = display_date(dateSprite, now, dateSprite.width()/2, dateSprite.height()/2);
  }

  if (minute != lastTime) {
    lastTime = display_time(timeSprite, now);
  }

  if (now_unix != lastSec) {
    lastSec = display_sec(secSprite, now);
  }

  // update the weather values
  ForecastEntry* nowWeather = getClosestForecast(now_unix + TZ_OFFSET); // forecast dt is UTC

  if (nowWeather != nullptr) {
    if (nowWeather->temp != lastTemp) {
      display_temp(tempSprite, nowWeather->temp, tempSprite.width(), tempSprite.height());
      lastTemp = nowWeather->temp;
    }
  }

}

// function to find the hi, lo, and rain percentage for a day
// returns false if there is no forecast data for that day
// iconOut (at least 3 bytes) gets the 2-digit OpenWeather icon code of the entry closest to noon
static bool getStats(unsigned long now_unix, char* iconOut) {

    iconOut[0] = '\0';

    // RTC holds local time, forecast dt is UTC: find local midnight, then convert to UTC
    unsigned long startDay = (now_unix / 86400) * 86400 + TZ_OFFSET;
    unsigned long endDay = startDay + 86400;

    // keep track of high low and max rain chance
    float hi = -999;
    float lo = 999;
    float maxRain = -999;

    // the entry closest to local noon represents the day's weather icon
    unsigned long noon = startDay + 43200;
    long bestDiff = 1000000;

    // reset arr
    mainWeather[0] = mainWeather[1] = mainWeather[2] = 0;

    // iterate through forecasts to find the stats
    for (int i = 0; i < 40; i++) {

        // look through all the entries that are in the current day
        ForecastEntry entry = forecast[i];
        if (entry.dt >= startDay && entry.dt < endDay) {

            if (entry.temp > hi) {
                hi = entry.temp;
            }

            if (entry.temp < lo) {
                lo = entry.temp;
            }
            
            if (entry.pop > maxRain) {
                maxRain = entry.pop;
            }

            long diff = labs((long)entry.dt - (long)noon);
            if (diff < bestDiff) {
                bestDiff = diff;
                iconOut[0] = entry.icon[0];
                iconOut[1] = entry.icon[1];
                iconOut[2] = '\0';
            }

        }
    }

    // no entries fell in this day
    if (hi == -999) return false;

    // set the stats to the mainWeather arr in main
    mainWeather[0] = hi;
    mainWeather[1] = lo;
    mainWeather[2] = maxRain;
    return true;

}

// OpenWeather icon code (first 2 chars, day/night ignored) -> icon bitmap
struct IconEntry { const char* code; const uint8_t* map; };
static const IconEntry ICONS[] = {
    {"01", icon_01d_map}, {"02", icon_02d_map}, {"03", icon_03d_map},
    {"04", icon_04d_map}, {"09", icon_09d_map}, {"10", icon_10d_map},
    {"11", icon_11d_map}, {"13", icon_13d_map}, {"50", icon_50d_map},
};

static const uint8_t* findIcon(const char* code) {
    for (const IconEntry& e : ICONS) {
        if (strncmp(code, e.code, 2) == 0) return e.map;
    }
    return nullptr;
}

// draw a 100x100 RGB565 icon into a sprite at half size (50x50), averaging each 2x2 block
static void drawIcon(TFT_eSprite& spr, const uint8_t* map, int x, int y) {

    const int S = WEATHER_ICON_W / 2;

    for (int py = 0; py < S; py++) {
        for (int px = 0; px < S; px++) {

            uint16_t r = 0, g = 0, b = 0;
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    int idx = ((py * 2 + dy) * WEATHER_ICON_W + (px * 2 + dx)) * 2;
#if ICON_BYTE_SWAP
                    uint16_t c = (map[idx] << 8) | map[idx + 1];
#else
                    uint16_t c = map[idx] | (map[idx + 1] << 8);
#endif
                    r += c >> 11;
                    g += (c >> 5) & 0x3F;
                    b += c & 0x1F;
                }
            }

            spr.drawPixel(x + px, y + py, ((r / 4) << 11) | ((g / 4) << 5) | (b / 4));
        }
    }
}

// draw one day's icon / hi / lo / rain chance into a sprite and push it to the screen
static void drawDay(TFT_eSprite& spr, int x, unsigned long day_unix) {

    char iconCode[3];
    bool ok = getStats(day_unix, iconCode);

    char hiStr[8], loStr[8], rainStr[8];
    if (ok) {
        snprintf(hiStr, sizeof(hiStr), "%.1f", mainWeather[0]);
        snprintf(loStr, sizeof(loStr), "%.1f", mainWeather[1]);
        snprintf(rainStr, sizeof(rainStr), "%.0f%%", mainWeather[2] * 100);
    } else {
        strcpy(hiStr, "--");
        strcpy(loStr, "--");
        strcpy(rainStr, "--");
    }

    // sprite is 60px wide; size 2 text is 12px/char so up to 5 chars fit
    int cx = spr.width() / 2;
    spr.fillSprite(TFT_BLACK);
    spr.setTextSize(2);
    spr.setTextDatum(MC_DATUM);

    spr.setTextColor(TFT_YELLOW, TFT_BLACK);
    spr.drawString(hiStr, cx, 10);

    spr.setTextColor(TFT_CYAN, TFT_BLACK);
    spr.drawString(loStr, cx, 40);

    // 50x50 icon between lo and rain, centered in the 60px column
    const uint8_t* icon = ok ? findIcon(iconCode) : nullptr;
    if (icon != nullptr) {
        drawIcon(spr, icon, (spr.width() - WEATHER_ICON_W / 2) / 2, 52);
    }

    spr.setTextColor(TFT_BLUE, TFT_BLACK);
    spr.drawString(rainStr, cx, 110);

    spr.pushSprite(x, 0);
}

// give me high, low, rain percentage, and desc for today and the next 3 days and write it to the screen
void forecastScreen(unsigned long now_unix, TFT_eSprite& day1, TFT_eSprite& day2, TFT_eSprite& day3) {

    // clear the screen only when switching to this screen, not every loop (avoids flicker)
    if (currentScreen != SCREEN_FORECAST) {
        tft.fillScreen(TFT_BLACK);
        currentScreen = SCREEN_FORECAST;
    }

    // screen is 240x135; three 60px-wide columns centered, 60px apart
    TFT_eSprite* days[3] = { &day1, &day2, &day3 };
    for (int i = 0; i < 3; i++) {
        drawDay(*days[i], 30 + 60 * i, now_unix + (86400 * i));
    }

}

// stopwatch
unsigned long stopwatch(TFT_eSprite& stopwatch, unsigned long last_stop) {

    // clear screen
    tft.fillScreen(TFT_BLACK);

    // 
    // set txt to the screen
    stopwatch.setTextColor(TFT_WHITE, TFT_BLACK);
    stopwatch.setTextDatum(MC_DATUM);
    
}


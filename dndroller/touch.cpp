#include "touch.h"
#include "config.h"
#include <TFT_eSPI.h>
#include <LittleFS.h>
using namespace fs;

extern TFT_eSPI tft;

void touch_calibrate() {
  uint16_t calData[5];
  bool calDataOK = false;

  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }

  if (LittleFS.exists(CALIBRATION_FILE)) {
    if (!REPEAT_CAL) {
      File f = LittleFS.open(CALIBRATION_FILE, "r");
      if (f && f.readBytes((char *)calData, sizeof(calData)) == sizeof(calData))
        calDataOK = true;
      f.close();
    } else {
      LittleFS.remove(CALIBRATION_FILE);
    }
  }

  if (calDataOK) {
    tft.setTouch(calData);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("Touch corners as indicated");
    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
    File f = LittleFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, sizeof(calData));
      f.close();
    }
    tft.setTouch(calData);
  }
}
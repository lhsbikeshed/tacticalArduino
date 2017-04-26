#ifndef PanelDisplay_h
#define PanelDisplay_h

#include "Arduino.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

class PanelDisplay {
public:
  PanelDisplay();
  void init();
  void update();
  void powerOn();
  void powerOff();

  void setChargeRate(int bank, int rate);
  void setValue(int bank, int val);
  int getValue(int bank);
  void setName(int bank, char* name);

private:
  LiquidCrystal_I2C* lcd1;
  LiquidCrystal_I2C* lcd2;
  LiquidCrystal_I2C* lcd3;
  LiquidCrystal_I2C* lcd4;

  LiquidCrystal_I2C* screenList[4];

  int ct[4];
  int rates[4];
  String names[4];

  int blinkCt;
  int currentScreen;

  long lastUpdateTime;

  boolean powerState;
  boolean blinker;
};

#endif

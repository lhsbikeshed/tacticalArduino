#include "PanelDisplay.h"
uint8_t font1[] = {
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
};

uint8_t font2[] = {
  B011000,
  B011000,
  B011000,
  B011000,
  B011000,
  B011000,
  B011000,
  B011000,
};

uint8_t font3[] = {
  B011100,
  B011100,
  B011100,
  B011100,
  B011100,
  B011100,
  B011100,
  B011100,
};

uint8_t  font4[] = {
  B011110,
  B011110,
  B011110,
  B011110,
  B011110,
  B011110,
  B011110,
  B011110,
};

uint8_t font5[] = {
  B011111,
  B011111,
  B011111,
  B011111,
  B011111,
  B011111,
  B011111,
  B011111,
};

PanelDisplay::PanelDisplay() {
  currentScreen = 0;
  powerState = false;
}

void PanelDisplay::init() {
  lastUpdateTime = 0;
  lcd1 = new LiquidCrystal_I2C(0x21, 16, 2);  // set the LCD address to 0x27 for a 16 chars and 2 line display
  lcd2 = new LiquidCrystal_I2C(0x23, 16, 2);
  lcd3 = new LiquidCrystal_I2C(0x25, 16, 2);
  lcd4 = new LiquidCrystal_I2C(0x27, 16, 2);

  screenList[0] = lcd2;  //b2    lcd2
  screenList[1] = lcd1;  //b3   lcd1
  screenList[2] = lcd4;  //b4   lcd
  screenList[3] = lcd3; //b5   lcd34

  ct[0] = 0;
  ct[1] = 0;
  ct[2] = 0;
  ct[3] = 0;

  rates[0] = 2;
  rates[1] = 2;
  rates[2] = 2;
  rates[3] = 2;

  blinkCt = 0;
  blinker = false;

  names[0] = "LASER";
  names[1] = "LASER";
  names[2] = "LASER";
  names[3] = "EMP";

  for (int i = 0; i < 4; i++) {
    LiquidCrystal_I2C* t = screenList[i];
    t->init();
    //t->init();
    // Print a message to the LCD.
    t->backlight();
    t->setCursor(3, 0);
    String s = "STARTUP ";
    s.concat(i);
    t->print(s);
    t->createChar(0, font1);
    t->createChar(1, font2);
    t->createChar(2, font3);
    t->createChar(3, font4);
    t->createChar(4, font5);
    ct[i] = 0;
    t->noBacklight();
    t->clear();
  }
}

void PanelDisplay::update() {
  if (millis() - lastUpdateTime < 50) {
    return;
  }
  lastUpdateTime = millis();
  blinkCt += 10;
  if (blinkCt > 80) {
    blinkCt = 0;
    blinker = !blinker;
  }

  if (powerState) {
    ct[currentScreen] += rates[currentScreen];
    if (ct[currentScreen] >= 80) ct[currentScreen] = 80;

    LiquidCrystal_I2C* t = screenList[currentScreen];
    t->setCursor(0, 0);
    String s = "WEAPON: ";
    s.concat(names[currentScreen]);
    t->print(s);
    t->setCursor(0, 1);
    s = "CHARGE: ";
    t->print(s);

    if (ct[currentScreen] < 80) {
      for (int c = 0; c < ct[currentScreen] / 10; c++) {
        t->write((byte)4);
      }
      int p = ct[currentScreen] % 10;
      p /= 2;
      if (p > 0) {
        t->write((byte)(p - 1));
      }
    } else {
      if (blinker) {
        t->print("READY   ");
      } else {
        t->print("FIRE    ");
      }
    }
    //on next update cycle do the next screen
    currentScreen++;
    currentScreen %= 4;
  }
}
void PanelDisplay::powerOn() {
  if (powerState == false) {
    for (int i = 0; i < 4; i++) {
      screenList[i]->backlight();
    }
    powerState = true;
  }
}
void PanelDisplay::powerOff() {
  if (powerState == true) {
    for (int i = 0; i < 4; i++) {
      screenList[i]->noBacklight();
      screenList[i]->clear();
      ct[i] = 0;
    }
    powerState = false;
  }
}

void PanelDisplay::setChargeRate(int bank, int rate) {
  if (bank >= 0 && bank < 4) {
    rates[bank] = rate;
  }
}
void PanelDisplay::setValue(int bank, int val) {
  if (bank >= 0 && bank < 4) {
    ct[bank] = val;
    screenList[bank]->setCursor(0, 1);
    screenList[bank]->print("CHARGE:         ");
  }
}

int PanelDisplay::getValue(int bank) {
  if (bank >= 0 && bank < 4) {
    return ct[bank];
  }
}

void PanelDisplay::setName(int bank, char nameIn[]) {
  if (bank >= 0 && bank < 4) {
    names[bank] = nameIn;
  }
}

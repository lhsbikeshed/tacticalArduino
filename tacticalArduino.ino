#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "PanelDisplay.h"
#include <Keypad.h>

/*
 * Controls tactical console
 * outputs:
 * K<0-9 A-D * #> from the keypad
 * f<X> for weapon with no charge
 * F<X> for weapon that is ready
 * cX for disconnected cable X
 * CX for connected cable X
 * w/W weapons on/off
 *
 * inputs:
 * d = damage flicker
 * T for flap open
 * S for smoke machine
 * sX for smoke machine blast of X * 500 ms (x = 0-9)
 * p for power off
 * P for power on
 * C dump out the current cable state (in cX/CX format above)]
 * F Flash the strobe
 * A/a weapon charge light on/off
 * L<Bank><rate> set the weapon charge rate
 * n<bank><comma terminated string> set the name of a weapon bank
 * X clears charge levels on all banks
 * x<bank> clears charge level on given bank
 * W turn weapons panel on
 * w turn weapons Panel off
 */

 //------------------------ KEYPAD SECTION -------------------------
const byte ROWS = 4; // Four rows
const byte COLS = 4; // Three columns
// Define the Keymap
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' },
};

//-- WIRING FOR KEYPAD
// look at keypad front on
//left hand wire is 0 index in array below (pin 3 in this case
//2 ==  orang stripe
//3 = orange
//4 = blue stripe
//5 = blue
//6 = green stripe
//7 = green
//8 = brown
//9 = brown stripe
byte rowPins[ROWS] = { 36,39,38,41 };
byte colPins[COLS] = { 40,43,42,37 };

// Create the Keypad
Keypad kpd = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//------------------------- BUTTON SECTION -------------------------
//base pin for buttons on tactical panel
#define BASEPIN 44

int lastVal = 0;
long lastReadTime = 0;

//------------------------- LCD SECTION ----------------------------
PanelDisplay p;

//------------------------- cable puzzle section -------------------
//game state defs
#define STATE_START 0
#define STATE_PLAY 1
#define STATE_COMPLETE 2

//last state of the cable puzzle
long lastPanelRead = 0;
byte debounceCableState = 0;  //for 30ms debounce things
byte lastCableState = 0;      //the last confirmed read of the cables state
//cable pins -> colour map
// yellow = 0
// black = 1
// white = 2
// blue = 3
// red = 4

byte state = STATE_START;
boolean first = true;

//------------------------- other hardware stuff -------------------

#define SMOKEPIN 59
#define TACPANELPIN 12
#define TUBEPIN 7
#define STROBEPIN 8

#define COMPLIGHT1 9
#define COMPLIGHT2 10
#define SCREENCHANGEBUTTON 50
#define WEAPONSWITCH 52
#define WEAPONLIGHT 53
#define BASE_CABLE_PIN 54
//weapons toggle stuff
byte weaponSwitchState = 0;
byte lastWeaponSwitchState = 0;
boolean weaponLightState = false;

//strobe bits
long strobeTimer = 0;
boolean strobing = false;
int strobeTime = 400;

//computer lights
int compLightTimer = 200;
boolean compLightState = false;
//smoke machine triggers
boolean smoke = false;
long smokeTimer = 0;
int smokeDuration = 0;

long tacticalPanelTimer = 0;
boolean tacticalPanelSolenoid = false;

boolean lastScreenButtonState = false;
boolean screenButtonState = false;
long lastScreenButtonRead = 0;

//------------------------------- GENERAL STUFF --------------------------

//is the console powered on?
boolean poweredOn = false;
int poweredOnTimer = 320;
//toggles on and off every second, for animation purposes
boolean blinker = false;

//is decoy light blinking?
boolean decoyBlink = false;
//should the large wall tube blink?
boolean tubeBlink = false;
long tubeBlinkTimer = 0;

char buffer[10]; //serial buffer
byte bufPtr = 0;

long loopTime = 0;

void setup() {
  pinMode(STROBEPIN, OUTPUT);
  digitalWrite(STROBEPIN, HIGH);

  pinMode(COMPLIGHT1, OUTPUT);
  pinMode(COMPLIGHT2, OUTPUT);
  digitalWrite(COMPLIGHT1, HIGH);
  digitalWrite(COMPLIGHT2, HIGH);

  pinMode(WEAPONLIGHT, OUTPUT);
  digitalWrite(WEAPONLIGHT, LOW);

  pinMode(WEAPONSWITCH, INPUT);
  digitalWrite(WEAPONSWITCH, HIGH);

  pinMode(SCREENCHANGEBUTTON, INPUT);
  digitalWrite(SCREENCHANGEBUTTON, HIGH);

  //set pin modes for buttons on controller
  for (int i = 0; i < 6; i++) {
    pinMode(BASEPIN + i, INPUT);
    digitalWrite(BASEPIN + i, HIGH);
  }
  //setup the pins for the cable puzzle, inputs with pullups
  for (int i = 0; i < 5; i++) {
    pinMode(BASE_CABLE_PIN + i, INPUT);
    digitalWrite(BASE_CABLE_PIN + i, HIGH);
  }

  //push all relay pinshigh
  for (int i = 7; i < 11; i++) {
    digitalWrite(i, HIGH);

    pinMode(i, OUTPUT);
  }

  //smoke stuff
  pinMode(SMOKEPIN, OUTPUT);
  digitalWrite(SMOKEPIN, LOW);
  pinMode(TACPANELPIN, OUTPUT);
  digitalWrite(TACPANELPIN, LOW);

  //read the first set of cable states
  for (int i = 0; i < 5; i++) {
    lastCableState |= (digitalRead(BASE_CABLE_PIN + i) << i);
  }
  Serial.begin(115200);

  //initialise the lcd screens. This blocks for 5 seconds
  p.init();

  //remove me for prod
  p.powerOn();
}

void processCableState(byte in) {
  if (in == lastCableState) {
    return;
  }
  //now work out what changed
  for (int i = 0; i < 5; i++) {
    if ((in  & (1 << i)) != (lastCableState &  (1 << i))) {
      if (in & (1 << i)) {
        Serial.print("c");
      } else {
        Serial.print("C");
      }
      Serial.print(i);
      Serial.print(",");
    }
  }

  lastCableState = in;
}

//process inbound serial messages
void doSerial() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'P') {        //power on signal
      poweredOn = true;
      decoyBlink = true;
      poweredOnTimer = 320;
      digitalWrite(TUBEPIN, LOW);
    } else if (c == 'p') {  //power off signal
      poweredOn = false;
      tubeBlink = false;
      decoyBlink = false;
      digitalWrite(TUBEPIN, HIGH);
    } else if (c == 'L') {  //set the charging rates of the 4 weapon banks
      char bank = Serial.read();
      char rate = Serial.read();
      bank = bank - '0';
      rate = rate - '0';

      p.setChargeRate(bank, rate);
    } else if (c == 'X') {    //clear the charge rates on the 4 weapon banks
      p.setValue(0, 0);
      p.setValue(1, 0);
      p.setValue(2, 0);
      p.setValue(3, 0);
    } else if (c == 'x') {    //clear a single weapon bank charge level x(0-4)
      char bank = Serial.read();
      bank = bank - '0';
      p.setValue(bank, 0);
    } else if (c == 'n') {  //set a name
      char bank = Serial.read();
      bank = bank - '0';
      char name[8];
      memset(name, 32, sizeof(char) * 8);
      int pt = 0;
      char c = Serial.read();
      while (c != ',' && pt < 8) {
        name[pt] = c;
        pt++;
        c = Serial.read();
      }
      p.setName(bank, name);
    } else if (c == 'S') {  //fire some smoke, add 1500ms to the smoke counter
      smoke = true;
      smokeTimer = millis();

      smokeDuration = 1000;
      //smokeDuration > 5000 ? 5000 : smokeDuration;
    } else if (c == 's') {  //add 0-9 * 500ms of smoke to counter
      smoke = true;
      smokeTimer = millis();
      char n = Serial.read();
      if (n >= '1' && n <= '9') {
        int dur = (buffer[1] - 48) * 500;
        smokeDuration += 1500;
        //Serial.println(dur);
      } else {
        smokeDuration += 1500;
        smokeDuration > 5000 ? 5000 : smokeDuration;
      }
    } else if (c == 'T') {    //trigger the drop flap
      tacticalPanelSolenoid = true;
      tacticalPanelTimer = millis();
      strobing = true;
      strobeTimer = millis();
      strobeTime = 1000;
    } else if (c == 'Q') {    //blink the large wall tube (not currently used)
      if (poweredOn) {
        tubeBlink = true;
        tubeBlinkTimer = millis();
      }
    } else if (c == 'C') {    //dump out the current cable state
      for (int i = 0; i < 5; i++) {
        if (digitalRead(BASE_CABLE_PIN + i)) {
          Serial.print("c");
        } else {
          Serial.print("C");
        }
        Serial.print(i);
        Serial.print(",");
      }
    }

    else if (c == 'F') {
      strobing = true;
      strobeTimer = millis();
      strobeTime = 300 + random(600);
    } else if (c == 'A') {
      weaponLightState = true;
    } else if (c == 'a') {
      weaponLightState = false;
    } else if (c == 'W') {
      p.powerOn();
    } else if (c == 'w') {
      p.powerOff();
    }
  }
}

void loop() {
  loopTime = millis();
  char key = kpd.getKey();
  if (key)  // Check for a valid key.
  {
    Serial.print("K");
    Serial.print(key);
    Serial.print(",");
  }
  //do serial stuff
  doSerial();

  //update the screens
  p.update();

  //read the button states and pass to game
  int btnVal = 0;
  for (int i = 0; i < 6; i++) {
    btnVal |= digitalRead(BASEPIN + i) << i;
  }
  if (btnVal != lastVal) {
    lastVal = btnVal;
    for (int bitId = 0; bitId < 6; bitId++) {
      if (((btnVal >> bitId) & 1) == 0) {
        if (bitId >= 2 && bitId <= 5) {
          if (p.getValue(bitId - 2) == 80) {  //weapon is ready, send a fire signal
            Serial.print("F");
          } else {
            Serial.print("f");        //weapon is not ready, send a fail
          }
          Serial.print(bitId - 2);
          Serial.print(",");
        } else {                      //its not a weapon button, so send a Button command
          Serial.print("B");

          Serial.print(bitId);
          Serial.print(",");
        }
      }
    }
  }

  long currentTime = millis();
  if (poweredOn) {
    compLightTimer--;
    if (compLightTimer < 0) {
      compLightTimer = 50 + random(50);
      compLightState = !compLightState;
      if (compLightState) {
        digitalWrite(COMPLIGHT1, HIGH);
        digitalWrite(COMPLIGHT2, LOW);
      }

      else {
        digitalWrite(COMPLIGHT1, LOW);
        digitalWrite(COMPLIGHT2, HIGH);
      }
    }
  } else {
    digitalWrite(COMPLIGHT1, HIGH);
    digitalWrite(COMPLIGHT2, HIGH);
  }

  //----- strobe
  if (strobing && strobeTimer + strobeTime > currentTime) {
    digitalWrite(STROBEPIN, LOW);
  } else {
    digitalWrite(STROBEPIN, HIGH);
    strobing = false;
  }

  //--------------- tactical panel
  if (tacticalPanelSolenoid && tacticalPanelTimer + 500 > currentTime) {
    digitalWrite(TACPANELPIN, HIGH);
  } else {
    tacticalPanelSolenoid = false;
    digitalWrite(TACPANELPIN, LOW);
  }

  //----------------- conduit puzzle
  if (lastPanelRead + 30 < currentTime) {
    //debounce stuff
    lastPanelRead = currentTime;
    byte b = 0;
    for (int i = 0; i < 5; i++) {
      b |= (digitalRead(BASE_CABLE_PIN + i) << i);
    }
    //if its settled then go process it
    if (b == debounceCableState) {
      processCableState(b);
    }
    debounceCableState = b;
  }
  //-------------------- smoke machine
  if (smoke == true) {
    digitalWrite(SMOKEPIN, HIGH);
    if (smokeTimer + smokeDuration < currentTime) {
      smoke = false;
      smokeDuration = 0;
      digitalWrite(SMOKEPIN, LOW);
    }
  }

  screenButtonState = digitalRead(SCREENCHANGEBUTTON);

  if (lastScreenButtonRead + 50 < millis()) {
    screenButtonState = digitalRead(SCREENCHANGEBUTTON);
    if (lastScreenButtonState != screenButtonState) {
      //if( screenButtonState == false){
      Serial.print("S,");
      //}
      lastScreenButtonState = screenButtonState;
    }
    lastScreenButtonRead = millis();
  }
  // digitalWrite(WEAPONLIGHT, weaponLightState);  //set the weapon armed light led on or off

  byte weaponCurrent = digitalRead(WEAPONSWITCH);
  if (weaponCurrent != lastWeaponSwitchState) {
    if (weaponCurrent) {
      Serial.print("w,");
    } else {
      Serial.print("W,");
    }
    lastWeaponSwitchState = weaponCurrent;
  }

  long loopDuration = millis() - loopTime;
  loopDuration = 50 - loopDuration;
  if (loopDuration > 0) {
    delay(loopDuration);
  }
}

#include <avr/pgmspace.h>
/*
 * Controls tactical console
 * outputs:
 * ' ' for scan button
 * 0-9 for enumpad
 * f for beam with no charge
 * F for beam that is ready
 * m for decoy
 * cX for disconnected cable X
 * CX for connected cable X
 * S for screne change button
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
 */

//keymap
PROGMEM prog_uchar charMap[] = {  
  'g', '#', 'f', 'e', 'd', 'c', 'b', 'a', 
  't', 's', 'r', 'q', 'p', 'o', 'n', 'm',
  '5', '4', '3', '2', '1', ' ', 'l', 'k',
  '6', '0', '*', '9', '8', '7', '7'};

/*
orn     common led anode (i.e. +5V)
 blk     Ground
 prp     2a              buttons pl
 blu     4a              common clock               
 red     +5V      
 grn     3a              serial data for the leds     
 wht     1a              led strobe              
 yel     5y              from buttons data (output from the board)
 */
//keyboard pin defs
#define CLOCKPIN 2   //BLUE
#define DATAPIN  3     //YELLOW
#define READPIN  4    //PURPLE
#define LEDSTROBE 5   //WHITE
#define LEDDATA  6   //GREEN

//game state defs
#define STATE_START 0
#define STATE_PLAY 1
#define STATE_COMPLETE 2

//other pin defs  
//19 is A5 on UNO, which on mega is 59
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


//last state of buttons from keyboard
long lastButtonState = 0;
//led states
long ledState = 0;

//is the console powered on?
boolean poweredOn = false;
int poweredOnTimer = 320;
//time taken to charge teh layzors
int chargeTimer = 0;

//charge levels of each bank
byte beamCharge[] = {
  0,0,0,0};

//toggles on and off every second, for animation purposes
boolean blinker = false;

//is decoy light blinking?
boolean decoyBlink = false;
//should the large wall tube blink?
boolean tubeBlink = false;
long tubeBlinkTimer = 0;

//rate at which we recharge the layzors
int chargeRate = 5;




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

char buffer[10]; //serial buffer
byte bufPtr = 0;




byte state = STATE_START;
boolean first = true;

boolean smoke = false;
long smokeTimer = 0;
int smokeDuration = 0;

long tacticalPanelTimer = 0;
boolean tacticalPanelSolenoid = false;

boolean lastScreenButtonState = false;
boolean screenButtonState = false;
long lastScreenButtonRead = 0;

//get the current key status and set the LEDS at the same time
unsigned long boardStatus(unsigned long leds){
  unsigned long buttons = 0;
  byte bbit;

  digitalWrite(READPIN,HIGH);
  delayMicroseconds(20);
  digitalWrite(READPIN,LOW);

  digitalWrite(LEDSTROBE, HIGH);

  for (int i = 0; i < 32 ; i++) {
    //read the current bit from the keyboard

    bbit = digitalRead(DATAPIN);
    // Serial.print(bbit);
    buttons |= (long)bbit << i;

    digitalWrite(CLOCKPIN,HIGH);
    long mask = 1l << i;
    if (leds & mask)
      digitalWrite(LEDDATA,LOW);
    else
      digitalWrite(LEDDATA,HIGH);
    digitalWrite(CLOCKPIN,LOW);


  }
  //  Serial.println();

  digitalWrite(LEDSTROBE,LOW);

  return buttons;
}


/* process serial buffer */
void processBuffer(){

  if(buffer[0] == 'P'){    //Power on
    poweredOn = true;
    decoyBlink = true;
    poweredOnTimer = 320;
    digitalWrite(TUBEPIN, LOW);
    
  } 
  else if (buffer[0] == 'p'){  //power off
    poweredOn = false;
    tubeBlink = false;
    decoyBlink = false;
    digitalWrite(TUBEPIN, HIGH);
    ledState = 0;
    for(int b = 0; b < 4; b++){
      beamCharge[b] = 0;
    }
  }  
  else if(buffer[0] == 'd'){    //turn off decoy blink effect
    decoyBlink = false;
  } 
  else if (buffer[0] == 'D'){  //turn on decoy blink effect
    decoyBlink = true;
  } 
  else if (buffer[0] == 'S'){  //fire some smoke, add 1500ms to the smoke counter
    smoke = true;
    smokeTimer = millis();

    smokeDuration = 1000;
    //smokeDuration > 5000 ? 5000 : smokeDuration;
  } 
  else if (buffer[0] == 's'){  //add 0-9 * 500ms of smoke to counter
    smoke = true;
    smokeTimer = millis();
    if( buffer[1] >= '1' && buffer[1] <= '9'){
      int dur = (buffer[1] - 48) * 500;
      smokeDuration += 1500;
      //Serial.println(dur);
    } 
    else {
      smokeDuration += 1500;
      smokeDuration > 5000 ? 5000 : smokeDuration;
    }
  }
  else if (buffer[0] == 'L' ){ //set beamcharge rate
    switch (buffer[1]){
    case '0':
      chargeRate = 3;
      break;
    case '1':
      chargeRate = 7;
      break;
    case '2':
      chargeRate = 15;
      break;
    }
  } 
  else if (buffer[0] == 'T'){    //trigger the drop flap
    tacticalPanelSolenoid = true;
    tacticalPanelTimer = millis();
    strobing = true;
    strobeTimer = millis();
    strobeTime = 1000;
  } 
  else if (buffer[0] == 'Q'){    //blink the large wall tube (not currently used)
    if(poweredOn){
      tubeBlink = true;
      tubeBlinkTimer = millis();
    }
  } 
  else if (buffer[0] == 'C'){    //dump out the current cable state
    for(int i = 0; i < 5; i++){
      if( digitalRead(BASE_CABLE_PIN + i)){
        Serial.print("c");
      } 
      else {
        Serial.print("C");
      }
      Serial.print(i);
      Serial.print(",");
    }
  } 

  else if (buffer[0] == 'F'){
    strobing = true;
    strobeTimer = millis();
    strobeTime = 300 + random(600);
  } 
  else if (buffer[0] == 'A'){
    weaponLightState = true;
  } else if (buffer[0] == 'a'){
    weaponLightState = false;
  }



}



void setup()
{
  Serial.begin(9600);

  /* Initialize our digital pins...
   */
  pinMode(READPIN, OUTPUT);
  pinMode(CLOCKPIN, OUTPUT);
  pinMode(DATAPIN, INPUT);
  digitalWrite(DATAPIN,LOW);
  pinMode(LEDSTROBE, OUTPUT);
  pinMode(LEDDATA, OUTPUT);
  digitalWrite(CLOCKPIN, LOW);

  pinMode(STROBEPIN, OUTPUT);
  digitalWrite(STROBEPIN, HIGH);


  pinMode(COMPLIGHT1, OUTPUT);
  pinMode(COMPLIGHT2, OUTPUT);
  digitalWrite(COMPLIGHT1, HIGH);
  digitalWrite(COMPLIGHT2, HIGH);
  
  pinMode(SCREENCHANGEBUTTON, INPUT);
  digitalWrite(SCREENCHANGEBUTTON, HIGH);

  pinMode(WEAPONLIGHT, OUTPUT);
  digitalWrite(WEAPONLIGHT, LOW);
  
  pinMode(WEAPONSWITCH, INPUT);
  digitalWrite(WEAPONSWITCH, HIGH);


  ledState = 1;
  long btn = boardStatus(ledState);

  //setup the pins for the cable puzzle, inputs with pullups
  for(int i = 0; i < 5; i++){
    pinMode(BASE_CABLE_PIN + i, INPUT);
    digitalWrite(BASE_CABLE_PIN + i, HIGH);


  }

  //push all relay pinshigh
  for(int i=7; i < 11; i++){
    digitalWrite(i,HIGH);

    pinMode(i, OUTPUT);
  }

  //smoke stuff
  pinMode(SMOKEPIN, OUTPUT);
  digitalWrite(SMOKEPIN,LOW);
  lastPanelRead = millis();
  pinMode(TACPANELPIN, OUTPUT);
  digitalWrite(TACPANELPIN, LOW);

  //read the first set of cable states
  for(int i = 0; i < 5; i++){
    lastCableState |= (digitalRead(BASE_CABLE_PIN + i) << i);
  }  
}

void processCableState(byte in){
  if(in == lastCableState){
    return;
  }
  //now work out what changed
  for(int i = 0; i < 5; i++){
    if( (in  & (1 << i) )!= (lastCableState &  (1 << i))){
      if(in & ( 1 << i)){
        Serial.print("c");
      } 
      else {
        Serial.print("C");
      } 
      Serial.print(i);
      Serial.print(",");
    }        
  } 




  lastCableState = in;
}


void loop()
{
  long currentTime = millis();
  if(poweredOn){
    compLightTimer--;
    if(compLightTimer < 0){
      compLightTimer = 200 + random(100);
      compLightState = !compLightState;
      if(compLightState){
        digitalWrite(COMPLIGHT1, HIGH);
        digitalWrite(COMPLIGHT2, LOW);
      }

      else {
        digitalWrite(COMPLIGHT1, LOW);
        digitalWrite(COMPLIGHT2, HIGH);

      }
    }
  } 
  else {
    digitalWrite(COMPLIGHT1, HIGH);
    digitalWrite(COMPLIGHT2, HIGH);
  }

  //----- strobe
  if(strobing && strobeTimer + strobeTime > currentTime){
    digitalWrite(STROBEPIN, LOW);
  } 
  else {
    digitalWrite(STROBEPIN, HIGH);
    strobing = false;
  }

  //--------------- tactical panel
  if(tacticalPanelSolenoid && tacticalPanelTimer + 500 > currentTime){
    digitalWrite(TACPANELPIN, HIGH);
  } 
  else {
    tacticalPanelSolenoid = false;
    digitalWrite(TACPANELPIN, LOW);
  }

  //----------------- conduit puzzle
  if(lastPanelRead + 30 < currentTime){
    //debounce stuff
    lastPanelRead = currentTime;
    byte b = 0;
    for(int i = 0; i < 5; i++){
      b |= (digitalRead(BASE_CABLE_PIN + i) << i);
    }
    //if its settled then go process it
    if (b == debounceCableState){

      processCableState(b);
    }
    debounceCableState = b;

  }
  
  //-- scren change button
  screenButtonState = digitalRead(SCREENCHANGEBUTTON);
  
  if(lastScreenButtonRead + 50 < millis()){
      screenButtonState = digitalRead(SCREENCHANGEBUTTON);
      if(lastScreenButtonState != screenButtonState){
        if( screenButtonState == false){
          Serial.print("S,");
        }
        lastScreenButtonState = screenButtonState;
      }
      lastScreenButtonRead = millis();
  }
      
      
   
  
  
  //-------------------- serial reads --------------
  while ( Serial.available() > 0) {  // If data is available,
    char c = Serial.read();
    if(c == ','){
      processBuffer();
      bufPtr = 0;
    } 
    else {
      buffer[bufPtr] = c;
      if(bufPtr + 1 < 5){
        bufPtr++;
      } 
      else {
        bufPtr = 0;
      }

    }

  }
  //-------------------- smoke machine
  if(smoke == true){
    digitalWrite(SMOKEPIN, HIGH);
    if(smokeTimer + smokeDuration < currentTime){
      smoke = false;
      smokeDuration = 0;
      digitalWrite(SMOKEPIN, LOW);
    }
  }

 // digitalWrite(WEAPONLIGHT, weaponLightState);  //set the weapon armed light led on or off

  //------------ panel power, if off then nothing past here executes
  if(poweredOn == false) {
    ledState = 0;
    long btn = boardStatus(ledState);
    return;
  }
  
  byte weaponCurrent = digitalRead(WEAPONSWITCH);
  if(weaponCurrent != lastWeaponSwitchState){
    if(weaponCurrent){
      Serial.print("w,");
    } else {
      Serial.print("W,");
    }
    lastWeaponSwitchState = weaponCurrent;
    
  }
  
  
  //reset leds
  ledState = 0;
  //chargin teh layzors
  chargeTimer ++;
  if(chargeTimer > 20){
    chargeTimer = 0;
    blinker = !blinker;
    for (int b = 0; b < 4; b++){
      if(beamCharge[b] + chargeRate < 200){
        beamCharge[b] += chargeRate;
      } 
      else {
        beamCharge[b] = 200;
      }
    }
  }
  //update laser leds
  for (int b = 0; b < 4; b++){

    int c = beamCharge[b] / 50;
    byte v = 0;
    if(beamCharge[b] >= 0 && beamCharge[b] < 67 ){
      v = 0x01;
    } 
    else if(beamCharge[b] >= 67 && beamCharge[b] < 134 ) {
      v = 0x03;
    } 
    else if(beamCharge[b] >= 134 && beamCharge[b] < 200 ){
      v = 0x07;
    } 
    else {
      if(blinker){
        v = 15;
      } 
      else {
        v = 0;
      }
    }

    ledState |= (long)v << (b * 4);
  } 
  //and the decoy blinker
  if (decoyBlink && blinker){
    ledState |= 1l <<  18;
    ledState |= 1l <<  19;
  }
  //read the buttons and push leds
  long btn = boardStatus(ledState);
  //Serial.println(btn,BIN);
  if(btn != lastButtonState){

    for(int i = 0; i < 32; i++){
      byte b = (btn & (1l << i)) ? 1 : 0;
      byte lb = (lastButtonState & (1l << i)) ? 1 : 0; 

      if( b!= lb &&  b){
        //Serial.print(i);
        char d = pgm_read_byte_near(charMap + i); 

        if(d == ' '  || (d >= '0' && d <= '9')){

          Serial.print(d);
          Serial.print(",");
        }

        if(d == 'm'){
          Serial.print("m,");
        }

        if(d >= 'a' && d <= 'd'){
          for(int b = 0; b < 4; b++){
            if(d == 'a' + b){
              if(beamCharge[b] == 200){
                Serial.print("F,");
                beamCharge[b] = 0;
              } 
              else {
                Serial.print("f,");
              }
            }
          }
        }
      }
    }   

    lastButtonState = btn; 
  }



  delay(10);
}













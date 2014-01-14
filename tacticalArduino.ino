#include <avr/pgmspace.h>

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

#define CLOCKPIN 2   //BLUE
#define DATAPIN  3     //YELLOW
#define READPIN  4    //PURPLE
#define LEDSTROBE 5   //WHITE
#define LEDDATA  6   //GREEN

long lastButtonState = 0;
long ledState = 0;

boolean poweredOn = false;
int poweredOnTimer = 320;
int chargeTimer = 0;

byte beamCharge[] = {
  0,0,0,0};
boolean blinker = false;

boolean decoyBlink = false;
boolean tubeBlink = false;
long tubeBlinkTimer = 0;
int chargeRate = 5;
long lastPanelRead = 0;

//conduit things


byte lastCableState = 0;
byte cableState = 0;

char buffer[10]; //serial buffer
byte bufPtr = 0;

byte connectionOrder[] = {  
  2,0,1,4,3 };
//'y, s, b, p, w*/
byte currentConnection = 0;

#define STATE_START 0
#define STATE_PLAY 1
#define STATE_COMPLETE 2

#define SMOKEPIN 19
#define TACPANELPIN 12
#define TUBEPIN 7

byte state = STATE_START;
boolean first = true;

boolean smoke = false;
long smokeTimer = 0;

long tacticalPanelTimer = 0;
boolean tacticalPanelSolenoid = false;


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
    poweredOnTimer = 320;
    digitalWrite(TUBEPIN, LOW);
  } 
  else if (buffer[0] == 'p'){
    poweredOn = false;
    tubeBlink = false;
    digitalWrite(TUBEPIN, HIGH);
    ledState = 0;
    for(int b = 0; b < 4; b++){
      beamCharge[b] = 0;
    }
  }  
  else if(buffer[0] == 'd'){
    decoyBlink = false;
  } 
  else if (buffer[0] == 'D'){
    decoyBlink = true;
  } 
  else if (buffer[0] == 'S'){
    smoke = true;
    smokeTimer = millis();
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
  } else if (buffer[0] == 'T'){
     tacticalPanelSolenoid = true;
     tacticalPanelTimer = millis();
  } else if (buffer[0] == 'Q'){
    if(poweredOn){
      tubeBlink = true;
      tubeBlinkTimer = millis();
    }
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

  ledState = 1;
  long btn = boardStatus(ledState);

  for(int i = 0; i < 5; i++){
        pinMode(14 + i, INPUT);
        digitalWrite(14 + i, HIGH);

    
  }

  //push all relay pinshigh
  for(int i=7; i < 11; i++){
    digitalWrite(i,HIGH);

    pinMode(i, OUTPUT);
  }

  pinMode(SMOKEPIN, OUTPUT);
  digitalWrite(SMOKEPIN,LOW);
  lastPanelRead = millis();
  pinMode(TACPANELPIN, OUTPUT);
  digitalWrite(TACPANELPIN, LOW);
}
void fail(){
  currentConnection = 0;
  lastCableState = 0;
  Serial.print("X,");
  state = STATE_START;
}

unsigned long ct = 0;
void loop()
{

  //tactical panel
  if(tacticalPanelSolenoid && tacticalPanelTimer + 500 > millis()){
      digitalWrite(TACPANELPIN, HIGH);
  } else {
      tacticalPanelSolenoid = false;
      digitalWrite(TACPANELPIN, LOW);
  }
  
  //conduit
  if(state == STATE_START){
    byte c = 0;
    for(int i = 0; i < 5; i++){
      c |= (1 - digitalRead(14 + i)) << i;
    }

    if(c == 0){
      state = STATE_PLAY;
      if(!first){
        Serial.print("R,");
      } 
      else {
        first = false;
      }
    } 
  } 
  else if(state == STATE_PLAY){
    int curCable = connectionOrder[currentConnection];

    byte c = 0;    
    for(int i= 0; i < 5; i++){

      byte a = digitalRead(14 + i);
      //delay(10);


      if(a == LOW && a == digitalRead(14 + i)){

        c |= 1 << i;
      }
    }
    byte changes = c ^ lastCableState;
    // Serial.println(changes,BIN);
    if(changes != 0){  //it changed!

      for(int i = 0; i < 5; i++){ //find out what and how
        if( changes & 1 << i )  //this bit changed
          if(c & 1 << i)  {  //this loops cable state changed to connected
            if( curCable == i ){
              Serial.print("C");
              Serial.print(currentConnection);
              Serial.print(",");              
              currentConnection += 1;
              if(currentConnection > 4){
                state = STATE_COMPLETE;
                Serial.print("P,");
              }
            } 
            else {
              fail();
            }
          } 
      }
      lastCableState = c;
    }





  } 
  else {
    byte c = 0;
    for(int i = 0; i < 5; i++){
      c |= (1 - digitalRead(14 + i)) << i;
    }

    if(c == 0){
      state = STATE_START;
      Serial.print("R,");
    } 
  }

  //serial reads --------------
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

  if(smoke == true){
    digitalWrite(SMOKEPIN, HIGH);
    if(smokeTimer + 1500 < millis()){
      smoke = false;
      digitalWrite(SMOKEPIN, LOW);
    }
  }

  if(poweredOn == false) {
    ledState = 0;
    long btn = boardStatus(ledState);
    return;
  }


  //reset leds
  ledState = 0;

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
  for (int b = 0; b < 4; b++){

    int c = beamCharge[b] / 50;
    byte v = 0;
    if(beamCharge[b] >= 0 && beamCharge[b] < 50 ){
      v = 0x01;
    } 
    else if(beamCharge[b] >= 50 && beamCharge[b] < 100 ) {
      v = 0x03;
    } 
    else if(beamCharge[b] >= 100 && beamCharge[b] < 150 ){
      v = 0x07;
    } 
    else if(beamCharge[b] >= 150 && beamCharge[b] < 200 ){
      v = 15;
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

  if (decoyBlink && blinker){
    ledState |= 1l <<  18;
    ledState |= 1l <<  19;
  }

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
              } else {
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



//
//  Darkroom Timer v2
//  (c)2016, Antonis Maglaras
//
//  Parts:
//  Arduino, 7 segment 4 digit display (w/ TM1637) and a push-button encoder.
//
//  Schematic/How to:
//  Sometime in the near future (or not?)
//

#include <Arduino.h>
#include <TM1637Display.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
#include <EEPROM.h>

const uint8_t SEG_DONE[] = {
	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
	SEG_C | SEG_E | SEG_G,                           // n
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
	};

const uint8_t SEG_RDY[] = {
        0,                                               // blank
	SEG_E | SEG_G,                                   // r
    	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
	SEG_B | SEG_C | SEG_D | SEG_G | SEG_F            // y
	};

const uint8_t SEG_STOP[] = {
        SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,
        SEG_D | SEG_E | SEG_F | SEG_G,
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
        SEG_A | SEG_B | SEG_E | SEG_F | SEG_G
        };

const uint8_t SEG_HOLD[] = {
        SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
        SEG_D | SEG_E | SEG_F,
    	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           
        };

const uint8_t SEG_SAVE[] = {
        SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,        
        SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,
        SEG_C | SEG_D | SEG_E,
        SEG_A | SEG_D | SEG_E | SEG_F | SEG_G
        };

const uint8_t SEG_ACC[] = {
        0,
        SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,
        SEG_A | SEG_D | SEG_E | SEG_F,
        SEG_A | SEG_D | SEG_E | SEG_F
        };

const uint8_t SEG_NOAC[] = {
	SEG_C | SEG_E | SEG_G,                           
  	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   
        SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,
        SEG_A | SEG_D | SEG_E | SEG_F
        };


// 7 segment pins
#define CLK 2
#define DIO 3

// rotary encoder pins
#define APin 8
#define BPin 9
#define SWPin 10
#define SW2Pin 11

// relay pin
#define RelayPin 12


// #define DEBUG;


long value = 10;
long last = -1;

ClickEncoder *encoder;


TM1637Display display(CLK, DIO);

void timerIsr() {
  encoder->service();
}

void setup()
{
  pinMode(SW2Pin, INPUT_PULLUP);
  pinMode(RelayPin, OUTPUT);
#ifdef DEBUG  
  Serial.begin(9600);
#endif  
  display.setBrightness(0x0f);
  display.setSegments(SEG_RDY);
  delay(1000);
  value = ReadFromMem(0);
  display.setColon(false);
  encoder = new ClickEncoder(APin,BPin,SWPin,2);
  encoder->setAccelerationEnabled(ReadFromMem(2));
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr); 
}


void loop()
{
  value += encoder->getValue();
  if (value != last)
  {
    if (value<1)
      value=1;
    if (value>5*60)
      value=5*60;
    ShowTimer(value,true);
    last = value;
  }
  
  switch (encoder->getButton())
  {
    case ClickEncoder::Clicked:
      StartTimer(value);
      break;
    case ClickEncoder::Held:
      display.setSegments(SEG_SAVE);
      WriteToMem(0,value);
#ifdef DEBUG      
      Serial.print("ACC: ");
      Serial.println(encoder->getAccelerationEnabled());
      Serial.print("Time: ");
      Serial.println(value);
#endif      
      WriteToMem(2,encoder->getAccelerationEnabled());
      delay(3000);
      ShowTimer(value,true);
      break;    
    case ClickEncoder::DoubleClicked:
      encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
      if (encoder->getAccelerationEnabled())
        display.setSegments(SEG_ACC);
      else
        display.setSegments(SEG_NOAC);
      delay(3000);
      ShowTimer(value,true);  
  }
  if (digitalRead(SW2Pin) == LOW)
    StayOn();
}

void Relay(boolean mode)
{
  digitalWrite(RelayPin, mode);
}

void StayOn()
{
  while (digitalRead(SW2Pin) == LOW) { delay(1); }
  Relay(true);
  display.setSegments(SEG_HOLD);
  delay(250);
  while (digitalRead(SW2Pin)==HIGH) {delay(1); }
  display.setSegments(SEG_DONE);
  delay(3000);
  ShowTimer(value, true);
}

void ShowTimer(int timetoshow, bool colon)
{
  if (colon)
  {
    if (timetoshow>59)
      display.setColon(true);
    else
      display.setColon(false);
  }
  int minutes=timetoshow / 60;
  int seconds = timetoshow % 60;
  display.showNumberDec((minutes*100)+seconds);
}

void StartTimer(long tmptime)
{
  boolean forcedstop = false;
  unsigned long TimeLeft = tmptime*1000;
  unsigned long StartMillis = millis();
  unsigned long Elapsed = 0; 
#ifdef DEBUG 
  Serial.println();
  Serial.print("Time Left: ");
  Serial.println(tmptime*1000);
  Serial.print("Started at ");
  Serial.println(millis());
#endif  
  long tmpmillis = millis();
  boolean tick = false;
  Relay(true);
  while ((millis()<(TimeLeft+StartMillis)) && (!forcedstop))
  {      
    if (encoder->getButton() == ClickEncoder::Clicked)
      forcedstop = true;
    Elapsed = (StartMillis+TimeLeft-millis())/1000;
    ShowTimer(Elapsed,false);
    if (Elapsed>59)
    {
      if ((millis()-tmpmillis)>499)
      {
        tmpmillis=millis();
        tick = !tick;      
        display.setColon(tick);
      }
    }
    else
      display.setColon(false);    
  }
  Relay(false);
#ifdef DEBUG  
  Serial.print("Ended at ");
  Serial.println(millis());
#endif  
  if (forcedstop)
    display.setSegments(SEG_STOP);
  else
    display.setSegments(SEG_DONE);
  delay(3000);
  value = last;
  ShowTimer(value,true);
}


int ReadFromMem(byte address)
{
  byte a=EEPROM.read(address);
  byte b=EEPROM.read(address+1);

  return a*256+b;
}

void WriteToMem(byte address, int number)
{
  byte a = number/256;
  byte b = number % 256;
  EEPROM.write(address,a);
  EEPROM.write(address+1,b);
}

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MAX31865.h>

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);

// OLED FeatherWing buttons map to different pins depending on board:
#define BUTTON_A  9
#define BUTTON_B  6 // used to be 6 SREENI, 8 on other boards
#define BUTTON_C  5 // used to be 5 SREENI, 7 on other boards

Adafruit_MAX31865 thermo = Adafruit_MAX31865(4); // SREENI hardware SPI0 (25, 189, 19, 20) on Feather RP2040. Pass in CS pin only
#define RREF      430.0
#define RNOMINAL  100.0

void setup() {
  Serial.begin(115200);

  delay(250); // wait for the OLED to power up
  display.begin(0x3C, true); // Address 0x3C default

  // Set display color and orientation
  display.setTextColor(SH110X_WHITE);
  display.setRotation(1);

  // MagGenix Splash
  display.clearDisplay();
  display.setTextSize(3);
  display.print("MAG");
  display.setTextSize(2);
  display.println("GENIX");
  display.drawLine(54, 16, 111, 16, SH110X_WHITE);
  display.println("");
  display.setTextSize(1);
  display.println("SREENIVAS EADARA");
  display.println("FW 0.0.0 2023");
   
  display.display();
  delay(1000);

  // Reset text size
  display.setTextSize(1);

  // Clear the buffer.
  display.clearDisplay();
  display.display();

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  thermo.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary
  
}

volatile int state = 0;
volatile long elapsed = 0;

const int LOOP_DURATION = 10;
const long SLEEP_DURATION = 10000;

void loop() {
  // TODO USB deactivate on sleep, reactivate on wake
  if (state == -1) {
    if (!digitalRead(BUTTON_A) || !digitalRead(BUTTON_B) || !digitalRead(BUTTON_C)) {
      state = 0;
      elapsed = 0;
      // Only wake from sleep when button released, otherwise next state will be triggered in 10ms
      while (!digitalRead(BUTTON_A) || !digitalRead(BUTTON_B) || !digitalRead(BUTTON_C)) {
        delay(LOOP_DURATION);
      }
    }
  }
  else {
    if(!digitalRead(BUTTON_B) && state == 2){ // only stop recording where one was started
      state = 1;
      elapsed = 0;
      while (!digitalRead(BUTTON_B)) {
        delay(LOOP_DURATION);
      }
      Serial.print("STOP RECORDING:\t");
      Serial.println(millis());
    } 
    else if(!digitalRead(BUTTON_C) && state != 2){ // do not restart recording when one is already running
      state = 2;
      elapsed = 0;
      while (!digitalRead(BUTTON_C)){
        delay(LOOP_DURATION);
      }
      Serial.print("BEGIN RECORDING:\t");
      Serial.println(millis());
    }
    else { // state transition logic
      if (elapsed > SLEEP_DURATION){
          state -= 1;
          elapsed = 0;
          if (state == 1) {
            Serial.print("STOP RECORDING:\t");
            Serial.println(millis());
          }
      }
      else {
        elapsed+=LOOP_DURATION; // avoid overflowing elapsed! only increment if < SLEEP_DURATION
      }
    }
  }
  display.clearDisplay();
  display.setCursor(0,0);
  if(state == -1){ // should deactivate USB to save power.
    display.display();
    return;
  }
  else if(state == 0){
    display.println("THERMOMETER READY.");
    display.println("");
    display.println("");
  }
  else if(state == 1){
    display.println("RECORDING STOPPED.");
    display.println("");
    display.println("");
  }
  else if(state == 2){
    float temp = getTemp();
    display.println("STARTED RECORDING.");
    display.print("TEMP: ");
    display.println(temp);
    display.println("");
    // should activate USB if not alr activated
    // filestream should close in the same cycle like cat! otherwise holding a button may interrupt an open filestream upon next loop (b/c it uses delay())
  }

  display.println("<-- STOP  RECORDING");
  display.println("");
  display.println("<-- START RECORDING");
  display.display();
  
  delay(LOOP_DURATION);
  yield();

  // TODO add maximum to recording time and force state transition
  
}

void startUSB() {
  delay(10);
}

float getTemp() {
  uint16_t rtd = thermo.readRTD();
  return thermo.temperature(RNOMINAL, RREF);
}

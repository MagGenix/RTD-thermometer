#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MAX31865.h>

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);

// OLED FeatherWing buttons map to different pins depending on board:
#define BUTTON_A  9
#define BUTTON_B  8 // used to be 6 SREENI
#define BUTTON_C  7 // used to be 5 SREENI

Adafruit_MAX31865 thermo = Adafruit_MAX31865(25); // SREENI hardware SPI0 (25, 189, 19, 20) on Feather RP2040. Pass in CS pin only
#define RREF      430.0
#define RNOMINAL  100.0

void setup() {
  Serial.begin(115200);

  delay(250); // wait for the OLED to power up
  display.begin(0x3C, true); // Address 0x3C default

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(1000);

  // Clear the buffer.
  display.clearDisplay();
  display.display();

  display.setRotation(1);

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  // text display tests
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  thermo.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary
  
}

volatile int state = 0;
volatile long elapsed = 0;

const int LOOP_DURATION = 10;
const long SLEEP_DURATION = 10000;

void loop() {
  // TODO add sleep if inactive for > 1 min to prevent OLED burn-in! Needs USB to be deactivated (prevent battery consumption) and display written blank, then MCU sleep. PCINT on button A to wake.
  if (state == -1) {
    if (!digitalRead(BUTTON_A) || !digitalRead(BUTTON_B) || !digitalRead(BUTTON_C)) {
      state = 0;
      elapsed = 0;
    }
  }
  else {
    else if(!digitalRead(BUTTON_B)){
      state = 1;
      elapsed = 0;
    } 
    else if(!digitalRead(BUTTON_C)){
      state = 2;
      elapsed = 0;
    }
    else { // state transition logic
      if (elapsed > SLEEP_DURATION && state > -1){
          state -= 1;
          elapsed = 0;
      }
      else if (state > -1) {
        elapsed+=LOOP_DURATION; // avoid overflowing elapsed! only increment if < SLEEP_DURATION
      }
    }
  }
  display.clearDisplay();
  display.setCursor(0,0);
  // TODO make sure transitions only happen once! while button is held multiple loops occur where the button is depressed.
  if(state == -1){ // should deactivate USB to save power. Assume filestream is closed or close if open for some reason
    display.display();
    return;
  }
  else if(state == 0){ // if state != 1, close filestream
    display.println("THERMOMETER READY.");
    display.println("");
    display.println("");
  }
  else if(state == 1){ // if state != 1, close filestream
    display.println("RECORDING STOPPED.");
    display.println("");
    display.println("");
  }
  else if(state == 2){
    display.println("STARTED RECORDING.");
    display.println("TEMP:");
    display.println("");
    // should activate USB if not alr activated
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

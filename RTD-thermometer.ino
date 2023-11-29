#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MAX31865.h>
#include <string.h>

#include "SdFat.h"
#include "msc_data_logger/usbh_helper.h"

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);

// OLED FeatherWing buttons map to different pins depending on board:
#define BUTTON_A  9
#define BUTTON_B  6 // used to be 6 SREENI, 8 on other boards
#define BUTTON_C  5 // used to be 5 SREENI, 7 on other boards

Adafruit_MAX31865 thermo = Adafruit_MAX31865(4); // SREENI hardware SPI0 (25, 189, 19, 20) on Feather RP2040. Pass in CS pin only
#define RREF      430.0
#define RNOMINAL  100.0

Adafruit_USBH_MSC_BlockDevice msc_block_dev;

FatVolume fatfs;
File32 f_log;

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);

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
volatile bool is_mounted = false;

const int LOOP_DURATION = 10;
const long SLEEP_DURATION = 10000;

void loop() {
  // USB reactivate on wake
  if (state == -1) {
    if (!digitalRead(BUTTON_A) || !digitalRead(BUTTON_B) || !digitalRead(BUTTON_C)) {
      state = 0;
      elapsed = 0;
      digitalWrite(PIN_5V_EN, PIN_5V_EN_STATE);
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
    fatfs.end();
    digitalWrite(PIN_5V_EN, !PIN_5V_EN_STATE);
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
    f_log.close();
  }
  else if(state == 2){
    unsigned long time = millis() + 10;
    float temp = getTemp();
    writeTemp(temp, time);
    Serial.printf("%i\t%f\n", time, temp);
    display.println("STARTED RECORDING.");
    display.print("TEMP: ");
    display.println(temp);
    display.println("");
  }

  display.println("<-- STOP  RECORDING");
  display.println("");
  display.println("<-- START RECORDING");
  display.display();
  
  delay(LOOP_DURATION);
  yield();

  // TODO add maximum to recording time and force state transition
  
}

float getTemp() {
  return thermo.temperature(RNOMINAL, RREF);
}

void writeTemp(float temp, unsigned long time) {
  if (!is_mounted) {
    return;
  }
  String csvName = "TEMPERATURE_DATA.tsv";
  /*String newcsvName = "ERR.csv";
  for (int i = 1; i <= 256; i++) {
    newcsvName = i + csvName;
    if (!fatfs.exists(newcsvName)) {
      break;
    }
  }*/
  digitalWrite(LED_BUILTIN, HIGH);
  if (!f_log) {
    f_log = fatfs.open(csvName, O_WRITE | O_APPEND | O_CREAT);
  } else {
    f_log.printf("%i\t%f\n", time, temp);
  }
  
  //Serial.flush();
}



//------------- Core1 -------------//
void setup1() {
  // configure pio-usb: defined in usbh_helper.h
  rp2040_configure_pio_usb();

  // run host stack on controller (rhport) 1
  // Note: For rp2040 pico-pio-usb, calling USBHost.begin() on core1 will have most of the
  // host bit-banging processing works done in core1 to free up core0 for other works
  USBHost.begin(1);
}

void loop1() {
  USBHost.task();
}

//--------------------------------------------------------------------+
// TinyUSB Host callbacks
//--------------------------------------------------------------------+
bool write_complete_callback(uint8_t dev_addr, tuh_msc_complete_data_t const *cb_data) {
  (void) dev_addr;
  (void) cb_data;

  // turn off LED after write is complete
  // Note this only marks the usb transfer is complete, device can take longer to actual
  // write data to physical flash
  digitalWrite(LED_BUILTIN, LOW);

  return true;
}

extern "C"
{

// Invoked when device is mounted (configured)
void tuh_mount_cb(uint8_t daddr) {
  (void) daddr;
}

/// Invoked when device is unmounted (bus reset/unplugged)
void tuh_umount_cb(uint8_t daddr) {
  (void) daddr;
}

// Invoked when a device with MassStorage interface is mounted
void tuh_msc_mount_cb(uint8_t dev_addr) {
  // Initialize block device with MSC device address
  msc_block_dev.begin(dev_addr);

  // For simplicity this example only support LUN 0
  msc_block_dev.setActiveLUN(0);
  msc_block_dev.setWriteCompleteCallback(write_complete_callback);
  is_mounted = fatfs.begin(&msc_block_dev);

  if (is_mounted) {
    fatfs.ls(&Serial, LS_SIZE);
  } else {
    Serial.println("Failed to mount mass storage device. Make sure it is formatted as FAT");
  }
}

// Invoked when a device with MassStorage interface is unmounted
void tuh_msc_umount_cb(uint8_t dev_addr) {
  (void) dev_addr;

  // unmount file system
  is_mounted = false;
  fatfs.end();

  // end block device
  msc_block_dev.end();
}

}


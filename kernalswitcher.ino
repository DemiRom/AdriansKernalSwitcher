#include <Arduino.h>
#include <EEPROM.h>

// C64 Kernel Switcher and Restore Key Reset/Selector
// version 1.1 -- 26-March-2019
// By Adrian Black
// Cleaned up by Demetry Romanowski

// Restore key mod: https://www.breadbox64.com/blog/c64-restore-mod/

// PINMAPPING DEFINITIONS
// ----------------------------------------
#define POWER_LED 4      // Output Power LED
#define ALT_PWR_LED 13  // Output Power LED (onboard LED)
#define ADR_13 5           // Output EPROM Address Line 13
#define ADR_14 6           // Output EPROM Address Line 14
#define RESET_LINE 7     // Output to /RESET line
#define EXROM_LINE 3     // Output the /EXROM line
#define RESTORE_KEY 8    // Input Restore key
// ----------------------------------------
// PROGRAM CONSTANTS
// ----------------------------------------
#define FLASH_SPEED 75 // LED Flash delay
#define REPEAT_DELAY 500 // Used for debouncing
// ----------------------------------------
// PROGRAM VARIABLES
// ----------------------------------------
int16_t restoreDelay = 2000;  // 2000ms delay for restore key
int16_t currentROM; // which rom is select (0-3)
int16_t restoreHeld;
int16_t buttonInput; // used to return if restore is held
int16_t buttonDuration = 0; // for keeping track of how long restore is held down

boolean buttonHeld = false; // for keeping track when you are holding down 
boolean restoreKeyReleased = false; // Keeping track when the restore key is released
boolean restoreKeyHeld = false; // Keeping track if you are holding restore
boolean resetSystem = false; // keep track wheter to reset

uint32_t timeHeld; // Amount of time Restore is held down
uint32_t time; //used to keep track of millis output
uint32_t hTime; //used to keep track of millis output
uint32_t bTime; //used to keep track of bounce millis output
// -----------------------------------------
// Function defenitions
// -----------------------------------------
void flash_led(uint8_t);
void set_slot(uint8_t);
void save_slot(uint8_t);
int16_t read_button();
// -----------------------------------------

/**
 * The setup function
 **/
void setup() {
  pinMode(POWER_LED, OUTPUT);
  pinMode(ALT_PWR_LED, OUTPUT);
  pinMode(ADR_13, OUTPUT);
  pinMode(ADR_14, OUTPUT);
  pinMode(RESET_LINE, INPUT);
  pinMode(EXROM_LINE, INPUT);
  pinMode(RESTORE_KEY, INPUT);

  digitalWrite(POWER_LED, HIGH); // turn on the power LED

  digitalWrite(RESET_LINE, LOW); // keep the system reset
  pinMode(RESET_LINE, OUTPUT); // switch reset line to OUTPUT so it can hold it low
  digitalWrite(RESET_LINE, LOW); // keep the system reset

  currentROM = EEPROM.read(1);
  set_slot(currentROM);
  delay(200);
  pinMode(RESET_LINE, INPUT); // set the reset pin back to high impedance which releases the INPUT line
  delay(1000); // wait 1000ms 
  flash_led(currentROM);  // flash the power LED to show the current state
}

/**
 * Loopdy loop
 **/
void loop() {
  buttonInput = read_button(); 
  delay(500);

  time = millis(); // load the number of milliseconds the arduino has been running into variable time
  if (buttonInput == HIGH) {
    if (!buttonHeld) {
      hTime = time; 
      timeHeld = 0; 
      buttonHeld = 1;
      //restore button is pushed 
    } else { 
      timeHeld = time - hTime; // button is being held down, keep track of total time held.
    } 
  }
  if (buttonInput == LOW) {
    if (buttonHeld) {
      restoreKeyReleased = 1; 
      buttonHeld = 0; 
      hTime = millis(); 
      timeHeld = 0; //restore button not being held anymore
    } 
  }
  
  if (timeHeld > (uint32_t)restoreDelay && !restoreKeyReleased) { // do this when the time the button is held is longer than the delay and the button is released
    hTime = millis();
    
    if (restoreKeyHeld == 0) { 
      flash_led(currentROM); 
      restoreKeyHeld = 1; 
      resetSystem = 1; 
      // first time this is run, so flash the LED with current slot and reset time held. Set the holding restore variable.
    } else {
      if (currentROM < 3) { 
        currentROM++; 
        save_slot(currentROM); 
        // or you've already been holding restore, so increment the current ROM slot otherwise reset it to 0
      } else { 
        currentROM = 0; 
        save_slot(currentROM);
      }
      
      if (timeHeld > (uint32_t)restoreDelay) 
        timeHeld = 0;  // reset the time held
      
      flash_led(currentROM); //flash the LED
    }
  }
  
  if (!restoreKeyReleased) {
    //if time held greater than restore delay, reset the system, set the current rom slot, reselt the time held and holding restore
    if (resetSystem) { // on do this if the reset system has been set above
      hTime = millis();
      resetSystem = 0;
      restoreKeyHeld = 0;
      restoreKeyReleased = 0;
      digitalWrite(RESET_LINE, LOW); // keep the system reset
      digitalWrite(EXROM_LINE, LOW); // keep the EXROM line low
      pinMode(RESET_LINE, OUTPUT);
      pinMode(EXROM_LINE, OUTPUT);
      digitalWrite(RESET_LINE, LOW); // keep the system reset
      digitalWrite(EXROM_LINE, LOW); // keep the EXROM line low
      delay(50); // wait 50ms
      set_slot(currentROM); // select the appropriate kernal ROM
      delay(200); // wait 200ms before releasing RESET line
      pinMode(RESET_LINE, INPUT); // set the reset pin back to high impedance so computer boots
      delay(300); // wait 300ms before releasing EXROM line
      pinMode(EXROM_LINE, INPUT); // set the reset pin back to high impedance so computer boots
    } else { 
      //otherwise do nothing
      hTime = millis(); 
      restoreKeyReleased = 0; 
      resetSystem = 0; 
      restoreKeyHeld = 0;
    }
  }
}

/**
 * Read the button
 * @returns int16 if the button is held
 **/
int16_t read_button() {
  int16_t debounceReading = 0;
  int16_t debounceCounter = 0; // how many times we have seen new value (for debounce)
  int16_t debounceCount = 0;

  if (!digitalRead(RESTORE_KEY) && (millis() - bTime >= REPEAT_DELAY)) {
    for(size_t i = 0; i < 10; i++) {
      debounceReading = !digitalRead(RESTORE_KEY);
      (!debounceReading && debounceCounter > 0) ? debounceCounter-- : debounceCounter ++;

      // If the Input has shown the same value for long enough let's switch it
      if(debounceCounter >= debounceCount)
      {
        bTime = millis();
        debounceCounter = 0;
        restoreHeld = 1;
      }
      delay (10); // wait 10ms
    }
  } else {
    restoreHeld = 0;
  }
  return restoreHeld;
}

/**
 * Save the current ROM Selection (0-3) into EEPROM
 **/
void save_slot(uint8_t crs) {
  EEPROM.write(1, crs);
}

/**
 * Flash the LED
 **/
void flash_led(uint8_t fc) {
  for(size_t i = 0; i <= (size_t)fc + 1; i++) { 
    digitalWrite(POWER_LED, LOW); 
    digitalWrite(ALT_PWR_LED, LOW); 
    delay(FLASH_SPEED); 
    digitalWrite(POWER_LED, HIGH); 
    digitalWrite(ALT_PWR_LED, HIGH);
  }
}

/**
 * Selects the actual rom location in the EEPROM chip
 **/
void set_slot(uint8_t drs) {
  digitalWrite(ADR_13, ((drs & 0b00000001) != 0 ? HIGH : LOW ));
  digitalWrite(ADR_14, ((drs & 0b00000010) != 0 ? HIGH : LOW ));
}

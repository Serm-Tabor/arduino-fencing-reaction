
#include <Arduino.h>
#include <TM1637Display.h>
/**
 * @file reaction.ino - Measurement of reaction time for epee and foil fencing
 *
 * This file contains the main program for measuring reaction time of fencers
 * using arduino board. The program uses TM1637 display to show the result.
 *
 * The program uses interrupts to detect the reaction time. Interrupts are important to measure the reaction accurately.
 * The reaction time is measured from the moment show measurement time until the weapon is touched.
 * The start measurement time is delayed in random interval between MIN_START_TIME and MAX_START_TIME after activation.
 * The activation is enabled by pressing the weapon a few seconds.
 * The program uses different states to control the measurement process.
 * The program also supports synchronization of multiple fencers.
 *
 * Restrictions for UNO, nano and mini boards:
 *   These boards have only two interrupt pins (2, 3).
 *   So the program can be used for both weapons, but without synchronization.
 *   Or one weapon and synchronization with another board.
 *
 *
 * The board has the following connections:
 *
 *      ----
 *     |    |=====  <-- FOIL_PIN
 * ____|    |
 *          |
 * ____     |=====  <-- GND
 *     |    |
 *     |    |=====  <-- EPEE_PIN
 *      ----
 */



//
// Program Constants
//

// Definitions of segment values
#define SEG_0  0x00  ///< Empty segment
#define SEG_DP  0b10000000  ///< Decimal point

// Colon value for function showNumberDecEx in library TM1637Display.h
#define COLON_ENABLED 0b01000000
#define COLON_DISABLED 0b0


/* Measuring time in milliseconds. */
#define MAX_TIME 10000

/* Long press time in milliseconds. */
#define LONG_PRESS 2000

/* Delayed start time intervals in milliseconds. */
#define MIN_START_TIME 3000
#define MAX_START_TIME 6000

/* Animation speed in milliseconds. */
#define ANIMATION_SPEED 50

/* Pin numbers for communication with the display. */
#define CLK 7
#define DIO 6

/* Input pin for the foil. Other foil wires are connected to GND. */
#define EPEE_PIN 2      // Interrupt pin for the epee
#define FOIL_PIN 4      // Interrupt pin for the foil (NOT supported for UNO, nano and mini board!)

/* Input pin for synchronization. */
#define SYNC_PIN_INPUT  3 // Interrupt pin for synchronization
/* Output pin for synchronization. */
#define SYNC_PIN_OUTPUT 5

/* Delay time for synchronization signal. */
#define SYNC_OFF_DELAY 30

/* Control diode indicating the pressed weapon. */
#define CONTROL_DIODE 13

/* The epee pin is pressed (INPUT_PULLUP and the pin is activated by GND connection). */
#define EPEE_PRESSED LOW
/* The foil pin is pressed (INPUT_PULLUP and the pin is activated by GND connection). */
#define FOIL_PRESSED HIGH

/* Delay time of loop in milliseconds. */
#define DELAY_TIME 1


//
// Enumerations
//

/**
 * @brief Enumeration of states of the program
 */
typedef enum {
  ACTIVATION,  ///< Activation state - waiting for weapon long touch
  WAITING,  ///< Waiting state - waiting for synchronization
  MEASUREMENT,  ///< Measurement state - measuring reaction time
  RESULT,  ///< Result state - showing the result
} States;


/**
 * @brief Enumeration of fencing weapons
 */
typedef enum {
  EPEE,  ///< Epee weapon
  FOIL,  ///< Foil weapon
  NOT_SET,  ///< Weapon not set
} Weapons;


/**
 * @brief Enumeration of synchronization states
 */
typedef enum {
  SINGLE,  ///< Single mode - no synchronization
  MASTER,  ///< Master mode - synchronizing other fencers
  SLAVE,  ///< Slave mode - waiting for synchronization signal
} SyncStates;


/**
 * @brief Enumeration of reaction time results
 */
typedef enum {
    TOO_FAST = -1,  ///< Reaction time too fast
    TIME_OUT = -2,  ///< Reaction time timeout
    CHANGED_WEAPON = -3,  ///< Weapon changed during measurement
} FencerResults;



//
// Display Constants
//

/**
 * @brief Sequence of start animation
 */
const byte START_ANIMATION[6][4] = {
    {SEG_E | SEG_F, SEG_0, SEG_0, SEG_B | SEG_C},
    {SEG_F | SEG_A, SEG_0, SEG_0, SEG_C | SEG_D},
    {SEG_A, SEG_A, SEG_D, SEG_D},
    {SEG_0, SEG_A | SEG_D, SEG_A | SEG_D, SEG_0},
    {SEG_D, SEG_D, SEG_A, SEG_A},
    {SEG_D | SEG_E, SEG_0, SEG_0, SEG_A | SEG_B},
};

/**
 * @brief Length of the animation sequence
 */
const size_t ANIMATION_LENGTH = sizeof(START_ANIMATION) / sizeof(START_ANIMATION[0]);

/**
 * @brief Segments for FOIL text
 */
const byte FOIL_SEGMENTS[4] = {
    // F
    SEG_A | SEG_E | SEG_F | SEG_G,
    // O
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    // I
    SEG_E | SEG_F,
    // L
    SEG_D | SEG_E | SEG_F,
};

/**
 * @brief Segments for EPEE text
 */
const byte EPEE_SEGMENTS[4] = {
    // E
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
    // P
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,
    // E
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
    // E
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
};

/**
 * @brief Segments for TIME_OUT text
 *      - - - -
 */
const byte TIME_OUT_SEGMENTS[4] = {
    SEG_G, SEG_G, SEG_G, SEG_G
};

/**
 * @brief Segments for TOO_FAST text
 *      - 0 0 -
 */
const byte TOO_FAST_SEGMENTS[4] = {
    SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_G,
};

/**
 * @brief Segments for empty text
 */
const byte EMPTY_SEGMENTS[4] = {
    SEG_0, SEG_0, SEG_0, SEG_0
};

/**
 * @brief Segments for colon only
 *      __ : __
 */
const byte COLON_ONLY[4] = {
    SEG_DP, SEG_DP, SEG_DP, SEG_DP
};



//
// Program Variables
//

/**  The program state */
States fencer_state = RESULT;

/**  Weapon type */
Weapons fencer_weapon = NOT_SET;

/**  Synchronization type */
SyncStates sync_state = SINGLE;

/**  Measuring result. Positive value is the reaction time in milliseconds and negative value is measurement error */
short fencer_result = TIME_OUT;

/**  Delayed start interval */
unsigned long delay_interval = 0;

/**  The time stamp for the delay start. */
unsigned long delay_start_time = 0;

/**  The time stamp for measurement start time */
unsigned long measurement_start_time = 0;

/**  The time stamp for start of the long press */
unsigned long long_press_start_time = 0;

/**  True if during the long press */
bool long_press = false;

/**  Weapon state from the previous step */
bool last_weapon_state = false;

/**  Object for the display controller TM1637 */
TM1637Display display(CLK, DIO);

/**  Touch time */
volatile unsigned long touch_time = 0;

/**  Synchronization touch time */
volatile unsigned long sync_time = 0;

/**  Synchronization touch state */
volatile bool sync_touch = false;

/**  Epee touch state */
volatile bool epee_touch = false;

/**  Foil touch state */
volatile bool foil_touch = false;



//
// Program Functions
//

/**
 * @brief Initialize pins and display.
 *
 * Initializes input and output pins.
 * Attaches interrupt functions to the foil, epee and sync input pins.
 * Calls the init_weapon function.
 */
void setup() {

  // Set the mode of the pins pins
  pinMode(EPEE_PIN, INPUT_PULLUP); // foil
  pinMode(FOIL_PIN, INPUT_PULLUP); // epee
  pinMode(SYNC_PIN_INPUT, INPUT); // sync input
  pinMode(SYNC_PIN_OUTPUT, OUTPUT); // sync output
  pinMode(CONTROL_DIODE, OUTPUT); // control diode

  // Attach interrupt functions to the foil, epee and sync input pins
  attachInterrupt(digitalPinToInterrupt(SYNC_PIN_INPUT), sync_interrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(EPEE_PIN), epee_interrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FOIL_PIN), foil_interrupt, CHANGE);

  display_on();

  // Initialize the weapon
  init_weapon();
}


/**
 * @brief Change weapon and reset all states
 *
 * Resets fencer states to RESULT and sync state to SINGLE.
 * Sets fencer_result to CHANGED_WEAPON.
 * Sets fencer_weapon to the new weapon.
 * Turns off sync pin.
 *
 * @param weapon New weapon
 */
void change_weapon(Weapons weapon) {
  // Reset fencer states and sync state
  fencer_state = RESULT;
  sync_state = SINGLE;

  // Set fencer_result and fencer_weapon
  fencer_result = CHANGED_WEAPON;
  fencer_weapon = weapon;

  // Turn off sync pin
  digitalWrite(SYNC_PIN_OUTPUT, LOW);
}


/**
 * @brief Initialize the weapon
 *
 * Read the state of the pins for the epee and the foil. Then set the weapon for the program.
 *
 * @return The initialized weapon
 */
Weapons init_weapon() {
    // Read the state of the pins
    bool epee_state = digitalRead(EPEE_PIN) == EPEE_PRESSED;

    // Foil is switched on by default. So the detect of foil is during in connect weapon, not in valid hit.
    bool foil_state = digitalRead(FOIL_PIN) == LOW;

    // Check the state of the pins and set the weapon.
    if (epee_state) {
        change_weapon(EPEE);
        return EPEE;
    } else if (foil_state) {
        change_weapon(FOIL);
        return FOIL;
    }
    // Neither button is pressed, return NOT_SET
    return NOT_SET;
}


/**
 * @brief Check if the weapon has a valid hit
 *
 * Get value from epee_touch or foil_touch depending on the connected weapon.
 *
 * @return true if the weapon has a valid hit, false otherwise
 */
bool is_touched() {
    if (fencer_weapon == EPEE) {
        return epee_touch;
    }

    if (fencer_weapon == FOIL) {
        return foil_touch;
    }

    return false;
}


/**
 * @brief Get segments text for different states.
 *
 * Transform enum FencerResults into byte array representing segments.
 *
 * @param result Short integer representing the problem result.
 * @return Pointer to byte array representing segments on display.
 */
byte* get_result_problem(short result) {
    switch (result) {
        // Time out.
        case TIME_OUT: {
            return TIME_OUT_SEGMENTS;
        }
        // Too fast.
        case TOO_FAST: {
            return TOO_FAST_SEGMENTS;
        }
        // Weapon changed.
        case CHANGED_WEAPON: {
            // Return segments for current weapon.
            switch (fencer_weapon) {
                // Epee
                case EPEE: {
                    return EPEE_SEGMENTS;
                }
                // Foil
                case FOIL: {
                    return FOIL_SEGMENTS;
                }
                // Weapon not set
                default: {
                    return TIME_OUT_SEGMENTS;
                }
            }

        }
        // Default case, without info status.
        default: {
            return EMPTY_SEGMENTS;
        }
    }
}


/**
 * @brief Print the measured result in milliseconds.
 *
 * If the result is negative, the time is not displayed, but the reason for cancellation is displayed.
 *
 * @param weapon_state If it is WEAPON_PRESSED, the decimal point is displayed on the display.
 * The visual response to the raised weapon.
 */
void print_result() {

  // If the result is positive, display it in milliseconds.
  if (fencer_result > 0) {
    display.showNumberDecEx(fencer_result, (is_touched()) ? COLON_ENABLED : COLON_DISABLED);
    return;
  }

  // Display bad results (time expired or pressed before the start).
  byte data[4];
  memcpy(data, get_result_problem(fencer_result), 4);

  // If the weapon is raised, display the decimal point on the display.
  if (is_touched()) {
    // Decimal point on the display - it is displayed only when the weapon is raised.
    data[1] |= SEG_DP;
  }

  // Display the result on the display.
  display.setSegments(data);
}


/**
 * @brief Switch to the RESULT state
 *
 * Set the result to zero and switch to the RESULT state.
 */
void to_result() {
  fencer_state = RESULT;
}


/**
 * @brief Switch to the WAITING state
 *
 * Set a random delay for the delayed start, or wait for the synchronization signal in the SLAVE state.
 * Display the dvojtečka on the display.
 */
void to_waiting() {
  fencer_state = WAITING;

  digitalWrite(SYNC_PIN_OUTPUT, LOW);
  // Define the random delay for the delayed start.
  // If the current state is SLAVE, set the delay to maximum. Slave is waiting for the synchronization signal..
  delay_interval = (sync_state == SLAVE) ? MAX_START_TIME : random(MIN_START_TIME, MAX_START_TIME);

  // Set the start time of the delay
  delay_start_time = millis();
  // Set the result to zero.
  fencer_result = 0;
  // Show only colon on the display like a waiting mode
  display.setSegments(COLON_ONLY);
}


/**
 * @brief Prepare to the MEASUREMENT state and start the measurement
 *
 * If the current state is MASTER, the synchronization pin is set to HIGH.
 * The start time of the measurement is set to the current time if the current state is SLAVE.
 * Else it's set to the time when the synchronization signal was received.
 * The display is set to show the number 8888 in blinking mode.
 */
void to_measurement() {
  fencer_state = MEASUREMENT;

  // Send the synchronization signal if the current state is MASTER
  if (sync_state == MASTER) {
    digitalWrite(SYNC_PIN_OUTPUT, HIGH);
  }

  // Set the start time of the measurement. If the current state is SLAVE, the start time is from the synchronization signal
  measurement_start_time = (sync_state == SLAVE) ? sync_time : millis();

  // Show the number 8888 for better visibility the start of the measurement.
  display.showNumberDec(8888, true);
}


/**
 * Sets the fencer state to ACTIVATION and initializes the animation time.
 * Starts synchronization by setting the synchronization pin to HIGH.
 * If the sync pin is enabled, it sets the sync state to SLAVE. Otherwise, it sets it to SINGLE.
 */
void to_activation() {
  fencer_state = ACTIVATION; // Set the fencer state to ACTIVATION

  /* Start synchronization */
  digitalWrite(SYNC_PIN_OUTPUT, HIGH); // Set the synchronization pin to HIGH

  // Default state is single. If the sync pin is enabled, it will change to slave.
  // The master mod can change in activation status when sync changes to high.
  sync_state = (sync_touch) ? SLAVE : SINGLE; // Set the sync state based on the sync pin
}


/**
 * Sets the display brightness to zero
 */
void display_off() {
  display.setBrightness(0);
}


/**
 * Sets the display brightness to maximum
 */
void display_on() {
  display.setBrightness(7);
}


/**
 * Updates the display blinking animation based on the elapsed time.
 *
 * @param current_time The current time in milliseconds.
 */
void blinking_display(unsigned long current_time) {
  // Calculate the display toggle value, which toggles the display on and off
  int display_toggle = (current_time / ANIMATION_SPEED) % 2;

  // If the display toggle is 0, turn on the display
  if (display_toggle == 0) {
    display_on();
  } else {
    // If the display toggle is 1, turn off the display
    display_off();
  }
}


/**
 * Updates the waiting animation on the LED display based on the elapsed time.
 *
 * @param current_time The current time in milliseconds.
 */
void waiting_animation(unsigned long current_time) {
  // Calculate the animation frame based on the elapsed time
  int animation_frame = ((current_time) / ANIMATION_SPEED) % ANIMATION_LENGTH;

  // Display the animation frame on the LED display
  display.setSegments(START_ANIMATION[animation_frame]);
}


/**
 * @brief The main loop of the program
 *
 * It handles the state of the fencer,
 * including the waiting, measurement, and result states. It also
 * includes the logic for the activation state.
 */
void loop() {
  // Actual time on the processor in milliseconds
  unsigned long current_time = millis();

  // Check if the fencer has not been initialized
  if (fencer_weapon == NOT_SET) {
      init_weapon();
  }

  bool weapon_state = is_touched();

  // Control diode of the activated weapon
  digitalWrite(CONTROL_DIODE, (weapon_state) ? HIGH : LOW);

  switch (fencer_state) {
    case ACTIVATION: {
      // Check if the weapon is being held down
      if (weapon_state) {
        // Display waiting animation
        waiting_animation(current_time);

        // If the sync pin is not being held down while in SINGLE mode, switch to MASTER mode
        if (sync_touch && (sync_state == SINGLE)) {
          sync_state = MASTER;
        }
        break;
      }

      switch (sync_state) {
        case MASTER:
          // In MASTER mode, if synchronization touch is not detected, switch to waiting state. (wating for another fencer)
          if (!sync_touch) {
            to_waiting();
          }
          break;
        case SINGLE:
          // In SINGLE mode, no synchronization is needed, switch to waiting state
          to_waiting();
          break;
        case SLAVE:
          // In SLAVE mode, turn off the synchronization pin before switching to waiting state
          digitalWrite(SYNC_PIN_OUTPUT, LOW);
          // For synchronization of another fencer.
          // If synchronization touch is not detected, switch to waiting state
          if (!sync_touch) {
            to_waiting();
          }
          break;
      }

      break;
    }

    case WAITING: {
      // If in SLAVE mode waiting for synchronization before go to measurement mode
      if ((sync_state == SLAVE) && sync_touch) {
        to_measurement();
      }

      // Go to measurement mode after delay start.
      if (current_time - delay_start_time >= delay_interval) {
        to_measurement();
      }

      // If the weapon is pressed before the start of the measurement, go to result mode with an error TOO_FAST
      if ((weapon_state) && (weapon_state != last_weapon_state)) {
        to_result();
        fencer_result = TOO_FAST;
      }
      break;
    }

    case MEASUREMENT: {
      // Calculate the result of the measurement
      fencer_result = current_time - measurement_start_time;

      // Display the actual time of the measurement (with leading zeros for clarity)
      display.showNumberDec(fencer_result, false);

      // If in MASTER mode turn off the synchronization pin after 30 ms
      if ((sync_state == MASTER) && (fencer_result > SYNC_OFF_DELAY)) {
          digitalWrite(SYNC_PIN_OUTPUT, LOW);
      }

      // If the weapon is pressed during the measurement, go to result mode with the result of the measurement
      if ((weapon_state) && (weapon_state != last_weapon_state)) {
        to_result();
        fencer_result = touch_time - measurement_start_time;
      }

      // If the measurement time has passed, go to result mode with an error TIME_OUT
      if (current_time - measurement_start_time >= MAX_TIME) {
        to_result();
        fencer_result = TIME_OUT;
      }
      break;
    }

    case RESULT: {
      // Display the result on the LED display
      print_result();

      // Reset the long press flag
      if (!weapon_state) {
        long_press = false;
      }

      // Set the start time of the long press and set the long press flag
      if (weapon_state && (weapon_state != last_weapon_state)) {
        long_press_start_time = current_time;
        long_press = true;
      }

      // If the long press flag is set and the weapon is pressed, go to the activation state
      if (long_press && weapon_state &&
          (current_time - long_press_start_time >= LONG_PRESS)) {
        long_press = false;
        to_activation();
      }
      break;
    }
  }

  // Save the last state of the weapon for the next loop
  last_weapon_state = weapon_state;

  delay(DELAY_TIME);
}


/**
 * @brief Interrupt function for the epee
 *
 * Reads the pin of the connected weapon for the epee. The weapon is
 * activated when the pin is connected to GND, so the active state is LOW.
 */
void epee_interrupt() {
  /* Read the pin of the connected weapon for the epee */
  epee_touch = digitalRead(EPEE_PIN) == EPEE_PRESSED;

  /* If the connected weapon is the epee and it is touched, update the touch time */
  if ((fencer_weapon == EPEE) && epee_touch) {
    touch_time = millis();
  }
}


/**
 * @brief Interrupt function for the foil
 *
 * Reads the pin of the connected weapon for the foil. The foil obvod
 * releases when touched, so the active state is HIGH.
 */
void foil_interrupt() {
  /* Read the pin of the connected weapon for the foil */
  foil_touch = digitalRead(FOIL_PIN) == FOIL_PRESSED;

  /* If the connected weapon is the foil and it is touched, update the touch time */
  if ((fencer_weapon == FOIL) && foil_touch) {
    touch_time = millis();
  }
}


/**
 * @brief Interrupt function for the sync pin
 *
 * Reads the sync pin and updates the sync time if it is touched.
 */
void sync_interrupt() {
  sync_touch = digitalRead(SYNC_PIN_INPUT) == HIGH;

  /* Update the sync time only on change sync from low to high */
  if (sync_touch) {
    sync_time = millis();
  }
}

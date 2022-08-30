
#include <Arduino.h>
#include <TM1637Display.h>

typedef enum {
  ACTIVATION,
  WAITING,
  MEASUREMENT,
  RESULT,
} States;

typedef enum {
  EPEE,
  FOIL,
  NOT_SET,
} Weapons;

/*
 * Sekvence animace spuštění
 */
const byte START_ANIMATION[6][4] = {
    {SEG_E | SEG_F, 0x00, 0x00, SEG_B | SEG_C},
    {SEG_F | SEG_A, 0x00, 0x00, SEG_C | SEG_D},
    {SEG_A, SEG_A, SEG_D, SEG_D},
    {0x00, SEG_A | SEG_D, SEG_A | SEG_D, 0x00},
    {SEG_D, SEG_D, SEG_A, SEG_A},
    {SEG_D | SEG_E, 0x00, 0x00, SEG_A | SEG_B},
};

const byte FOIL_SEGMENTS[4] = {
    SEG_A | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_E | SEG_F,
    SEG_D | SEG_E | SEG_F,
};

const byte EPEE_SEGMENTS[4] = {
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
};

const byte TIME_OUT_SEGMENTS[4] = {
    SEG_G,
    SEG_G,
    SEG_G,
    SEG_G,
};

const byte TOO_FAST_SEGMENTS[4] = {
    SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_G,
};

const byte EMPTY_SEGMENTS[4] = {0x00, 0x00, 0x00, 0x00};
/* čas v ms pro dobu měření. */
#define MAX_TIME 10000

/* čas v ms pro dlouhé stisknutí */
#define LONG_PRESS 2000

/* intervaly odloženého startu v ms */
#define MIN_START_TIME 4000
#define MAX_START_TIME 10000

/* Rychlost animace v ms */
#define ANIMATION_SPEED 50

/* Piny pro komunikaci s displayem */
#define CLK 3
#define DIO 2

/* Vstupní pin pro kord. Další kabely kordu se zapojují do GND */
#define EPEE_PIN A0
#define FOIL_PIN A2

/* Kokntrolní dioda stisknutí zbraně. */
#define CONTROL_DIODE 13

/* čas výsledku pro vypršení času */
#define TIME_OUT -2
/* čas výsledku pro předčasné spuštění */
#define TOO_FAST -1
/* čas výsledku pro předčasné spuštění */
#define CHANGED_WEAPON -3

/* zbraň stiskuta (INPUT_PULLUP a pin se spína přes GND) */
#define WEAPON_PRESSED LOW
/* zbraň odpojena (INPUT_PULLUP a pin se spína přes GND) */
#define WEAPON_UNPRESSED HIGH

/* Stav programu */
States fencer_state = RESULT;
/* Stav programu */
Weapons fencer_weapon = NOT_SET;
/* Hodnota měření */
short fencer_result = TIME_OUT;
/* Interval odloženého startu */
unsigned long delay_interval = 0;
/* Čas začátku odloženého startu */
unsigned long delay_start_time = 0;
/* Čas začátku měření */
unsigned long measurement_start_time = 0;
/* Čas začátku dlouhého stisku */
unsigned long long_press_start_time = 0;
/* True, během dlouhého stisku */
bool long_press = false;
/* Čas řídící animaci při souštění */
unsigned long animation_time = 0;

/* Stav zbraně z předchozího kroku */
byte last_weapon_state = WEAPON_UNPRESSED;
/* Objekt pro display s řadičem TM1637 */
TM1637Display display(CLK, DIO);

/**
 * @brief Inincializace pinů a displaye
 */
void setup() {
  pinMode(EPEE_PIN, INPUT_PULLUP);
  pinMode(FOIL_PIN, INPUT_PULLUP);
  pinMode(CONTROL_DIODE, OUTPUT);
  display.setBrightness(4);
  Serial.begin(9600);
  last_weapon_state = check_weapon();
}

bool check_weapon() {
  /* Čtení pinu z připojené zbraně */
  bool epee = digitalRead(EPEE_PIN) == LOW;

  /* Čtení pinu z připojené zbraně */
  bool foil = digitalRead(FOIL_PIN) == HIGH;

  if (epee) {
    if (fencer_weapon != EPEE) {
      fencer_state = RESULT;
      fencer_result = CHANGED_WEAPON;
      fencer_weapon = EPEE;
      return true;
    }
  }

  if (!foil) {
    if (fencer_weapon != FOIL) {
      fencer_state = RESULT;
      fencer_result = CHANGED_WEAPON;
      fencer_weapon = FOIL;
      return false;
    }
  }

  switch (fencer_weapon) {
  case EPEE: {
    return epee;
  }
  case FOIL: {
    return foil;
  }
  case NOT_SET: {
    fencer_state = RESULT;
    fencer_result = CHANGED_WEAPON;
    fencer_weapon = EPEE;
    return false;
  }
  }

  return false;
}

/**
 * @brief Vykreslení změřeného výsledku v ms.
 * Při záporné hodnotě výsledku se nevykresluje čas, ale důvod zrušení měření.
 *
 * @param weapon_state při hodnotě WEAPON_PRESSED se vykresluje dvojtečka na
 * display. Vizuální odezva sepnuté zbraně.
 */
void print_result(bool weapon_state) {
  /* Dvojtečka na display - zobrazuje se jen, když je zbraň sepnutá. */
  byte show_colon = 0b10000000;
  if (!weapon_state) {
    show_colon = 0;
  }

  /* Zobrazení špatných výsledků (čas vypršel nebo stisnuto před startem) */
  if (fencer_result < 0) {
    byte *text;
    //        byte text[4];

    switch (fencer_result) {
      case TIME_OUT: {
        text = TIME_OUT_SEGMENTS;
        break;
      }
      case TOO_FAST: {
        text = TOO_FAST_SEGMENTS;
        break;
      }
      case CHANGED_WEAPON: {
        text = (fencer_weapon == EPEE) ? EPEE_SEGMENTS : FOIL_SEGMENTS;
        break;
      }
      default: {
        text = EMPTY_SEGMENTS;
        break;
      }
    }

    byte data[4];
    for (byte i = 0; i < 4; i++) {
      data[i] = *(text + i);
    }
    data[1] |= show_colon;
    display.setSegments(data);
  }

  /* Zobrazení správhého výsledku */
  else {
    if (weapon_state) {
      // Hodnota pro rozsvícení dvojtečky je jiná než pro metodu setSegments.
      show_colon = 0b11100000;
    }
    display.showNumberDecEx(fencer_result, show_colon);
  }
}

/**
 * @brief Hlavní smyčka programu
 */
void loop() {
  /* Aktuální čas na procesoru */
  unsigned long current_time = millis();
  /* Čtení pinu z připojené zbraně */
  // uint8_t weapon_state = digitalRead(EPEE_PIN);
  bool weapon_state = check_weapon();

  /* Kontrolní dioda sepnuté zbraně */
  digitalWrite(CONTROL_DIODE, (weapon_state) ? HIGH : LOW);

  switch (fencer_state) {
    case ACTIVATION: {
      /* Animace při stisknuté zbrani */
      if (weapon_state) {
        int animation_frime = (current_time - animation_time) / ANIMATION_SPEED;
        if (animation_frime >=
            sizeof(START_ANIMATION) / sizeof(START_ANIMATION[0])) {
          animation_frime = 0;
          animation_time = current_time;
        }
  
        display.setSegments(START_ANIMATION[animation_frime]);
        break;
      }

      /* Definování odloženého startu a přechod do stavu WAITING */
      delay_interval = random(MIN_START_TIME, MAX_START_TIME);
      delay_start_time = current_time;
      fencer_result = 0;
      fencer_state = WAITING;
      break;
    }

    case WAITING: {
      /* Rozsvvítit dvojtečku */
      uint8_t data[] = {
          0x00,
          0x80,
          0x00,
          0x00,
      };
      display.setSegments(data);
  
      /* Čas odloženého startu vypršel -> Přechod do stavu MEASUREMENT */
      if (current_time - delay_start_time >= delay_interval) {
        fencer_state = MEASUREMENT;
        measurement_start_time = current_time;
      }
  
      /* Stisknuto před zahájením měření -> Přechod do stavu RESULT s chybou
       * TOO_FAST */
      if ((weapon_state) && (weapon_state != last_weapon_state)) {
        fencer_state = RESULT;
        fencer_result = TOO_FAST;
      }
      break;
    }

    case MEASUREMENT: {
      fencer_result = current_time - measurement_start_time;
  
      /* Aktuální čas od startu měření (zobrazeno s nulama zleva pro výraznější
       * signál začátku) */
      display.showNumberDec(fencer_result, true);
  
      /* Stisknuto v intervalu -> Předhod do stavu s vísledkem. */
      if ((weapon_state) && (weapon_state != last_weapon_state)) {
        fencer_state = RESULT;
      }
  
      /* Čas vypršel -> Předhod do stavu RESULT s chybou TIME_OUT. */
      if (current_time - measurement_start_time >= MAX_TIME) {
        fencer_state = RESULT;
        fencer_result = TIME_OUT;
      }
      break;
    }

    case RESULT: {
      print_result(weapon_state);
  
      /* Rušení dlouhého stisku */
      if (!weapon_state) {
        long_press = false;
      }
  
      /* Začátek dlouhého stisku */
      if (weapon_state && (weapon_state != last_weapon_state)) {
        long_press_start_time = current_time;
        long_press = true;
      }
  
      /* PŘECHOD DO STATUSU ACTIVATION */
      if (long_press && weapon_state &&
          (current_time - long_press_start_time >= LONG_PRESS)) {
        fencer_state = ACTIVATION;
        long_press = false;
      }
      break;
    }
  }

  /* Uložení posledního stavu */
  last_weapon_state = weapon_state;
  delay(1);
}

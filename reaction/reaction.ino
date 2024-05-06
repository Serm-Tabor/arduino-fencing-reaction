
#include <Arduino.h>
#include <TM1637Display.h>


#define SEG_0  0x00
#define SEG_DP  0b10000000

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

typedef enum {
  SINGLE,
  MASTER,
  SLAVE,
} SyncStates;

/*
 * Sekvence animace spuštění
 */
const byte START_ANIMATION[6][4] = {
    {SEG_E | SEG_F, SEG_0, SEG_0, SEG_B | SEG_C},
    {SEG_F | SEG_A, SEG_0, SEG_0, SEG_C | SEG_D},
    {SEG_A, SEG_A, SEG_D, SEG_D},
    {SEG_0, SEG_A | SEG_D, SEG_A | SEG_D, SEG_0},
    {SEG_D, SEG_D, SEG_A, SEG_A},
    {SEG_D | SEG_E, SEG_0, SEG_0, SEG_A | SEG_B},
};

/* Text FOIL */
const byte FOIL_SEGMENTS[4] = {
    SEG_A | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_E | SEG_F,
    SEG_D | SEG_E | SEG_F,
};

/* Text EPEE */
const byte EPEE_SEGMENTS[4] = {
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
};

/* Text ---- */
const byte TIME_OUT_SEGMENTS[4] = {
    SEG_G,
    SEG_G,
    SEG_G,
    SEG_G,
};

/* Text --00-- */
const byte TOO_FAST_SEGMENTS[4] = {
    SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_G,
};

/* Bez textu */
const byte EMPTY_SEGMENTS[4] = {SEG_0, SEG_0, SEG_0, SEG_0};
/* čas v ms pro dobu měření. */
#define MAX_TIME 10000

/* čas v ms pro dlouhé stisknutí */
#define LONG_PRESS 2000

/* intervaly odloženého startu v ms */
#define MIN_START_TIME 3000
#define MAX_START_TIME 6000

/* Rychlost animace v ms */
#define ANIMATION_SPEED 50

/* Piny pro komunikaci s displayem */
#define CLK 3
#define DIO 2

/* Vstupní pin pro kord. Další kabely kordu se zapojují do GND */
#define EPEE_PIN A0
#define FOIL_PIN A2

/* Vstupní pin pro synchronizaci */
#define SYNC_PIN_INPUT  4
/* Výstupní pin pro synchronizaci */
#define SYNC_PIN_OUTPUT 5

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
/* Stav programu */
SyncStates sync_state = SINGLE;
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

/* Kontrolní čas pro výpočet doby cyklu. Používa se jen do Serial.print */
unsigned long testing_time = 0;
/* Počet cyklů pro výpočet doby cyklu*/
float loop_counter;

/* Stav zbraně z předchozího kroku */
byte last_weapon_state = WEAPON_UNPRESSED;
/* Objekt pro display s řadičem TM1637 */
TM1637Display display(CLK, DIO, 80);


/**
 * @brief Inincializace pinů a displaye
 */
void setup() {
  pinMode(EPEE_PIN, INPUT_PULLUP);
  pinMode(FOIL_PIN, INPUT_PULLUP);
  pinMode(SYNC_PIN_INPUT, INPUT);
  pinMode(SYNC_PIN_OUTPUT, OUTPUT);
  pinMode(CONTROL_DIODE, OUTPUT);
  display.setBrightness(7);
  Serial.begin(115200);
  last_weapon_state = check_weapon();
}


/**
 * @brief Přepnutí všech režimů do výchozího stavu. Došlo k výměně zbraní.
 *
 * @param weapon Nová zbraň
 */
void change_weapon(Weapons weapon) {
  fencer_state = RESULT;
  sync_state = SINGLE;
  fencer_result = CHANGED_WEAPON;
  fencer_weapon = weapon;
  digitalWrite(SYNC_PIN_OUTPUT, LOW);
}


/**
 * @brief Pouze testování.
 * Odešle po Serial průměrný čas jednoho cyklu od posledního voléní této funkce.
 *
 * @param current_time Aktuální čas.
 */
void test_time(unsigned long current_time) {
  float loop_time = (current_time - testing_time) / loop_counter;
  loop_counter = 0;
  testing_time = current_time;
  Serial.println(loop_time);
}


/**
 * @brief Stiskutí zbraně.
 * Funkce kontroluje piny pro kord i fleret najednou. Při změmě zbraně spustí change_weapon()
 *
 * @return true pokud je zbraň sepnutá.
 * @return false pokud není zbraň sepnutá.
 */
bool check_weapon() {
  /* Čtení pinu z připojené zbraně pro kord. Spínání je zapnuto na GDN, takže sepnutý stav je LOW */
  bool epee = digitalRead(EPEE_PIN) == LOW;

  /* Čtení pinu z připojené zbraně pro fleret. Fleret obvod rozpíná, takže sepnutým fleretem odpojíme zem. */
  bool foil = digitalRead(FOIL_PIN) == HIGH;

  if (epee) {
    /* Pin kordu je sepnutý. */
    if (fencer_weapon != EPEE) {
      change_weapon(EPEE);
      return epee;
    }
  }

  if (!foil) {
    /* Pin fleretu je sepnutý. Fleret je připojený, ale není sepnutý. */
    if (fencer_weapon != FOIL) {
      change_weapon(FOIL);
      return foil;
    }
  }

  /* Pokud je zbraň vyměněna */
  switch (fencer_weapon) {
    case EPEE: {
      return epee;
    }
    case FOIL: {
      return foil;
    }
    case NOT_SET: {
      change_weapon(EPEE);
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
  byte show_colon = SEG_DP;
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
 * @brief Přepínání do stavu WAITING
 * Náhodně zvolí interval odloženého startu. Pokud je slave, nastaví ho na MAX.
 *
 * @param current_time Aktuální čas
 */
void to_waiting(unsigned long current_time) {
  test_time(current_time);
  /* Definování odloženého startu a přechod do stavu WAITING. V případě SLAVE se čeká na mastra */
  delay_interval = (sync_state == SLAVE) ? MAX_START_TIME : random(MIN_START_TIME, MAX_START_TIME);
  delay_start_time = current_time;
  fencer_result = 0;
  fencer_state = WAITING;
  /* Rozsvvítit dvojtečku */
  uint8_t data[] = {
      0x00,
      0x80,
      0x00,
      0x00,
  };
  display.setSegments(data);
}


/**
 * @brief Přepínání do stavu MEASUREMENT
 *
 * @param current_time Aktuální čas
 */
void to_measurement(unsigned long current_time) {
  test_time(current_time);
  fencer_state = MEASUREMENT;
  measurement_start_time = current_time;
  if (sync_state == MASTER) {
    digitalWrite(SYNC_PIN_OUTPUT, HIGH);
  }
  display.showNumberDec(0, true);
}


/**
 * @brief Hlavní smyčka programu
 */
void loop() {
  /* Aktuální čas na procesoru */
  unsigned long current_time = millis();
  /* Čtení pinu z připojené zbraně */
  bool weapon_state = check_weapon();

  /* Pokuze pro výpočet času jednoho cyklu. */
  loop_counter++;

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

        if ((digitalRead(SYNC_PIN_INPUT) == HIGH) && (sync_state == SINGLE)) {
          /* Přepnutí do modu MASTER */
          sync_state = MASTER;
        }
        break;
      }

      if (sync_state != MASTER) {
        digitalWrite(SYNC_PIN_OUTPUT, LOW);
      }

      if ((sync_state == MASTER) && (digitalRead(SYNC_PIN_INPUT) == LOW)) {
        /* MASTER spouští synchronizační pin */
        to_waiting(current_time);
        digitalWrite(SYNC_PIN_OUTPUT, LOW);
      }

      if (sync_state == SINGLE) {
        /* SINGLE není synchronizovaný */
        to_waiting(current_time);
      }

      if ((sync_state == SLAVE) && (digitalRead(SYNC_PIN_INPUT) == LOW)) {
        /* SLAVE čeká na MASTER */
        to_waiting(current_time);
      }

      break;
    }

    case WAITING: {

      if ((sync_state == SLAVE) && (digitalRead(SYNC_PIN_INPUT) == HIGH)) {
        /* Čekání na mastera */
        to_measurement(current_time);
      }

      /* Čas odloženého startu vypršel -> Přechod do stavu MEASUREMENT */
      if (current_time - delay_start_time >= delay_interval) {
        if (sync_state == MASTER) {
          digitalWrite(SYNC_PIN_OUTPUT, HIGH);
        }
        to_measurement(current_time);
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

      if ((sync_state == MASTER) && (fencer_result > 50)) {
          digitalWrite(SYNC_PIN_OUTPUT, LOW);
      }

      /* Aktuální čas od startu měření (zobrazeno s nulama zleva pro výraznější
       * signál začátku) */
      display.showNumberDec(fencer_result, true);

      /* Stisknuto v intervalu -> Předhod do stavu s vísledkem. */
      if ((weapon_state) && (weapon_state != last_weapon_state)) {
        fencer_state = RESULT;
        test_time(current_time);
      }

      /* Čas vypršel -> Předhod do stavu RESULT s chybou TIME_OUT. */
      if (current_time - measurement_start_time >= MAX_TIME) {
        fencer_state = RESULT;
        fencer_result = TIME_OUT;
        test_time(current_time);
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

        /* Počátek synchronizace */
        digitalWrite(SYNC_PIN_OUTPUT, HIGH);
        if (digitalRead(SYNC_PIN_INPUT) == HIGH) {
          sync_state = SLAVE;
        } else {
          sync_state = SINGLE;
        }

        test_time(current_time);
      }
      break;
    }
  }

  /* Uložení posledního stavu */
  last_weapon_state = weapon_state;
//  delay(1);
}

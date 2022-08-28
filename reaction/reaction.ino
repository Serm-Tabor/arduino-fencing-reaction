
#include <Arduino.h>
#include <TM1637Display.h>

typedef enum
{
    ACTIVATION,
    WAITING,
    MEASUREMENT,
    RESULT
} States;

/*
 * Sekvence animace spuštění
 */
const uint8_t START_ANIMATION[6][4] = {
    {SEG_E | SEG_F, 0x00, 0x00, SEG_B | SEG_C},
    {SEG_F | SEG_A, 0x00, 0x00, SEG_C | SEG_D},
    {SEG_A, SEG_A, SEG_D, SEG_D},
    {0x00, SEG_A | SEG_D, SEG_A | SEG_D, 0x00},
    {SEG_D, SEG_D, SEG_A, SEG_A},
    {SEG_D | SEG_E, 0x00, 0x00, SEG_A | SEG_B},
};

/* čas v ms pro dobu měření */
#define MAX_TIME 15000

/* čas v ms pro dlouhé stisknutí */
#define LONG_PRESS 2000

/* intervaly odloženého startu v ms */
#define MIN_START_TIME 4000
#define MAX_START_TIME 10000

/* Rychlost animace v ms */
#define ANIMATION_SPEED 50

/* Piny pro komunikaci s displayem */
#define CLK 5
#define DIO 6

/* Vstupní pin pro kord. Další kabely kordu se zapojují do GND */
#define FENCER_BUTTON 2

/* Kokntrolní dioda stisknutí zbraně. */
#define CONTROL_DIODE 13

/* čas výsledku pro vypršení času */
#define TIME_OUT -2
/* čas výsledku pro předčasné spuštění */
#define TOO_FAST -1

/* zbraň stiskuta (INPUT_PULLUP a pin se spína přes GND) */
#define WEAPON_PRESSED LOW
/* zbraň odpojena (INPUT_PULLUP a pin se spína přes GND) */
#define WEAPON_UNPRESSED HIGH

/* Stav programu */
States fencer_state = RESULT;
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
uint8_t last_weapon_state = WEAPON_UNPRESSED;
/* Objekt pro display s řadičem TM1637 */
TM1637Display display(CLK, DIO);

/**
 * @brief Inincializace pinů a displaye
 */
void setup()
{
    pinMode(FENCER_BUTTON, INPUT_PULLUP);
    pinMode(CONTROL_DIODE, OUTPUT);
    display.setBrightness(4);
}

/**
 * @brief Vykreslení změřeného výsledku v ms.
 * Při záporné hodnotě výsledku se nevykresluje čas, ale důvod zrušení měření.
 *
 * @param weapon_state při hodnotě WEAPON_PRESSED se vykresluje dvojtečka na display. Vizuální odezva sepnuté zbraně.
 */
void print_result(uint8_t weapon_state)
{
    /* Dvojtečka na display - zobrazuje se jen, když je zbraň sepnutá. */
    int show_colon = 0b11000000;
    if (weapon_state == WEAPON_UNPRESSED)
    {
        show_colon = 0;
    }

    /* Zobrazení špatných výsledků (čas vypršel nebo stisnuto před startem) */
    if (fencer_result < 0)
    {
        /* čas vypršel nebo inicializace */
        if (fencer_result == TIME_OUT)
        {
            uint8_t data[] = {
                SEG_G,
                SEG_G | show_colon,
                SEG_G,
                SEG_G,
            };
            display.setSegments(data);
        }

        /* Zásah před spuštěním */
        if (fencer_result == TOO_FAST)
        {
            uint8_t data[] = {
                SEG_G,
                0x7F | show_colon,
                0x7F,
                SEG_G,
            };
            display.setSegments(data);
        }
    }

    /* Zobrazení správhého výsledku */
    else
    {
        display.showNumberDecEx(fencer_result, show_colon);
    }
}

/**
 * @brief Hlavní smyčka programu
 */
void loop()
{
    /* Aktuální čas na procesoru */
    unsigned long current_time = millis();
    /* Čtení pinu z připojené zbraně */
    uint8_t weapon_state = digitalRead(FENCER_BUTTON);

    /* Kontrolní dioda sepnuté zbraně */
    digitalWrite(CONTROL_DIODE, (weapon_state == WEAPON_PRESSED) ? HIGH : LOW);

    switch (fencer_state)
    {

    case ACTIVATION:
    {
        /* Animace při stisknuté zbrani */
        if (weapon_state == WEAPON_PRESSED)
        {
            int animation_frime = (current_time - animation_time) / ANIMATION_SPEED;
            if (animation_frime >= sizeof(START_ANIMATION) / sizeof(START_ANIMATION[0]))
            {
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

    case WAITING:
    {
        /* Rozsvvítit dvojtečku */
        uint8_t data[] = {
            0x00,
            0x80,
            0x00,
            0x00,
        };
        display.setSegments(data);

        /* Čas odloženého startu vypršel -> Přechod do stavu MEASUREMENT */
        if (current_time - delay_start_time >= delay_interval)
        {
            fencer_state = MEASUREMENT;
            measurement_start_time = current_time;
        }

        /* Stisknuto před zahájením měření -> Přechod do stavu RESULT s chybou TOO_FAST */
        if ((weapon_state == WEAPON_PRESSED) && (weapon_state != last_weapon_state))
        {
            fencer_state = RESULT;
            fencer_result = TOO_FAST;
        }
        break;
    }

    case MEASUREMENT:
    {
        fencer_result = current_time - measurement_start_time;

        /* Aktuální čas od startu měření (zobrazeno s nulama zleva pro výraznější signál začátku) */
        display.showNumberDec(fencer_result, true);

        /* Stisknuto v intervalu -> Předhod do stavu s vísledkem. */
        if ((weapon_state == WEAPON_PRESSED) && (weapon_state != last_weapon_state))
        {
            fencer_state = RESULT;
        }

        /* Čas vypršel -> Předhod do stavu RESULT s chybou TIME_OUT. */
        if (current_time - delay_start_time >= MAX_TIME)
        {
            fencer_state = RESULT;
            fencer_result = TIME_OUT;
        }
        break;
    }

    case RESULT:
    {
        print_result(weapon_state);

        /* Rušení dlouhého stisku */
        if (weapon_state == WEAPON_UNPRESSED)
        {
            long_press = false;
        }

        /* Začátek dlouhého stisku */
        if (weapon_state == WEAPON_PRESSED && (weapon_state != last_weapon_state))
        {
            long_press_start_time = current_time;
            long_press = true;
        }

        /* PŘECHOD DO STATUSU ACTIVATION */
        if (long_press && (weapon_state == WEAPON_PRESSED) && (current_time - long_press_start_time >= LONG_PRESS))
        {
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

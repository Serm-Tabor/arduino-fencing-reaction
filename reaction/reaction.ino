
typedef enum
{
    START_UP,
    WAITING,
    MEASUREMENT,
    RESULT
} States;

#define MAX_TIME 10000

States fencer_state = RESULT;
int fencer_button = 2;
long fencer_result = 0;
unsigned long delay_interval = 0;
unsigned long delay_start_time = 0;
unsigned long measurement_start_time = 0;


int last_state_fencer_button = HIGH;

void setup()
{
    Serial.begin(115200);
    pinMode(fencer_button, INPUT_PULLUP);
}

void loop()
{
    unsigned long current_time = millis();
    char current_button = digitalRead(fencer_button);

    switch (fencer_state)
    {
    
    case START_UP:
    {
        Serial.println("START_UP");

        delay_interval = (random(4, 10) * 1000);
        delay_start_time = current_time;
        fencer_result = 0;
        fencer_state = WAITING;
        break;
    }
    
    case WAITING:
    {
        Serial.print("WAITING> ");
        Serial.println(current_time - delay_start_time);
        if (current_time - delay_start_time >= delay_interval)
        {

            fencer_state = MEASUREMENT;
            measurement_start_time = current_time;
        }

        if ((current_button == LOW) && (current_button != last_state_fencer_button))
        {
            fencer_state = RESULT;
            fencer_result = 0;
        }
        break;
    }
    
    case MEASUREMENT:
    {
        Serial.print("MEASUREMENT> ");
        Serial.println(current_time - measurement_start_time);
        
        if ((current_button == LOW) && (current_button != last_state_fencer_button))
        {
            fencer_state = RESULT;
            fencer_result = current_time - measurement_start_time;
        }

        if (current_time - delay_start_time >= MAX_TIME)
        {
            fencer_state = RESULT;
            fencer_result = 0;
        }
        break;
    }
    
    case RESULT:
    {
        Serial.print("RESULT> ");
        Serial.println(fencer_result / 1000.0);
        if ((current_button == LOW) && (current_button != last_state_fencer_button))
        {
            fencer_state = START_UP;
        }
        break;
    }
    }

    last_state_fencer_button = current_button;
    delay(1);
}

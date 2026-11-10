**English** | [Čeština](README.cs.md)

# Arduino attack reaction

Measures and displays reaction time — from a visual signal until a button is pressed. Two units can be connected and synchronized.

The program is designed for the switches in sport fencing weapons, but it works with any push button.

## Setup
To build a unit you need
* an Arduino dev board
* a four-digit seven-segment LED display (clock style) with a [__TM1637__](https://www.google.com/search?tbm=isch&q=TM1637%20display) driver

### Connecting the weapon
The weapon is connected to the unit with a sport fencing body cord according to this diagram:
```
     ----
    |    |=====  <-- FOIL_PIN (Foil)
____|    |
         |
____     |=====  <-- GND
    |    |
    |    |=====  <-- EPEE_PIN (Épée)
     ----
```
Only the cord is needed to connect the weapon. The weapon inputs use __INPUT_PULLUP__ mode.

#### Using a button instead of a weapon
Any normally open push button can be connected between __GND__ and __EPEE_PIN__. A normally closed button (one that breaks the circuit when pressed) goes between __GND__ and __FOIL_PIN__.

### Connecting the display
Wire the display as described by the [TM1637](https://github.com/avishorp/TM1637) library.

### Connecting a second unit
To link two units, the __SYNC_PIN_INPUT__ pins must be pulled to ground through a __10k__ resistor.

__Board wiring__:
| Unit 1 | Unit 2
--- | ---
| Vin | Vin
| GND | GND
| __SYNC_PIN_OUTPUT__ | __SYNC_PIN_INPUT__ --_10k_-- GND
| GND --_10k_-- __SYNC_PIN_INPUT__ | __SYNC_PIN_OUTPUT__

## How it works
The program runs in four states.

### Result
Shows the measured time or the program status.

| Value | Meaning
--- | ---
| _number_ | measured time in _ms_
| __-00-__ | Measurement aborted.
| __----__ | Measurement timed out.
| __FOIL__ | Switched to foil mode
| __EPEE__ | Switched to épée mode

A __lit colon__ on the display means the button is pressed.

#### Entering the Arming state
Holding the button for more than _2s_ switches to the __Arming__ state.

### Arming
The state lasts as long as the button is held. On release, the unit moves to the __Waiting__ state.

#### Synchronization
If two units are connected (via _SYNC_PIN_INPUT_ and _SYNC_PIN_OUTPUT_) and both are in the __Arming__ state, they synchronize and both measure the identical time.

### Waiting
A random interval (_3–6s_) that prevents guessing when the measurement starts. When it elapses, the unit moves to the __Measuring__ state. If the units are synchronized, one unit picks the interval and triggers the other into __Measuring__.

If the button is pressed during waiting, the unit returns to __Result__ showing __-00-__ and the measurement is aborted.

### Measuring
Time is measured until the button is pressed, then the unit moves to __Result__. The state also ends after _10s_ with no result.


## Known issues and TODO
- [ ] Change how or how often the display is redrawn, as it slows down measurement. Current accuracy is about 20ms.
- [ ] Design synchronization for more units. Currently only 2 can be synchronized.
- [ ] Add other measurements, e.g. the interval between a double hit, number of hits in _10s_...

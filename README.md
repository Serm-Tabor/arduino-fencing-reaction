# Arduino attack reaction

Měření a zobrazení času reakce. Měří se od vizuální signalizace po stiskutí tlačítka. Lze zapojit dvě synchornizované jednotky.

Program je navrhnutý pro spínání zbraní sportovního šermu. Každopádně, je možné ho využít s jakýmkoliv tlačítkem.

## Zprovoznění
Pro zprovození jednotky potřebujete
* Arduino dev board
* Sedmisegmentový LED displej se čtyřmi bloky (hodinový) s řadičem [__TM1637__](https://www.google.com/search?tbm=isch&q=TM1637%20display) 

### Zapojení zbraně
Zbraň se do jednotky zapojuje podle následujícího schématu, pomocí kabelu pro sportovní šerm:
```
     ---- 
    |    |=====  <-- FOIL_PIN (Fleret)
____|    |
         |
____     |=====  <-- GND
    |    |
    |    |=====  <-- EPEE_PIN (Kord)
     ---- 
```
Pro připojení zbraně stačí pouze kabel. Vstupy ze zbraní jsou v režimu __INPUT_PULLUP__.

#### Zapojení tlačítka místo zbraně
Mezi piny __GND__ a __EPEE_PIN__ lze připojit libovolné spínací tlačítko. Pokud máte k dispozici rozpínací tlačítko (při stisnutí obvod přeruší) připojte jej mezipiny __GND__ a __FOIL_PIN__ .

### Zapojení displeje
Displej se zapojuje dle knihovny [TM1637](https://github.com/avishorp/TM1637).

### Zapojení druhé jednotky
Pro spojení dvou jednotek je potřeba uzemnit piny __SYNC_PIN_INPUT__ odporem __10k__.

__Propojení desek__:
| Jednotka 1 | Jednotka 2
--- | ---
| Vin | Vin
| GND | GND
| __SYNC_PIN_OUTPUT__ | __SYNC_PIN_INPUT__ --_10k_-- GND
| GND --_10k_-- __SYNC_PIN_INPUT__ | __SYNC_PIN_OUTPUT__

## Chod programu
Program pracuje ve čtyřech stavech.

### Výsledek
Zobrazení změřeného času, a nebo stavu programu.

| Hodnota | Popis
--- | ---
| _číslo_ | změřený čas v _ms_
| __--00--__ | Měření přerušeno.
| __----__ | Výpršel čas měření.
| __FOIL__ | Přepnutý režim pro fleret
| __EPEE__ | Přepnutý režim pro kord

__Rozsvícená dvojtečka__ nadisplay znázorňuje sepnuté tlačítko.
#### Přechod do stavu Spouštění
Pokud se drží tlačítko déle než _2s_, přechází se do stavu __Spouštění__.

### Spouštění
Stav je aktivní po celou dobu stisknutého tlačítka. Při odpojení se přechází do stavu __Čekání__.

#### Synchronizace
Pokud jsou dvě jednotky propojeny (přes _SYNC_PIN_INPUT_ a _SYNC_PIN_OUTPUT_) a obě jednotky jsou ve stavu __Spouštění__ tak se synchronizují a na obou jednotkách se měří identický čas.

### Čekání
Náhodný interval (_3-6s_), který zabrání odhadnout čas startu měření. Po uplynutí intervalu přechází do stavu __Měření__. Pokud jsou jednotky synchronizovány, tak interval určí jedna jednotka a druhou spustí do přechodu __Měření__.

Pokud se během čekání stiskne tlačítko, přechází se do stavu __Výsledek__ s textem __-00-__ a měření je přerušené.

### Měření
Měří se do sepnutí tlačítka. Po stiskutí přechází do stavu __Výsledek__. Stav se ukončí i po uplynutí _10s_ s neznámým výsledkem.


## Chyby a dodělání
- [ ] Změnit způsob nebo intervaly vykreslování, které měření zpomaluje. V současnosti se dostáváme na přesnost měření 20ms.
- [ ] Navrnout synchronizaci více jednotek. V současnosti lze synchronizovat pouze 2.
- [ ] Navrhnout i jiné měření. Například intervali mezi dvojzásahem, počet zásahu za _10s_...

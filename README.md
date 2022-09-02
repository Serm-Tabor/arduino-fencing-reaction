# Arduino reaction

Vizuální zobrazení změřeného času reakce. Čas se měří od vizuální signalizace do stiskutí tlačítka. Lze zapojit dvě jednotky, které si sinchronizují čas měření.

Program je navrhnutý pro spínání zbraní sportovního šermu, nicméně libovolné tlačítko pro měření reakcí.

## Zprovoznění
Pro zprovození jednotky potřebujete
* Arduino 
* 7 segmentový LED display se 4 znaky (hodinový) s řadičem __TM1637__

### Zapojení zbraně
Zbraň se do jednotky zapojuje podle následujícího schématu do šňůry pro sportovní šerm:
```
     ---- 
    |    |=====  <-- FOIL_PIN
____|    |
         |
____     |=====  <-- GND
    |    |
    |    |=====  <-- EPEE_PIN
     ---- 
```
Pro připojení zbraně nejsou nutné žádné dopory. Vstupy ze zbraní jsou v režimu __INPUT_PULLUP__.
#### Zapojení tlačítka místo zbraně:
Mezi piny __GND__ a __EPEE_PIN__ lze připojit libovolné spínací tlačítko. Pokud máte k dispozici rozpínací tlačítko (při stisnutí obvod přeruší) připojte jej mezipiny __GND__ a __FOIL_PIN__
### Zapojení displaye
Display se zapojuje dle knihovny [TM1637](https://github.com/avishorp/TM1637)

### Zapojení druhé jednotky
Pro spojení dvou jednotek je třeba piny __SYNC_PIN_INPUT__ uzemnit odporem velikosti přibližně __10 kOhm__.
Piny zapojení

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
Pokud se drží tlačítko déle než _2s_, přechází se do stavu __Spouštění__
### Spouštění
Stav je aktivní po celou dobu stisknutého tlačítka. Při odpojení se přechází do stavu __Čekání__
#### Synchronizace
Pokud jsou dvě jednotky propojeny (přes _SYNC_PIN_INPUT_ a _SYNC_PIN_OUTPUT_) a obě jednotky jsou ve stavu __Spouštění__ tak se synchronizují a na obou jednotkách se měří identický čas.
### Čekání
Náhodný interval (_3-6s_), který zabrání odhadnout čas startu měření. Po uplynutí intervalu předhází do stavu __Měření__. Pokud jsou jednotky synchronizovaný, tak interval určí jedna jednotka a druhou spustí do přechodu __Měření__

Pokud se během čekání stiskne tlačítko, přechází se do stavu Výsledek s textem __-00-__ a měření je přerušené.
### Měření
Měří se do spuštění tlačítka. Po stiskutí přechází do stavu __Výslekek__. Stav se ukončí i po uplinutí _10s_ s neznámím výsledkem.


## Chyby a dodělání
- [ ] Změnit způsob nebo intervaly vykreslování, které měření zpomaluje. V současnosti se dostáváme na přesnost měření 20ms.
- [ ] Navrnout synchronizaci více jednotek. V současnosti lze synchronizovat pouze 2.
- [ ] Navrhnout i jiné měření. Například intervali mezi dvojzásahem, počet zásahu za _10s_...
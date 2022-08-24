# arduino reaction

Vizuální zobrazení změřeného času reakce. Čas se měří od vizuální signalizace do stiskutí tlačítka.

## Návrh
Program má pacovat ve čtyřech stavech.
### Spouštění
vhodné použití tlačítka. Probíhá od stiskutí tlačíta a trvá 2s. Po splnění této podímky stav přechází do dalšího kroku. Bude možné tento stav použít z libovolného stavu?
### Čekání
Náhodný interval (5-10s), který zabrání odhadnout čas startu měření. Po uplynutí intervalu předhází do dalšího stavu
### Měření
Měří se do spuštění tlačítka. Po stiskutí přechází do dalšího stavu. Stav se ukončí i po uplinutí (10s?) s neznámím výsledkem.
### Výsledku
Zobrazení změřeného času dle hardware.


# masterpiec
## 
## Co to jest, zasada działania
Masterpiec to kompletny sterownik pieca CO z podajnikiem działający w oparciu o algorytm 'trójstanowy'.
Polega to na tym że spalanie odbywa się w trzech trybach: duża moc, niska moc oraz postój. Sterownik stara się nie wchodzić w tryb postoju w miarę możliwości, zamiast tego pracuje
w trybie ciągłym przełączając się między wysoką a niską mocą. 

Ustawienia dla mocy wysokiej i niskiej są konfigurowalne i powinny być dobrane doświadczalnie, w zależności od możliwości palnika w kotle oraz od zapotrzebowania instalacji na ciepło. Założenie jest takie że tryb wysoki powinien dawać moc wyższą niż instalacja jest w stanie odbierać (czyli że piec będzie w stanie podgrzewać wodę w instalacji mimo ciągłego odbioru ciepła), a tryb niskiej mocy powinien być poniżej średniego zapotrzebowania instalacji na ciepło (czyli że podczas pracy w tym trybie temperatura instalacji będzie się obniżać wraz z odbiorem ciepła przez budynek). Wtedy, przełączając się między trybem wysokim a niskim sterownik jest w stanie regulować temperaturę w instalacji bez przerywania pracy kotła.

Sterownik używa tylko dwóch mocy podczas pracy, nie próbuje uzyskać innych mocy pośrednich. Dzięki temu można wyregulować ustawienia spalania tak żeby było ono poprawne w obu trybach.

## Funkcje sterownika
* pompa CO
* pompa CWU
* pompa cyrkulacji
* czujniki: temperatura CO, CWU, powrotu, temp. spalin
* podajnik
* dmuchawa - sterowanie algorytmem grupowym 
* obsługa zewnętrznego termostatu
* tryb letni (tylko CWU)

## Hardware

* Arduino Mega
* Shield SD card
* 8-kanałowy moduł przekaźników SSR z detekcją zera
* wyświetlacz i2c, enkoder
* moduł RTC

## Funkcje poprawiające jakość pracy kotła
- funkcja dopalania - przy zmniejszaniu mocy lub przechodzeniu w postój piec przeprowadza dodatkowy cykl dopalania nadmiaru opału, dzięki czemu zmniejsza się ilość wytwarzanej sadzy
- rozpalanie zawsze na wysokiej mocy przy wznawianiu pracy po postoju, dzięki czemu temperatura żaru rośnie szybko i zmniejsza się dymienie
- dobra współpraca z termostatem pokojowym, algorytm jest dostosowany do zewnętrznego sterowania i nie destabilizuje się przy niespodziewanych zmianach zapotrzebowania na ciepło
- zabezpieczenie przed przekraczaniem temp zadanej (poprzez odpowiednio wczesniejsze zmniejszenie mocy)


Zestawienie połąćzeń GPIO Arduino z pozostałymi modułami.
Nie wymieniamy tu pinów GND - wszystkie elementy mają wspólne GND 
Nie zaznaczam też VCC - wszystkie moduły zasilanie 5V wspólne

| Pin Arduino | Drugi koniec |
| --- | --- |
|  | Moduł wykonawczy SSR |
| 2 | DET - detekcja 0 |
| A9 | Pompa CO |
| A10 | Podajnik |
| A11 | Pompa cyrkulacji |
| A13 | Dmuchawa |
| A14 | POMPA CO 2 |
| A15 | POMPA CWU | 
|  |  |
| 47 | Termostat we |
| 26 | Termostat alternatywne wejscie (NC) |
|  | Termopara/czujnik spalin |
| 39 | MAX 6675 SCK |
| 40 | MAX 6675 SO |
| 41 | MAX 6675 CS |
|  | czujniki DALLAS |
| 22 | DATA |
|  | Enkoder |
| 15 | PIN A |
| 18 | PIN B |
| 34 | Przycisk |
|  | Wyswietlacz I2C oraz moduł RTC|
| SDA | SDA |
| SCL | SCL |


Zestawienie Modułów


1) Moduł Przekaźnika 8x SSR Triaki Detekcja 0 ARDUINO - z firmy Fast Electronic (dostępne z Allegro)
wybrać z optotriakiem MOC3021 choć druga wersja MOC3041 też powinna działać
2) 1x Moduł RTC
3) 1x Wyświetlacz LCD 2x16
4) 1x Enkoder
5) 4x DALLAS DS1820B (temp CO, temp CWU, temp. powrotu, temp podajnika). Wybrać wersje z odpowiednią długością kabla, zwłaszcza do CWU
6) 1x MAX 6675 z termoparą do 400 stopni
7) zasilacz 9V dla Arduino 
8) 5x Gniazdo AC komputerowe sieciowe 3pin do obudowy (3 pompy, podajnik, dmuchawa)
9) Arduino 2560 mega




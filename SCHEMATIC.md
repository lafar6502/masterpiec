
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

| Moduł | Liczba szt | Informacje |
| --- | --- | --- |
| Arduino 2560 Mega | 1 | klon zupełnie wystarcza |
| Ethernet shield W5100 | 1 | Aktualnie nie używamy ethernetu, wyłącznie karty SD. Ale warto mieć możliwość podłączenia sieci |
| Moduł Przekaźnika 8x SSR Triaki Detekcja 0 ARDUINO | 1 | z firmy Fast Electronic (dostępne z Allegro) wybrać z optotriakiem MOC3021 choć druga wersja MOC3041 też powinna działać |
| Zegar RTC DS1307 | 1 | |
| Wyświetlacz LCD 2x16 I2C | 1 | |
| Enkoder | 1 | Dowolny z przyciskiem |
| Czujnik temperatury DS18B20 | 4 | czujniki z kablem, wybrać kable odpowiedniej długości (temp CO, temp CWU, temp. powrotu, temp podajnika) |
| MAX 6675 | 1 | moduł termopary z termoparą 400st |
| Zasilacz 9V | 1 | zasilacz dla arduino (od 7.5 do 10V, 10-20W) |
| Gniazdo AC komputerowe 3pin | 5 | zasilanie pomp, podajnika, dmuchawy |
| Zabezpieczenie STB | 1 | Wyłącznik bimetaliczny NC 90 stopni C |






## Co to jest MAF i w jaki sposób go wykorzystujemy
MAF Sensor to przepływomierz wykorzystywany w układach dolotowych silników samochodowych - mierzy ilość przepływającego do silnika powietrza w jednostce czasu.
Po zainstalowaniu na dmuchawie można go wykorzystać także do pomiaru ilości powietrza wchodzącego do kotła oraz, w konsekwencji, do sterowania szybkością pracy dmuchawy tak żeby ilość powietrza była prawidłowa.

## Jak działa MAF
MAF opiera swoje działanie na pomiarze szybkości chłodzenia rozgrzanego drucika przez opływające powietrze. Oznacza to ze nie ma żadnych części ruchomych.
Jeśli chodzi o komunikację (sposób przekazywania pomiarów), MAFy mogą wykorzystywać różne techniki:
1. napięcie - najprostszy, starszy typ MAF. Na wyjściu czujnika jest generowane napięcie z zakresu 0..5V proporcjonalne do szybkości przepływu powietrza
2. częstotliwość - MAF generuje impulsy o częstotliwości zależnej od szybkości przepływu
3. szerokość pulsu (?) - nie wiem czy takie występują
4. cyfrowe -np Bosch  i inne. Nie udało mi się ustalić jak działa komunikacja z nimi.

## Jaki MAF dla Masterpiec
Używamy MAF typu 'napięciowego' tzn takiego który generuje napięcie proporcjonalne do szybkości przepływu. Są to czujniki spotykane w starszych samochodach, można je znaleźć np w Ford-ach (Mondeo, KA). Mają złącze z czterema przewodami, natomiast na wtyczce są często umieszczone symbole ABCD lub EABCDF oznaczające poszczególne wyprowadzenia. Używane czujniki tego typu można łatwo kupić w cenie ok 10 PLN.<br>
Generalnie powinniśmy wybierać MAFy od silników o niskiej pojemności bo czujniki te są przystosowane do mniejszych przepływów powietrza i dają dzięki temu dokładniejsze wyniki pomiaru. 

MAF FORD - Wyprowadzenia przedstawiają się tak: <br>
A - zasilanie +12V<br>
B - GND<br>
C - GND<br>
D - sygnał czujnika 0..5V - odczytywany przez wejście analogowe Arduino, np A0<br>
Przykładowe numery części: 97BP-12B579-AA,96FP-12B579-AB

Żeby przetestować ten typ MAF wystarczy zasilanie 12V i woltomierz -badamy napięcie na pinie wyjściowym oraz, dmuchając do przepływomierza, sprawdzamy czy napięcie odpowiednio się zmienia. Pobór prądu podczas pracy powinien być na poziomie 10mA.

![image](https://user-images.githubusercontent.com/1706814/174991645-42abb5e7-1ce4-499b-aaa7-12e494611787.png)

MAF Bosch <br/>
Bosch produkuje MAFy w wersji analogowej (sygnał napięciowy) albo cyfrowej.
Wyprowadzenia w 4-pinowym Boschu analogowym przedstawiają się tak (piny 1 2 3 4):<br/>
1 - GND<br>
2 - sygnał - (może być GND)<br>
3 - +12V<br>
4. sygnał + <br>
Interesuje nas różnica napięć między pin 2 i 4, ale w sumie wystarczy różnica między GND a pin 4.<br>

Dla 5-pinowych Bosch:<br>
1 - NTC (czujnik temperatury) - ignorujemy<br>
2 - +12V <br>
3 - GND<br>
4. +5V<br>
5. Wyjście czujnika 0..5V<br>

Przykładowe numery części Bosch:
(4 pin) 0 280 217 111 (4pin), 0 280 217 102, 0 280 217 120, 0 280 217 519, 0 280 217 801, 0 280 217 107<br>
(5 pin) 0 280 217 123,  0 280 218 019,  0 280 217 531, 0 280 218 008, 0 281 002 421 <br> 
0 280 217 123 (5pin), 0 280 218 037 (5pin), 0 280 218 116 (5 pin), 0 280 218 335 (5 pin),  0 280 218 088, 0 280 218 440 (5 pin), <br>
0 280 218 446, 0 280 218 089, 




## MAF typ 2 - częstotliwość
Drugi potencjalnie łatwy do zastosowania rodzaj MAFa to grupa 2, tj sterujący częstotliwością impulsu.
W tej chwili Masterpiec nie używa tego typu przepływomierzy, ale jest szansa na ich wykorzystanie w przyszłości.


## MAF typ 3 - szerokość pulsu
Uzupełnię gdy znajdę przykład

## MAF typ 4 - cyfrowy
Bosch produkuje takie MAFy, na pierwszy rzut oka nie różnią się niczym od wersji z grupy 1 ale wyniki pomiaru są dostarczane w postaci cyfrowej. Protokół komunikacyjny nie jest mi znany i nie ma dokumentacji. Nie będziemy takich wykorzystywać w masterpiec.

## Inne MAFy
W szczególności firmy 'continental', 'siemens' - stosowane w różnych samochodach. Mają także 4 wyprowadzenia, ale nie udało mi się zidentyfikować sposobu ich podłączenia ani zasady działania.
Uwaga: zalecana ostrożność w podłączaniu do Arduino, w nieznanym typie MAF na wyjściach może pojawić się np napięcie 12V które doprowadzi do uszkodzenia arduino.
Dotyczy wszystkich MAFów.

## Zasilanie MAF

MAFy samochodowe są przystosowane do zasilania napięciem 12V (a konkretnie ok 14V) więc można wykorzystać zasilacz 12V do zasilania zarówno MAFa jak i Arduino.
Natomiast bez problemu powinno dać się też pracować z zasilaczem 9V bo MAFy mają dość szerokie dopuszczalne napięcie zasilania, z przedziału mniej-więcej 8.5V do 17V.
Pobór prądu przez MAF powinien być w  okolicach 10-15mA.

## Wykorzystanie czujnika przepływu do sterowania spalaniem

Odczyt przepływu powietrza działa podczas pracy dmuchawy. Gdy dmuchawa nie pracuje MAF jest wyłączany (zakładam że nie jest to czujnik przeznaczony do pracy 24/7).
Podawana przez czujnik wartość przepływu nie jest wyrażona w żadnych jednostkach, jest to po prostu odczyt napięcia z czujnika.

Standardowo wartość napięcia odczytywana przez Arduino może być z przedziału 0...1023 gdzie 1023 odpowiada napięciu 5V. Czujnik MAF nie osiąga 5V na wyjściu, zwykle są to napięcia z zakresu 2-3.5V - dlatego dokonujemy przeskalowania wyniku pomiaru do docelowej wartości z zakresu 0..255 (0 - brak przepływu, 255 - maksymalny możliwy przepływ).


Parametr "MAF Skala" określa skalowanie - Flow = odczyt napięcia * 255 / (4 * MAF_Skala + 3) <br>
Np MAF Skala=160 oznacza że masterpiec będzie widział maksymalny przepływ (Flow = 255) gdy odczyt z pinu A0 da wartość 160*4 + 3 = 643. Co odpowiada napięciu ok 3.15V z MAFa. Dobieramy MAF Skala tak żeby wartość Flow = 255 była maksymalnym możliwym do uzyskania na naszej dmuchawie przepływem.

MAF Skala należy dobrać doswiadczalnie zależnie od posiadanego MAFa



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
Używamy MAF typu 'napięciowego' tzn takiego który generuje napięcie proporcjonalne do szybkości przepływu. Są to czujniki spotykane w starszych samochodach, można je znaleźć np w Ford-ach (Mondeo, KA). Mają złącze z czterema przewodami, natomiast na wtyczce są często umieszczone symbole ABCD lub EABCDF oznaczające poszczególne wyprowadzenia. Używane czujniki tego typu można łatwo kupić w cenie ok 10 PLN.

Wyprowadzenia przedstawiają się tak: <br>
A - zasilanie +12V<br>
B - GND<br>
C - GND<br>
D - sygnał czujnika 0..5V - odczytywany przez wejście analogowe Arduino, np A0<br>

Żeby przetestować ten typ MAF wystarczy zasilanie 12V i woltomierz -badamy napięcie na pinie wyjściowym oraz, dmuchając do przepływomierza, sprawdzamy czy napięcie odpowiednio się zmienia. Pobór prądu podczas pracy powinien być na poziomie 10mA.

![image](https://user-images.githubusercontent.com/1706814/174991645-42abb5e7-1ce4-499b-aaa7-12e494611787.png)


## Drugi typ MAF
Drugi potencjalnie łatwy do zastosowania rodzaj MAFa to grupa III, tj sterujący szerokością impulsu.
Są to MAFy nieco nowszej generacji, często produkowane przez firmę Bosch, dla samochodów Volkswagen, Rover, Opel i innych.
Posiadają one również cztery wyprowadzenia, ale najczęściej oznaczane cyframi 1,2,3,4
Podczas pracy generują impuls w przybliżeniu prostokątny, o napięciu ok 3.3V i częstotliwości ok 19Hz
Szerokość impulsu zależy od zmierzonego przepływu.
W tej chwili Masterpiec nie używa tego typu przepływomierzy, ale jest szansa na ich wykorzystanie w przyszłości.

Wyprowadzenia: <br>
1 - zasilanie +12V <br>
2 - GND <br>
3 - zasilanie +5V<br>
4 - wyjście impuls prostokątny<br>

Żeby sprawdzić rodzaj MAFa najlepiej podłączyć zasilanie 12V i sprawdzić co jest generowane na wyjściu 4/D (np oscyloskopem). Powinien być widoczny sygnał prostokątny. Konieczne jest podwójne zasilanie  - 12 i 5V.

![image](https://user-images.githubusercontent.com/1706814/174993647-45c46403-0cfc-4bfe-8f74-a0b608ba6647.png)



## Inne MAFy
W szczególności firmy 'continental', 'siemens' - stosowane w różnych samochodach. Mają także 4 wyprowadzenia, ale nie udało mi się zidentyfikować sposobu ich podłączenia ani zasady działania.
Uwaga: zalecana ostrożność w podłączaniu do Arduino, w nieznanym typie MAF na wyjściach może pojawić się np napięcie 12V które doprowadzi do uszkodzenia arduino.


## Wykorzystanie czujnika przepływu do sterowania spalaniem

Odczyt przepływu powietrza działa podczas pracy dmuchawy. Gdy dmuchawa nie pracuje MAF jest wyłączany (zakładam że nie jest to czujnik przeznaczony do pracy 24/7).
Podaawana przez czujnik wartość przepływu nie jest wyrażona w żadnych jednostkach, jest to po prostu odczyt napięcia z czujnika.

Standardowo wartość napięcia odczytywana przez Arduino może być z przedziału 0...1023 gdzie 1023 odpowiada napięciu 5V. Czujnik MAF nie osiąga 5V na wyjściu, zwykle są to napięcia z zakresu 2-3.5V - dlatego dokonujemy przeskalowania wyniku pomiaru do docelowej wartości z zakresu 0..255 (0 - brak przepływu, 255 - maksymalny możliwy przepływ).


Parametr "MAF Skala" określa skalowanie - Flow = odczyt napięcia * 255 / (4 * MAF_Skala + 3) 

MAF Skala należy dobrać doswiadczalnie zależnie od posiadanego MAFa

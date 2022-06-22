

## Co to jest MAF i w jaki sposób go wykorzystujemy
MAF Sensor to przepływomierz wykorzystywany w układach dolotowych silników samochodowych - mierzy ilość przepływającego do silnika powietrza w jednostce czasu.
Po zainstalowaniu na dmuchawie można go wykorzystać także do pomiaru ilości powietrza wchodzącego do kotła oraz, w konsekwencji, do sterowania szybkością pracy dmuchawy tak żeby ilość powietrza była prawidłowa.

## Jak działa MAF
MAF opiera swoje działanie na pomiarze szybkości chłodzenia rozgrzanego drucika przez opływające powietrze. Oznacza to ze nie ma żadnych części ruchomych.
Jeśli chodzi o komunikację (sposób przekazywania pomiarów), MAFy mogą wykorzystywać różne techniki:
* napięcie - najprostszy, starszy typ MAF. Na wyjściu czujnika jest generowane napięcie z zakresu 0..5V proporcjonalne do szybkości przepływu powietrza
* częstotliwość - MAF generuje impulsy o częstotliwości zależnej od szybkości przepływu
* szerokość pulsu - MAF generuje impulsy o stałej częstotliwości (ok 19Hz) natomiast szerokość impulsu jest zależna od szybkości przepływu
* inne - oczywiście występują też MAFy których zasady działania nie byłem w stanie zidentyfikować

## Jaki MAF dla Masterpiec
Używamy MAF typu 'napięciowego' tzn takiego który generuje napięcie proporcjonalne do szybkości przepływu. Są to czujniki spotykane w starszych samochodach, można je znaleźć np w Ford-ach (Mondeo, KA). Mają złącze z czterema przewodami, natomiast na wtyczce są często umieszczone symbole ABCD lub EABCDF oznaczające poszczególne wyprowadzenia.

Wyprowadzenia przedstawiają się tak: <br>
A - zasilanie +12V<br>
B - GND<br>
C - GND<br>
D - sygnał czujnika 0..5V - odczytywany przez wejście analogowe Arduino, np A0<br>

![image](https://user-images.githubusercontent.com/1706814/174991645-42abb5e7-1ce4-499b-aaa7-12e494611787.png)


## Drugi typ MAF
Drugi potencjalnie łatwy do zastosowania rodzaj MAFa to grupa III, tj sterujący szerokością impulsu.
Są to MAFy nieco nowszej generacji, często produkowane przez firmę Bosch, dla samochodów Volkswagen, Rover, innych
Posiadają one również cztery wyprowadzenia, ale najczęściej oznaczane cyframi 1,2,3,4
Podczas pracy generują impuls w przybliżeniu prostokątny, o napięciu ok 3.3V i częstotliwości ok 19Hz
Szerokość impulsu zależy od zmierzonego przepływu.
W tej chwili Masterpiec nie używa tego typu przepływomierzy, ale jest szansa na ich wykorzystanie w przyszłości.

Wyprowadzenia: <br>
1 - zasilanie +12V <br>
2 - GND <br>
3 - zasilanie +5V<br>
4 - wyjście impuls prostokątny<br>

Żeby sprawdzić rodzaj MAFa najlepiej podłączyć zasilanie 12V i sprawdzić co jest generowane na wyjściu 4/D (np oscyloskopem)

![image](https://user-images.githubusercontent.com/1706814/174993647-45c46403-0cfc-4bfe-8f74-a0b608ba6647.png)



## Inne MAFy
W szczególności firmy 'continental', 'siemens' - stosowane w różnych samochodach. Mają także 4 wyprowadzenia, ale nie udało mi się zidentyfikować sposobu ich podłączenia ani zasady działania.
Uwaga: zalecana ostrożność w podłączaniu do Arduino, w nieznanym typie MAF na wyjściach może pojawić się np napięcie 12V które doprowadzi do uszkodzenia arduino.



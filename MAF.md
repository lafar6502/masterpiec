

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

I tak, 
A - zasilanie +12V<br>
B - GND<br>
C - GND<br>
D - sygnał czujnika 0..5V<br>

![maf_ford](https://user-images.githubusercontent.com/1706814/174986853-192e27db-4cfa-488d-8cd3-0739687729a8.jpg)



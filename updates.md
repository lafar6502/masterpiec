
2022-06-27

* Obsługa samochodowego czujnika MAF do pomiaru ilości powietrza
* Funkcja korekcji prędkości dmuchawy w czasie pracy w oparciu o odczyt MAF
* Zmiana połączeń arduino-peryferia (zmiany numerów pinów) z powodu reorganizacji kabelków

2022-06-19

* Obsluga zapalarki (glownie ze wzgledu na pellet, ale z ekogroszkiem tez powinno dzialac)
* Automatyczne rozpalanie i wygaszanie <br>Możliwość ustawienia czy piec ma się rozpalać i wygaszać automatycznie, czy nie.
* Obsługa max 3 zestawów ustawień między którymi można się przełączać (np gdy mamy kocioł na ekogroszek i pellet)<br> Uwaga: po zmianie aktywnego banku ustawień nalezy zrobić restart sterownika - wtedy zostaną wczytane te z nowo wybranego banku. Gdy po zmianie numeru banku nie zrobisz restartu, tylko np zmienisz jeszcze jakiś parametr, to aktualne ustawienia nadpiszą te w wybranym banku (czyli można w ten sposob skopiować ustawienia)
* ulepszone wykrywanie wygasnięcia ognia<br>Z uwzględnieniem temperatury spalin
* Wykrywanie rozpalenia ognia<br>W oparciu o wzrost temperatury spalin


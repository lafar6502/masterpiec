# masterpiec

## Summary
Masterpiec is an arduino-based pellet and coal burner controller. It automates the burning process and controls temperatures of central heating and hot water boiler. 
The software runs on Arduino Mega 2560 and drives the fuel feeder, blower fan and water pumps (central heating, hot water boiler and hot water circulation). The control is optimized for fuel efficiency and clean burn. Masterpiec can be adapted to multiple types of pellet and coal burners with automatic fuel feed, and is pretty simple to build as a DIY project. 
## 
## Aktualizacje

[Lista ostatnich zmian](updates.md)

## Co to jest, zasada działania
Masterpiec to kompletny sterownik pieca CO z podajnikiem działający w oparciu o algorytm 'trójstanowy'.
Polega to na tym że spalanie odbywa się w trzech trybach: duża moc, niska moc oraz postój. Sterownik stara się nie wchodzić w tryb postoju w miarę możliwości, zamiast tego pracuje
w trybie ciągłym przełączając się między wysoką a niską mocą. 

Ustawienia dla mocy wysokiej i niskiej są konfigurowalne i powinny być dobrane doświadczalnie, w zależności od możliwości palnika w kotle oraz od zapotrzebowania instalacji na ciepło. Założenie jest takie że tryb wysoki powinien dawać moc wyższą niż instalacja jest w stanie odbierać (czyli że piec będzie w stanie podgrzewać wodę w instalacji mimo ciągłego odbioru ciepła), a tryb niskiej mocy powinien być poniżej średniego zapotrzebowania instalacji na ciepło (czyli że podczas pracy w tym trybie temperatura instalacji będzie się obniżać wraz z odbiorem ciepła przez budynek). Wtedy, przełączając się między trybem wysokim a niskim sterownik jest w stanie regulować temperaturę w instalacji bez przerywania pracy kotła.

Sterownik używa tylko dwóch mocy podczas pracy, nie próbuje uzyskać innych mocy pośrednich. Dzięki temu można wyregulować ustawienia spalania tak żeby było ono poprawne w obu trybach.

## Po co powstał masterpiec
Żeby czysto i oszczędnie prowadzić proces spalania przy prostym i zrozumiałym algorytmie sterującym. Współczesne sterowniki 'PID' mają mniej lub bardziej skomplikowane algorytmy regulacji mocy, ale ich logika jest mało przejrzysta dla użytkownika a możliwości konfiguracji często mocno ograniczone. Prowadzi to w wielu sytuacjach do 'zgłupienia' sterownika i jego nieoptymalnego działania, zwłaszcza w przypadku instalacji nietypowych względem tego co zakłada producent.
Masterpiec działa w oparciu o prosty ale skuteczny algorytm którego parametry są konfigurowalne, dzięki czemu powinno być jasne dla użytkownika jak osiągnąć pożądane zachowanie kotła poprzez regulację parametrów sterowania.

Powodem dodatkowym był 'fun factor' przedsięwzięcia, czyli budowa sterownika od podstaw z możliwością samodzielnego zaprogramowania logiki według widzimisię autora. 

## zasady korzystania z kodu
Kod źródłowy jest udostępniony publicznie dla wszystkich którzy są zainteresowani poznaniem zasady działania sterownika oraz jego modyfikacją i rozbudową. Wszelkie projekty zbudowane na podstawie niniejszego kodu powinny być również udostępnione publicznie, autor nie zezwala na wykorzystanie tego kodu w projektach komercyjnych.   

## Funkcje sterownika
* pompa CO
* pompa CWU
* pompa cyrkulacji
* czujniki: temperatura CO, CWU, powrotu, temp. spalin
* podajnik
* dmuchawa - sterowanie algorytmem grupowym 
* obsługa zewnętrznego termostatu
* tryb letni (tylko CWU)
* wygaszanie i rozpalanie

## Hardware - kocioł
Masterpiec działa z kotłami z podajnikiem ślimakowym (testowany z palnikiem SV200). Dmuchawa - dowolny typ, 230V. Pompy - dowolne, 230V. Zapalarka 230V ceramiczna, dowolny model.
Niektóre pompy elektroniczne źle znoszą zewnętrzne sterowanie zasilaniem i mogą uszkodzić układ sterujący SSR (miałem jeden model Wilo który nie lubił być włączany i wyłączany przekaźnikiem, powodował uszkodzenia układu sterującego zasilaniem). Ale np 'inteligentne' pompy Grundfos pracują bezproblemowo.

## komponenty sterownika
* Arduino Mega
* Shield SD card
* 8-kanałowy moduł przekaźników SSR z detekcją zera
* wyświetlacz i2c, enkoder
* moduł RTC
* czujniki temperatury (3 szt DS18B20 oraz 1 lub 2szt MAX6675)
* przepływomierz samochodowy (MAF) - opcjonalnie

Sterownik jest zbudowany z gotowych modułów, nie wymaga robienia własnych płytek drukowanych ani wykonywania skomplikowanych układów elektronicznych. Potrzebna jest podstawowa umiejętność używania lutownicy i innych narzędzi oraz zrozumienie zasad łączenia urządzeń elektrycznych.

[Schemat połączeń komponentów](SCHEMATIC.md)


## Funkcje poprawiające jakość pracy kotła
- funkcja dopalania - przy zmniejszaniu mocy lub przechodzeniu w postój piec przeprowadza dodatkowy cykl dopalania nadmiaru opału, dzięki czemu zmniejsza się ilość wytwarzanej sadzy
- rozpalanie zawsze na wysokiej mocy przy wznawianiu pracy po postoju, dzięki czemu temperatura żaru rośnie szybko i zmniejsza się dymienie
- dobra współpraca z termostatem pokojowym, algorytm jest dostosowany do zewnętrznego sterowania i nie destabilizuje się przy niespodziewanych zmianach zapotrzebowania na ciepło
- zabezpieczenie przed przekraczaniem temp zadanej (poprzez zmniejszenie mocy z wyprzedzeniem)
- cyrkulacja w trybie ciągłym lub załączanie cykliczne w celu ograniczenia strat ciepła
- utrzymywanie stałego przepływu powietrza na podstawie wskazań czujnika MAF

## Funkcje planowane, do realizacji lub rozważenia
- zewnętrzne sterowanie załączeniem kotła, do współpracy z pompą ciepła (kocioł załącza się gdy temperatura powietrza jest za niska dla efektywnej pracy pompy, dzięki czemu pompa ciepła nie musi używać grzałek elektrycznych)
- zewnętrzne sterowanie pompami CO i CWU, też w celu współpracy z pompą ciepła zasilającą instalację przez wymiennik płytowy (gdy pompa ciepła działa to masterpiec załącza pompę CO lub CWU żeby był przepływ w obu obiegach wymiennika).
- sterowanie zaworem mieszającym
- sterowanie zaworem przełączającym trójdrożnym (CO/CWU) - dla instalacji posiadającej tylko jedną pompę obiegową.
- obsługa bufora ciepła
- sterowanie prędkością pracy pompy CO za pomocą PWM, w celu kontroli ilości ciepła odbieranego z kotła/wymiennika płytowego

## MAF - wykorzystanie samochodowego przepływomierza

[Poradnik MAF](MAF.md)

## Podziękowania, inspiracje

Główną inspiracją dla masterpieca był Lucjan - czyli sterownik kotła w wersji DIY udostępniony przez UZI18 

https://github.com/uzi18/sterownik

To dzięki ekipie Lucjana zobaczyłem że budowa sterownika nie jest aż tak trudnym przedsięwzięciem. 
Masterpiec zaczął się jako próba zbudowania Lucjana. Próba zakończona powodzeniem, jednak okazało się że Lucjan nie do końca realizuje zadania które chciałem żeby realizował. Z uwagi na to że Lucjan nie udostępnia kodu źródłowego i nie umożliwia samodzielnej modyfikacji logiki sterownika postanowiłem stworzyć alternatywne oprogramowanie od zera.
Masterpiec opiera się na tych samych komponentach hardware-owych co Lucjan, czyli Arduino Mega, popularnych czujnikach temperatury oraz dostępnych na rynku modułach wykonawczych. Dzięki pracy ekipy Lucjana nie musiałem tych komponentów dobierać samodzielnie, co prawdopodobnie nie udało by się nigdy. 

Konsekwencją powyższego jest to że masterpiec jest w dużym stopniu zgodny hardware-owo z Lucjanem i posiadając Lucjana można go przekonwertować na masterpiec/i w drugą stronę - choćby w celu porównania funkcji i wybrania lepiej sprawdzającego się rozwiązania. Nie ma 100% zgodności, ale różnice są niewielkie i dotyczą innych numerów pinów GPIO - być może zostanie to poprawione w przyszłości. 

Najważniejszą różnicą jest to że Masterpiec udostępnia kod źródłowy i umożliwia samodzielną modyfikację. Masterpiec jest też prostszy, realizuje mniej funkcji i nie oferuje wielu różnych wariantów hardware-u. Zdecydowanie zachęcam tutaj do wypróbowania zarówno Masterpieca jak i Lucjana i dziękuję ekipie Lucjana za pracę włożoną w ten ciekawy projekt.







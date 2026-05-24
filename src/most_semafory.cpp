#include <pthread.h>   // Biblioteka pthread: watki i mutexy.
#include <semaphore.h> // Semafory POSIX: sem_init(), sem_wait(), sem_post().
#include <unistd.h>    // Funkcja usleep(), czyli opoznienia w mikrosekundach.

#include <cstdio>   // Funkcje printf() i fprintf().
#include <cstdlib>  // Funkcja atoi().
#include <vector>   // Wektor do przechowywania danych watkow.

// Kierunek jazdy samochodu przez most.
enum Kierunek {
    A_DO_B, // Samochod jedzie z miasta A do miasta B.
    B_DO_A  // Samochod jedzie z miasta B do miasta A.
};

// Dane przekazywane do pojedynczego watku samochodu.
struct DaneSamochodu {
    int id;                   // Numer samochodu widoczny w komunikatach.
    Kierunek kierunekStartowy; // Kierunek pierwszego przejazdu samochodu.
    int limitPrzejazdow;           // Limit przejazdow; -1 oznacza prace bez konca.
};

// Mutex chroni wszystkie zmienne opisujace wspolny stan symulacji.
pthread_mutex_t mutexStanu = PTHREAD_MUTEX_INITIALIZER;

// Semafor dla samochodow czekajacych po stronie miasta A.
sem_t semaforKolejkiA;

// Semafor dla samochodow czekajacych po stronie miasta B.
sem_t semaforKolejkiB;

// Liczba samochodow w miescie A, ktore nie stoja jeszcze w kolejce.
int autaWMiescieA = 0;

// Liczba samochodow w miescie B, ktore nie stoja jeszcze w kolejce.
int autaWMiescieB = 0;

// Liczba samochodow czekajacych przed mostem od strony miasta A.
int kolejkaA = 0;

// Liczba samochodow czekajacych przed mostem od strony miasta B.
int kolejkaB = 0;

// Liczba obudzonych samochodow z kolejki A, ktore maja zarezerwowany wjazd.
int rezerwacjeA = 0;

// Liczba obudzonych samochodow z kolejki B, ktore maja zarezerwowany wjazd.
int rezerwacjeB = 0;

// Numer samochodu na moscie; -1 oznacza, ze most jest pusty.
int autoNaMoscie = -1;

// Kierunek samochodu, ktory aktualnie znajduje sie na moscie.
Kierunek kierunekNaMoscie = A_DO_B;

// Kierunek, ktory ma pierwszenstwo, gdy kolejka jest po obu stronach.
Kierunek czyjaKolej = A_DO_B;

// Zwraca przeciwny kierunek jazdy.
Kierunek przeciwnyKierunek(Kierunek kierunek) {
    // Jesli auto jechalo z A do B, nastepny przejazd bedzie z B do A.
    if (kierunek == A_DO_B) {
        return B_DO_A;
    }

    // W przeciwnym przypadku auto jechalo z B do A, wiec wroci z A do B.
    return A_DO_B;
}

// Funkcja wypisuje caly stan symulacji.
// Wywolujemy ja tylko wtedy, gdy mutex mutexStanu jest juz zablokowany.
void wypiszStan() {
    // Jezeli nikt nie jedzie po moscie, pokazujemy puste miejsce w nawiasie.
    if (autoNaMoscie == -1) {
        std::printf("A-%d %d>>> [    -    ] <<<%d %d-B\n",
                    autaWMiescieA,
                    kolejkaA,
                    kolejkaB,
                    autaWMiescieB);
    }
    // Jezeli samochod jedzie z miasta A do miasta B, strzalki ida w prawo.
    else if (kierunekNaMoscie == A_DO_B) {
        std::printf("A-%d %d>>> [>> %d >>] <<<%d %d-B\n",
                    autaWMiescieA,
                    kolejkaA,
                    autoNaMoscie,
                    kolejkaB,
                    autaWMiescieB);
    }
    // Jezeli samochod jedzie z miasta B do miasta A, strzalki ida w lewo.
    else {
        std::printf("A-%d %d>>> [<< %d <<] <<<%d %d-B\n",
                    autaWMiescieA,
                    kolejkaA,
                    autoNaMoscie,
                    kolejkaB,
                    autaWMiescieB);
    }

    // Wymuszamy natychmiastowe wypisanie komunikatu na ekran.
    std::fflush(stdout);
}

// Sprawdza, czy zaden obudzony watek nie ma juz zarezerwowanego wjazdu.
bool brakZarezerwowanegoWjazdu() {
    // Rezerwacja chroni przed sytuacja, w ktorej nowe auto wyprzedza obudzone auto.
    return rezerwacjeA == 0 && rezerwacjeB == 0;
}

// Sprawdza, czy samochod z podanego kierunku moze wjechac bez czekania na semafor.
// Ta funkcja zaklada, ze mutex mutexStanu jest juz zablokowany.
bool czyMozeWjechacOdRazu(Kierunek kierunek) {
    // Na moscie moze byc tylko jeden samochod, wiec zajety most blokuje wjazd.
    if (autoNaMoscie != -1) {
        return false;
    }

    // Jezeli ktos zostal juz obudzony semaforem, to on ma prawo wjazdu jako pierwszy.
    if (!brakZarezerwowanegoWjazdu()) {
        return false;
    }

    // Samochod chce jechac z A do B.
    if (kierunek == A_DO_B) {
        // Jesli po stronie B nikt nie czeka, nie ma konfliktu o pierwszenstwo.
        if (kolejkaB == 0) {
            return true;
        }

        // Gdy obie strony czekaja, wpuszczamy strone, ktorej aktualnie kolej.
        return czyjaKolej == A_DO_B;
    }

    // Samochod chce jechac z B do A.
    // Jesli po stronie A nikt nie czeka, nie ma konfliktu o pierwszenstwo.
    if (kolejkaA == 0) {
        return true;
    }

    // Gdy obie strony czekaja, wpuszczamy strone, ktorej aktualnie kolej.
    return czyjaKolej == B_DO_A;
}

// Rezerwuje wjazd dla jednego samochodu z wybranej strony i budzi go semaforem.
void obudzJedenSamochod(Kierunek kierunek) {
    // Budzimy samochod czekajacy po stronie A.
    if (kierunek == A_DO_B) {
        rezerwacjeA++;
        sem_post(&semaforKolejkiA);
    }
    // Budzimy samochod czekajacy po stronie B.
    else {
        rezerwacjeB++;
        sem_post(&semaforKolejkiB);
    }
}

// Wybiera kolejny samochod do obudzenia, gdy most jest pusty.
// Funkcja zaklada, ze mutex mutexStanu jest juz zablokowany.
void obudzNastepnyJesliMozna() {
    // Nie budzimy nikogo, jesli most nadal jest zajety.
    if (autoNaMoscie != -1) {
        return;
    }

    // Nie budzimy kolejnej osoby, jesli ktos ma juz zarezerwowany wjazd.
    if (!brakZarezerwowanegoWjazdu()) {
        return;
    }

    // Jesli obie strony czekaja, wybieramy kierunek zapisany w zmiennej czyjaKolej.
    if (kolejkaA > 0 && kolejkaB > 0) {
        obudzJedenSamochod(czyjaKolej);
        return;
    }

    // Jesli czeka tylko strona A, budzimy jeden samochod z A.
    if (kolejkaA > 0) {
        obudzJedenSamochod(A_DO_B);
        return;
    }

    // Jesli czeka tylko strona B, budzimy jeden samochod z B.
    if (kolejkaB > 0) {
        obudzJedenSamochod(B_DO_A);
    }
}

// Wprowadza samochod na most.
// Funkcja zaklada, ze mutex mutexStanu jest juz zablokowany.
void wjedzNaMost(int numerSamochodu, Kierunek kierunek) {
    // Samochod z A opuszcza kolejke po stronie A.
    if (kierunek == A_DO_B) {
        kolejkaA--;
    }
    // Samochod z B opuszcza kolejke po stronie B.
    else {
        kolejkaB--;
    }

    // Zapisujemy numer samochodu aktualnie jadacego po moscie.
    autoNaMoscie = numerSamochodu;

    // Zapisujemy kierunek, zeby komunikat pokazal wlasciwe strzalki.
    kierunekNaMoscie = kierunek;

    // Wypisujemy stan, bo zmienila sie kolejka i zawartosc mostu.
    wypiszStan();
}

// Male opoznienie symulujace przebywanie samochodu w miescie.
useconds_t opoznienieWMiescie(int numerSamochodu) {
    // Rozne samochody maja lekko rozne czasy, zeby wyjscie bylo ciekawsze.
    return 120000 + static_cast<useconds_t>((numerSamochodu % 5) * 30000);
}

// Male opoznienie symulujace przejazd samochodu przez most.
useconds_t opoznienieNaMoscie(int numerSamochodu) {
    // Most jest krotki, ale przejazd nadal trwa chwile.
    return 160000 + static_cast<useconds_t>((numerSamochodu % 3) * 40000);
}

// Funkcja wykonywana przez kazdy watek samochodu.
void* watekSamochodu(void* argument) {
    // Odbieramy dane samochodu przekazane z funkcji main().
    DaneSamochodu* car = static_cast<DaneSamochodu*>(argument);

    // Kazdy samochod pamieta swoj aktualny kierunek jazdy.
    Kierunek aktualnyKierunek = car->kierunekStartowy;

    // Liczymy, ile razy dany samochod przejechal juz przez most.
    int wykonanePrzejazdy = 0;

    // Petla trwa bez konca albo do osiagniecia opcjonalnego limitu przejazdow.
    while (car->limitPrzejazdow < 0 || wykonanePrzejazdy < car->limitPrzejazdow) {
        // Samochod przez chwile znajduje sie w miescie przed kolejna proba jazdy.
        usleep(opoznienieWMiescie(car->id));

        // Blokujemy mutex, bo za chwile zmienimy wspolny stan programu.
        pthread_mutex_lock(&mutexStanu);

        // Samochod jadacy z A do B opuszcza miasto A i ustawia sie w kolejce A.
        if (aktualnyKierunek == A_DO_B) {
            autaWMiescieA--;
            kolejkaA++;
        }
        // Samochod jadacy z B do A opuszcza miasto B i ustawia sie w kolejce B.
        else {
            autaWMiescieB--;
            kolejkaB++;
        }

        // Wypisujemy stan, bo zmienila sie liczba aut w miescie i w kolejce.
        wypiszStan();

        // Jesli most jest wolny i to nasza kolej, wjezdzamy od razu.
        if (czyMozeWjechacOdRazu(aktualnyKierunek)) {
            wjedzNaMost(car->id, aktualnyKierunek);
            pthread_mutex_unlock(&mutexStanu);
        }
        // W przeciwnym razie czekamy na odpowiednim semaforze.
        else {
            // Odblokowujemy mutex przed sem_wait(), zeby inne watki mogly pracowac.
            pthread_mutex_unlock(&mutexStanu);

            // Samochod po stronie A czeka na semaforze kolejki A.
            if (aktualnyKierunek == A_DO_B) {
                sem_wait(&semaforKolejkiA);
            }
            // Samochod po stronie B czeka na semaforze kolejki B.
            else {
                sem_wait(&semaforKolejkiB);
            }

            // Po obudzeniu znow blokujemy mutex, bo zaraz wjedziemy na most.
            pthread_mutex_lock(&mutexStanu);

            // Samochod z A zuzywa swoja rezerwacje wjazdu.
            if (aktualnyKierunek == A_DO_B) {
                rezerwacjeA--;
            }
            // Samochod z B zuzywa swoja rezerwacje wjazdu.
            else {
                rezerwacjeB--;
            }

            // Obudzony samochod ma juz przydzielone prawo wjazdu na most.
            wjedzNaMost(car->id, aktualnyKierunek);

            // Po ustawieniu auta na moscie konczymy prace na wspolnym stanie.
            pthread_mutex_unlock(&mutexStanu);
        }

        // Symulujemy przejazd przez most poza sekcja krytyczna.
        usleep(opoznienieNaMoscie(car->id));

        // Po przejezdzie znow blokujemy mutex, bo aktualizujemy wspolny stan.
        pthread_mutex_lock(&mutexStanu);

        // Most staje sie pusty.
        autoNaMoscie = -1;

        // Po przejechaniu z A do B samochod trafia do miasta B.
        if (aktualnyKierunek == A_DO_B) {
            autaWMiescieB++;

            // Jesli po stronie B ktos czeka, oddajemy mu pierwszenstwo.
            if (kolejkaB > 0) {
                czyjaKolej = B_DO_A;
            }
        }
        // Po przejechaniu z B do A samochod trafia do miasta A.
        else {
            autaWMiescieA++;

            // Jesli po stronie A ktos czeka, oddajemy mu pierwszenstwo.
            if (kolejkaA > 0) {
                czyjaKolej = A_DO_B;
            }
        }

        // Ten samochod zakonczyl jeden pelny przejazd przez most.
        wykonanePrzejazdy++;

        // Wypisujemy stan, bo samochod zniknal z mostu i pojawil sie w miescie.
        wypiszStan();

        // Budzimy jeden samochod, jesli ktos czeka i most moze zostac zajety.
        obudzNastepnyJesliMozna();

        // Konczymy aktualizacje wspolnego stanu.
        pthread_mutex_unlock(&mutexStanu);

        // Po dotarciu do drugiego miasta nastepny przejazd bedzie w druga strone.
        aktualnyKierunek = przeciwnyKierunek(aktualnyKierunek);
    }

    // Watek konczy prace po wykonaniu limitu przejazdow.
    return nullptr;
}

// Funkcja wypisuje krotka instrukcje uruchomienia programu.
void wypiszUzycie(const char* nazwaProgramu) {
    // Pierwszy argument jest obowiazkowy, drugi jest tylko do testow.
    std::fprintf(stderr, "Uzycie: %s LICZBA_SAMOCHODOW [LIMIT_PRZEJAZDOW]\n", nazwaProgramu);
}

int main(int argc, char** argv) {
    // Program wymaga jednego albo dwoch argumentow.
    if (argc != 2 && argc != 3) {
        wypiszUzycie(argv[0]);
        return 1;
    }

    // atoi() zamienia tekst z argumentu linii polecen na liczbe calkowita.
    int liczbaSamochodow = std::atoi(argv[1]);

    // Liczba samochodow musi byc dodatnia.
    if (liczbaSamochodow <= 0) {
        std::fprintf(stderr, "Blad: liczba samochodow musi byc wieksza od zera.\n");
        return 1;
    }

    // Domyslnie program dziala bez konca, zgodnie z trescia zadania.
    int limitPrzejazdow = -1;

    // Jesli podano drugi argument, traktujemy go jako limit przejazdow.
    if (argc == 3) {
        limitPrzejazdow = std::atoi(argv[2]);

        // Limit rowniez musi byc dodatni, bo zero przejazdow nic nie pokazuje.
        if (limitPrzejazdow <= 0) {
            std::fprintf(stderr, "Blad: limit przejazdow musi byc wiekszy od zera.\n");
            return 1;
        }
    }

    // Inicjalizujemy semafor kolejki A wartoscia 0, bo na start nikt nie jest budzony.
    sem_init(&semaforKolejkiA, 0, 0);

    // Inicjalizujemy semafor kolejki B wartoscia 0, bo na start nikt nie jest budzony.
    sem_init(&semaforKolejkiB, 0, 0);

    // Przygotowujemy miejsce na identyfikatory watkow.
    std::vector<pthread_t> watki(liczbaSamochodow);

    // Przygotowujemy dane samochodow; wektor ma staly rozmiar przed startem watkow.
    std::vector<DaneSamochodu> samochody(liczbaSamochodow);

    // Ustawiamy poczatkowa liczbe aut po obu stronach mostu.
    for (int i = 0; i < liczbaSamochodow; ++i) {
        // Numer samochodu pokazujemy od 1, bo tak jest czytelniej w komunikatach.
        int numerSamochodu = i + 1;

        // Zapamietujemy numer samochodu.
        samochody[i].id = numerSamochodu;

        // Przekazujemy do watku limit przejazdow.
        samochody[i].limitPrzejazdow = limitPrzejazdow;

        // Samochody parzyste startuja w miescie A.
        if (numerSamochodu % 2 == 0) {
            samochody[i].kierunekStartowy = A_DO_B;
            autaWMiescieA++;
        }
        // Samochody nieparzyste startuja w miescie B.
        else {
            samochody[i].kierunekStartowy = B_DO_A;
            autaWMiescieB++;
        }
    }

    // Pokazujemy stan poczatkowy przed uruchomieniem watkow.
    pthread_mutex_lock(&mutexStanu);
    wypiszStan();
    pthread_mutex_unlock(&mutexStanu);

    // Tworzymy po jednym watku dla kazdego samochodu.
    for (int i = 0; i < liczbaSamochodow; ++i) {
        // pthread_create uruchamia funkcje watekSamochodu jako nowy watek.
        int result = pthread_create(&watki[i], nullptr, watekSamochodu, &samochody[i]);

        // Jesli utworzenie watku sie nie powiedzie, konczymy program z bledem.
        if (result != 0) {
            std::fprintf(stderr, "Blad: nie udalo sie utworzyc watku samochodu %d.\n", samochody[i].id);
            return 1;
        }
    }

    // Czekamy na zakonczenie wszystkich watkow.
    // Bez limitu przejazdow program bedzie tutaj czekal bez konca.
    for (int i = 0; i < liczbaSamochodow; ++i) {
        pthread_join(watki[i], nullptr);
    }

    // Sprzatamy mutex po zakonczeniu wszystkich watkow.
    pthread_mutex_destroy(&mutexStanu);

    // Sprzatamy semafor kolejki A po zakonczeniu wszystkich watkow.
    sem_destroy(&semaforKolejkiA);

    // Sprzatamy semafor kolejki B po zakonczeniu wszystkich watkow.
    sem_destroy(&semaforKolejkiB);

    // Kod 0 oznacza poprawne zakonczenie programu.
    return 0;
}

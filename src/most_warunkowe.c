#define _DEFAULT_SOURCE // Udostepnia m.in. deklaracje usleep() w trybie C.

#include <pthread.h> // Biblioteka pthread: watki, mutexy i zmienne warunkowe.
#include <unistd.h>  // Funkcja usleep(), czyli proste opoznienia w mikrosekundach.

#include <stdbool.h> // Typ bool oraz wartosci true i false.
#include <stdio.h>   // Funkcje printf(), fprintf() i fflush().
#include <stdlib.h>  // Funkcje atoi(), malloc() i free().

// Kierunek jazdy samochodu przez most.
typedef enum Kierunek {
    A_DO_B, // Samochod jedzie z miasta A do miasta B.
    B_DO_A  // Samochod jedzie z miasta B do miasta A.
} Kierunek;

// Dane przekazywane do pojedynczego watku samochodu.
typedef struct DaneSamochodu {
    int id;                  // Numer samochodu widoczny w komunikatach.
    Kierunek kierunekStartowy; // Kierunek pierwszego przejazdu samochodu.
    int limitPrzejazdow;           // Limit przejazdow; -1 oznacza prace bez konca.
} DaneSamochodu;

// Mutex chroni wszystkie ponizsze zmienne globalne.
pthread_mutex_t mutexStanu = PTHREAD_MUTEX_INITIALIZER;

// Zmienna warunkowa pozwala usypiac auta czekajace na wolny most.
pthread_cond_t warunekMostu = PTHREAD_COND_INITIALIZER;

// Liczba samochodow w miescie A, ktore nie stoja jeszcze w kolejce.
int autaWMiescieA = 0;

// Liczba samochodow w miescie B, ktore nie stoja jeszcze w kolejce.
int autaWMiescieB = 0;

// Liczba samochodow czekajacych przed mostem od strony miasta A.
int kolejkaA = 0;

// Liczba samochodow czekajacych przed mostem od strony miasta B.
int kolejkaB = 0;

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
        printf("A-%d %d>>> [    -    ] <<<%d %d-B\n",
                    autaWMiescieA,
                    kolejkaA,
                    kolejkaB,
                    autaWMiescieB);
    }
    // Jezeli samochod jedzie z miasta A do miasta B, strzalki ida w prawo.
    else if (kierunekNaMoscie == A_DO_B) {
        printf("A-%d %d>>> [>> %d >>] <<<%d %d-B\n",
                    autaWMiescieA,
                    kolejkaA,
                    autoNaMoscie,
                    kolejkaB,
                    autaWMiescieB);
    }
    // Jezeli samochod jedzie z miasta B do miasta A, strzalki ida w lewo.
    else {
        printf("A-%d %d>>> [<< %d <<] <<<%d %d-B\n",
                    autaWMiescieA,
                    kolejkaA,
                    autoNaMoscie,
                    kolejkaB,
                    autaWMiescieB);
    }

    // Wymuszamy natychmiastowe wypisanie komunikatu na ekran.
    fflush(stdout);
}

// Sprawdza, czy samochod z podanego kierunku moze wjechac na most.
// Ta funkcja rowniez zaklada, ze mutex mutexStanu jest juz zablokowany.
bool czyMozeWjechacNaMost(Kierunek kierunek) {
    // Na moscie moze byc tylko jeden samochod, wiec zajety most blokuje wjazd.
    if (autoNaMoscie != -1) {
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

// Male opoznienie symulujace przebywanie samochodu w miescie.
useconds_t opoznienieWMiescie(int numerSamochodu) {
    // Rozne samochody maja lekko rozne czasy, zeby wyjscie bylo ciekawsze.
    return 120000 + (useconds_t)((numerSamochodu % 5) * 30000);
}

// Male opoznienie symulujace przejazd samochodu przez most.
useconds_t opoznienieNaMoscie(int numerSamochodu) {
    // Most jest krotki, ale przejazd nadal trwa chwile.
    return 160000 + (useconds_t)((numerSamochodu % 3) * 40000);
}

// Funkcja wykonywana przez kazdy watek samochodu.
void* watekSamochodu(void* argument) {
    // Odbieramy dane samochodu przekazane z funkcji main().
    DaneSamochodu* car = (DaneSamochodu*)argument;

    // Kazdy samochod pamieta swoj aktualny kierunek jazdy.
    Kierunek aktualnyKierunek = car->kierunekStartowy;

    // Liczymy, ile razy dany samochod przejechal juz przez most.
    int wykonanePrzejazdy = 0;

    // Petla trwa bez konca albo do osiagniecia opcjonalnego limitu przejazdow.
    while (car->limitPrzejazdow < 0 || wykonanePrzejazdy < car->limitPrzejazdow) {
        // Samochod przez chwile znajduje sie w miescie przed kolejna proba jazdy.
        usleep(opoznienieWMiescie(car->id));

        // Blokujemy mutex, bo zaraz zmienimy wspolny stan symulacji.
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

        // Czekamy, dopoki most nie jest wolny albo nie jest nasza kolej.
        while (!czyMozeWjechacNaMost(aktualnyKierunek)) {
            // pthread_cond_wait odblokowuje mutex na czas czekania.
            // Po obudzeniu ponownie blokuje mutex przed powrotem z funkcji.
            pthread_cond_wait(&warunekMostu, &mutexStanu);
        }

        // Skoro samochod moze wjechac, usuwamy go z odpowiedniej kolejki.
        if (aktualnyKierunek == A_DO_B) {
            kolejkaA--;
        } else {
            kolejkaB--;
        }

        // Oznaczamy, ktory samochod znajduje sie aktualnie na moscie.
        autoNaMoscie = car->id;

        // Zapamietujemy kierunek, zeby wypiszStan() pokazal dobre strzalki.
        kierunekNaMoscie = aktualnyKierunek;

        // Wypisujemy stan, bo zmienila sie kolejka i zawartosc mostu.
        wypiszStan();

        // Odblokowujemy mutex na czas faktycznego przejazdu przez most.
        pthread_mutex_unlock(&mutexStanu);

        // Symulujemy przejazd przez most.
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

        // Budzimy wszystkie czekajace watki, zeby mogly sprawdzic warunek wjazdu.
        pthread_cond_broadcast(&warunekMostu);

        // Konczymy aktualizacje wspolnego stanu.
        pthread_mutex_unlock(&mutexStanu);

        // Po dotarciu do drugiego miasta nastepny przejazd bedzie w druga strone.
        aktualnyKierunek = przeciwnyKierunek(aktualnyKierunek);
    }

    // Watek konczy prace po wykonaniu limitu przejazdow.
    return NULL;
}

// Funkcja wypisuje krotka instrukcje uruchomienia programu.
void wypiszUzycie(const char* nazwaProgramu) {
    // Pierwszy argument jest obowiazkowy, drugi jest tylko do testow.
    fprintf(stderr, "Uzycie: %s LICZBA_SAMOCHODOW [LIMIT_PRZEJAZDOW]\n", nazwaProgramu);
}

int main(int argc, char** argv) {
    // Program wymaga jednego albo dwoch argumentow.
    if (argc != 2 && argc != 3) {
        wypiszUzycie(argv[0]);
        return 1;
    }

    // atoi() zamienia tekst z argumentu linii polecen na liczbe calkowita.
    int liczbaSamochodow = atoi(argv[1]);

    // Liczba samochodow musi byc dodatnia.
    if (liczbaSamochodow <= 0) {
        fprintf(stderr, "Blad: liczba samochodow musi byc wieksza od zera.\n");
        return 1;
    }

    // Domyslnie program dziala bez konca, zgodnie z trescia zadania.
    int limitPrzejazdow = -1;

    // Jesli podano drugi argument, traktujemy go jako limit przejazdow.
    if (argc == 3) {
        limitPrzejazdow = atoi(argv[2]);

        // Limit rowniez musi byc dodatni, bo zero przejazdow nic nie pokazuje.
        if (limitPrzejazdow <= 0) {
            fprintf(stderr, "Blad: limit przejazdow musi byc wiekszy od zera.\n");
            return 1;
        }
    }

    // Przygotowujemy miejsce na identyfikatory watkow.
    pthread_t* watki = (pthread_t*)malloc(sizeof(pthread_t) * liczbaSamochodow);

    // Przygotowujemy dane samochodow; tablica ma staly rozmiar przed startem watkow.
    DaneSamochodu* samochody = (DaneSamochodu*)malloc(sizeof(DaneSamochodu) * liczbaSamochodow);

    // Sprawdzamy, czy system przydzielil pamiec dla obu tablic.
    if (watki == NULL || samochody == NULL) {
        fprintf(stderr, "Blad: nie udalo sie przydzielic pamieci.\n");
        free(watki);
        free(samochody);
        return 1;
    }

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
        int result = pthread_create(&watki[i], NULL, watekSamochodu, &samochody[i]);

        // Jesli utworzenie watku sie nie powiedzie, konczymy program z bledem.
        if (result != 0) {
            fprintf(stderr, "Blad: nie udalo sie utworzyc watku samochodu %d.\n", samochody[i].id);
            free(watki);
            free(samochody);
            return 1;
        }
    }

    // Czekamy na zakonczenie wszystkich watkow.
    // Bez limitu przejazdow program bedzie tutaj czekal bez konca.
    for (int i = 0; i < liczbaSamochodow; ++i) {
        pthread_join(watki[i], NULL);
    }

    // Sprzatamy mutex po zakonczeniu wszystkich watkow.
    pthread_mutex_destroy(&mutexStanu);

    // Sprzatamy zmienna warunkowa po zakonczeniu wszystkich watkow.
    pthread_cond_destroy(&warunekMostu);

    // Zwalniamy tablice watkow utworzona przez malloc().
    free(watki);

    // Zwalniamy tablice danych samochodow utworzona przez malloc().
    free(samochody);

    // Kod 0 oznacza poprawne zakonczenie programu.
    return 0;
}

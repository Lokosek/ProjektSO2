#include <pthread.h> // Biblioteka pthread: watki, mutexy i zmienne warunkowe.
#include <unistd.h>  // Funkcja usleep(), czyli proste opoznienia w mikrosekundach.

#include <cstdio>   // Funkcje printf() i fprintf().
#include <cstdlib>  // Funkcja atoi().
#include <vector>   // Wektor do przechowywania danych watkow.

// Kierunek jazdy samochodu przez most.
enum Direction {
    A_TO_B, // Samochod jedzie z miasta A do miasta B.
    B_TO_A  // Samochod jedzie z miasta B do miasta A.
};

// Dane przekazywane do pojedynczego watku samochodu.
struct CarData {
    int id;                  // Numer samochodu widoczny w komunikatach.
    Direction startDirection; // Kierunek pierwszego przejazdu samochodu.
    int tripsLimit;           // Limit przejazdow; -1 oznacza prace bez konca.
};

// Mutex chroni wszystkie ponizsze zmienne globalne.
pthread_mutex_t stateMutex = PTHREAD_MUTEX_INITIALIZER;

// Zmienna warunkowa pozwala usypiac auta czekajace na wolny most.
pthread_cond_t bridgeCondition = PTHREAD_COND_INITIALIZER;

// Liczba samochodow w miescie A, ktore nie stoja jeszcze w kolejce.
int carsInA = 0;

// Liczba samochodow w miescie B, ktore nie stoja jeszcze w kolejce.
int carsInB = 0;

// Liczba samochodow czekajacych przed mostem od strony miasta A.
int waitingA = 0;

// Liczba samochodow czekajacych przed mostem od strony miasta B.
int waitingB = 0;

// Numer samochodu na moscie; -1 oznacza, ze most jest pusty.
int carOnBridge = -1;

// Kierunek samochodu, ktory aktualnie znajduje sie na moscie.
Direction bridgeDirection = A_TO_B;

// Kierunek, ktory ma pierwszenstwo, gdy kolejka jest po obu stronach.
Direction turn = A_TO_B;

// Zwraca przeciwny kierunek jazdy.
Direction oppositeDirection(Direction direction) {
    // Jesli auto jechalo z A do B, nastepny przejazd bedzie z B do A.
    if (direction == A_TO_B) {
        return B_TO_A;
    }

    // W przeciwnym przypadku auto jechalo z B do A, wiec wroci z A do B.
    return A_TO_B;
}

// Funkcja wypisuje caly stan symulacji.
// Wywolujemy ja tylko wtedy, gdy mutex stateMutex jest juz zablokowany.
void printState() {
    // Jezeli nikt nie jedzie po moscie, pokazujemy puste miejsce w nawiasie.
    if (carOnBridge == -1) {
        std::printf("A-%d %d>>> [    -    ] <<<%d %d-B\n",
                    carsInA,
                    waitingA,
                    waitingB,
                    carsInB);
    }
    // Jezeli samochod jedzie z miasta A do miasta B, strzalki ida w prawo.
    else if (bridgeDirection == A_TO_B) {
        std::printf("A-%d %d>>> [>> %d >>] <<<%d %d-B\n",
                    carsInA,
                    waitingA,
                    carOnBridge,
                    waitingB,
                    carsInB);
    }
    // Jezeli samochod jedzie z miasta B do miasta A, strzalki ida w lewo.
    else {
        std::printf("A-%d %d>>> [<< %d <<] <<<%d %d-B\n",
                    carsInA,
                    waitingA,
                    carOnBridge,
                    waitingB,
                    carsInB);
    }

    // Wymuszamy natychmiastowe wypisanie komunikatu na ekran.
    std::fflush(stdout);
}

// Sprawdza, czy samochod z podanego kierunku moze wjechac na most.
// Ta funkcja rowniez zaklada, ze mutex stateMutex jest juz zablokowany.
bool canEnterBridge(Direction direction) {
    // Na moscie moze byc tylko jeden samochod, wiec zajety most blokuje wjazd.
    if (carOnBridge != -1) {
        return false;
    }

    // Samochod chce jechac z A do B.
    if (direction == A_TO_B) {
        // Jesli po stronie B nikt nie czeka, nie ma konfliktu o pierwszenstwo.
        if (waitingB == 0) {
            return true;
        }

        // Gdy obie strony czekaja, wpuszczamy strone, ktorej aktualnie kolej.
        return turn == A_TO_B;
    }

    // Samochod chce jechac z B do A.
    // Jesli po stronie A nikt nie czeka, nie ma konfliktu o pierwszenstwo.
    if (waitingA == 0) {
        return true;
    }

    // Gdy obie strony czekaja, wpuszczamy strone, ktorej aktualnie kolej.
    return turn == B_TO_A;
}

// Male opoznienie symulujace przebywanie samochodu w miescie.
useconds_t cityDelay(int carId) {
    // Rozne samochody maja lekko rozne czasy, zeby wyjscie bylo ciekawsze.
    return 120000 + static_cast<useconds_t>((carId % 5) * 30000);
}

// Male opoznienie symulujace przejazd samochodu przez most.
useconds_t bridgeDelay(int carId) {
    // Most jest krotki, ale przejazd nadal trwa chwile.
    return 160000 + static_cast<useconds_t>((carId % 3) * 40000);
}

// Funkcja wykonywana przez kazdy watek samochodu.
void* carThread(void* argument) {
    // Odbieramy dane samochodu przekazane z funkcji main().
    CarData* car = static_cast<CarData*>(argument);

    // Kazdy samochod pamieta swoj aktualny kierunek jazdy.
    Direction currentDirection = car->startDirection;

    // Liczymy, ile razy dany samochod przejechal juz przez most.
    int tripsDone = 0;

    // Petla trwa bez konca albo do osiagniecia opcjonalnego limitu przejazdow.
    while (car->tripsLimit < 0 || tripsDone < car->tripsLimit) {
        // Samochod przez chwile znajduje sie w miescie przed kolejna proba jazdy.
        usleep(cityDelay(car->id));

        // Blokujemy mutex, bo zaraz zmienimy wspolny stan symulacji.
        pthread_mutex_lock(&stateMutex);

        // Samochod jadacy z A do B opuszcza miasto A i ustawia sie w kolejce A.
        if (currentDirection == A_TO_B) {
            carsInA--;
            waitingA++;
        }
        // Samochod jadacy z B do A opuszcza miasto B i ustawia sie w kolejce B.
        else {
            carsInB--;
            waitingB++;
        }

        // Wypisujemy stan, bo zmienila sie liczba aut w miescie i w kolejce.
        printState();

        // Czekamy, dopoki most nie jest wolny albo nie jest nasza kolej.
        while (!canEnterBridge(currentDirection)) {
            // pthread_cond_wait odblokowuje mutex na czas czekania.
            // Po obudzeniu ponownie blokuje mutex przed powrotem z funkcji.
            pthread_cond_wait(&bridgeCondition, &stateMutex);
        }

        // Skoro samochod moze wjechac, usuwamy go z odpowiedniej kolejki.
        if (currentDirection == A_TO_B) {
            waitingA--;
        } else {
            waitingB--;
        }

        // Oznaczamy, ktory samochod znajduje sie aktualnie na moscie.
        carOnBridge = car->id;

        // Zapamietujemy kierunek, zeby printState() pokazal dobre strzalki.
        bridgeDirection = currentDirection;

        // Wypisujemy stan, bo zmienila sie kolejka i zawartosc mostu.
        printState();

        // Odblokowujemy mutex na czas faktycznego przejazdu przez most.
        pthread_mutex_unlock(&stateMutex);

        // Symulujemy przejazd przez most.
        usleep(bridgeDelay(car->id));

        // Po przejezdzie znow blokujemy mutex, bo aktualizujemy wspolny stan.
        pthread_mutex_lock(&stateMutex);

        // Most staje sie pusty.
        carOnBridge = -1;

        // Po przejechaniu z A do B samochod trafia do miasta B.
        if (currentDirection == A_TO_B) {
            carsInB++;

            // Jesli po stronie B ktos czeka, oddajemy mu pierwszenstwo.
            if (waitingB > 0) {
                turn = B_TO_A;
            }
        }
        // Po przejechaniu z B do A samochod trafia do miasta A.
        else {
            carsInA++;

            // Jesli po stronie A ktos czeka, oddajemy mu pierwszenstwo.
            if (waitingA > 0) {
                turn = A_TO_B;
            }
        }

        // Ten samochod zakonczyl jeden pelny przejazd przez most.
        tripsDone++;

        // Wypisujemy stan, bo samochod zniknal z mostu i pojawil sie w miescie.
        printState();

        // Budzimy wszystkie czekajace watki, zeby mogly sprawdzic warunek wjazdu.
        pthread_cond_broadcast(&bridgeCondition);

        // Konczymy aktualizacje wspolnego stanu.
        pthread_mutex_unlock(&stateMutex);

        // Po dotarciu do drugiego miasta nastepny przejazd bedzie w druga strone.
        currentDirection = oppositeDirection(currentDirection);
    }

    // Watek konczy prace po wykonaniu limitu przejazdow.
    return nullptr;
}

// Funkcja wypisuje krotka instrukcje uruchomienia programu.
void printUsage(const char* programName) {
    // Pierwszy argument jest obowiazkowy, drugi jest tylko do testow.
    std::fprintf(stderr, "Uzycie: %s LICZBA_SAMOCHODOW [LIMIT_PRZEJAZDOW]\n", programName);
}

int main(int argc, char** argv) {
    // Program wymaga jednego albo dwoch argumentow.
    if (argc != 2 && argc != 3) {
        printUsage(argv[0]);
        return 1;
    }

    // atoi() zamienia tekst z argumentu linii polecen na liczbe calkowita.
    int numberOfCars = std::atoi(argv[1]);

    // Liczba samochodow musi byc dodatnia.
    if (numberOfCars <= 0) {
        std::fprintf(stderr, "Blad: liczba samochodow musi byc wieksza od zera.\n");
        return 1;
    }

    // Domyslnie program dziala bez konca, zgodnie z trescia zadania.
    int tripsLimit = -1;

    // Jesli podano drugi argument, traktujemy go jako limit przejazdow.
    if (argc == 3) {
        tripsLimit = std::atoi(argv[2]);

        // Limit rowniez musi byc dodatni, bo zero przejazdow nic nie pokazuje.
        if (tripsLimit <= 0) {
            std::fprintf(stderr, "Blad: limit przejazdow musi byc wiekszy od zera.\n");
            return 1;
        }
    }

    // Przygotowujemy miejsce na identyfikatory watkow.
    std::vector<pthread_t> threads(numberOfCars);

    // Przygotowujemy dane samochodow; wektor ma staly rozmiar przed startem watkow.
    std::vector<CarData> cars(numberOfCars);

    // Ustawiamy poczatkowa liczbe aut po obu stronach mostu.
    for (int i = 0; i < numberOfCars; ++i) {
        // Numer samochodu pokazujemy od 1, bo tak jest czytelniej w komunikatach.
        int carId = i + 1;

        // Zapamietujemy numer samochodu.
        cars[i].id = carId;

        // Przekazujemy do watku limit przejazdow.
        cars[i].tripsLimit = tripsLimit;

        // Samochody parzyste startuja w miescie A.
        if (carId % 2 == 0) {
            cars[i].startDirection = A_TO_B;
            carsInA++;
        }
        // Samochody nieparzyste startuja w miescie B.
        else {
            cars[i].startDirection = B_TO_A;
            carsInB++;
        }
    }

    // Pokazujemy stan poczatkowy przed uruchomieniem watkow.
    pthread_mutex_lock(&stateMutex);
    printState();
    pthread_mutex_unlock(&stateMutex);

    // Tworzymy po jednym watku dla kazdego samochodu.
    for (int i = 0; i < numberOfCars; ++i) {
        // pthread_create uruchamia funkcje carThread jako nowy watek.
        int result = pthread_create(&threads[i], nullptr, carThread, &cars[i]);

        // Jesli utworzenie watku sie nie powiedzie, konczymy program z bledem.
        if (result != 0) {
            std::fprintf(stderr, "Blad: nie udalo sie utworzyc watku samochodu %d.\n", cars[i].id);
            return 1;
        }
    }

    // Czekamy na zakonczenie wszystkich watkow.
    // Bez limitu przejazdow program bedzie tutaj czekal bez konca.
    for (int i = 0; i < numberOfCars; ++i) {
        pthread_join(threads[i], nullptr);
    }

    // Sprzatamy mutex po zakonczeniu wszystkich watkow.
    pthread_mutex_destroy(&stateMutex);

    // Sprzatamy zmienna warunkowa po zakonczeniu wszystkich watkow.
    pthread_cond_destroy(&bridgeCondition);

    // Kod 0 oznacza poprawne zakonczenie programu.
    return 0;
}

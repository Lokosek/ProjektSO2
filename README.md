# Projekt SO2 - Waski most

Projekt realizuje temat 2 z zajec: symulacje waskiego mostu laczacego miasta A i B.

Most jest waski i slaby, dlatego w jednym momencie moze znajdowac sie na nim tylko jeden samochod. Kazdy samochod jest osobnym watkiem i nieustannie probuje przejezdzac miedzy miastami.

## Uzyte mechanizmy

- `pthread_create()` tworzy watki samochodow.
- `pthread_join()` czeka na zakonczenie watkow.
- `pthread_mutex_lock()` i `pthread_mutex_unlock()` chronia wspolny stan programu.
- `pthread_cond_wait()` usypia samochody, ktore czekaja na wolny most.
- `pthread_cond_broadcast()` budzi samochody po zwolnieniu mostu.
- `usleep()` symuluje czas pobytu w miescie i czas przejazdu przez most.
- `atoi()` odczytuje liczby z argumentow linii polecen.

## Kompilacja

```bash
make
```

Powstanie program:

```bash
./bridge
```

## Uruchomienie

Program wymaga liczby samochodow:

```bash
./bridge 10
```

W tej wersji program dziala bez konca, zgodnie z trescia zadania.

Do testow mozna podac opcjonalny limit przejazdow kazdego samochodu:

```bash
./bridge 10 20
```

To znaczy: utworz 10 samochodow, a kazdy samochod ma przejechac przez most 20 razy.

## Format komunikatow

Przykladowy komunikat:

```text
A-5 10>>> [>> 4 >>] <<<4 6-B
```

Znaczenie:

- `A-5` oznacza 5 samochodow w miescie A, ktore nie stoja w kolejce.
- `10>>>` oznacza 10 samochodow czekajacych od strony miasta A.
- `[>> 4 >>]` oznacza, ze samochod numer 4 jedzie po moscie z A do B.
- `<<<4` oznacza 4 samochody czekajace od strony miasta B.
- `6-B` oznacza 6 samochodow w miescie B, ktore nie stoja w kolejce.

Gdy most jest pusty, srodek komunikatu wyglada tak:

```text
[    -    ]
```

## Jak dziala synchronizacja

Program trzyma caly stan symulacji w zmiennych globalnych, takich jak `carsInA`, `carsInB`, `waitingA`, `waitingB` i `carOnBridge`.

Dostep do tych zmiennych jest chroniony jednym mutexem `stateMutex`. Dzieki temu dwa watki nie zmieniaja jednoczesnie tych samych licznikow.

Zmienna warunkowa `bridgeCondition` pozwala samochodom czekac bez aktywnego krecenia sie w petli. Samochod czeka w `pthread_cond_wait()`, dopoki most jest zajety albo dopoki pierwszenstwo ma druga strona.

Zmienna `turn` przechowuje kierunek, ktory ma pierwszenstwo, gdy kolejki sa po obu stronach mostu. Po przejezdzie samochodu program oddaje pierwszenstwo przeciwnej stronie, jesli ktos tam czeka. To ogranicza ryzyko zaglodzenia jednej strony.

## Czyszczenie

```bash
make clean
```

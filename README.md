# Projekt SO2 - Waski most

Projekt realizuje temat 2 z zajec: symulacje waskiego mostu laczacego miasta A i B.

Most jest waski i slaby, dlatego w jednym momencie moze znajdowac sie na nim tylko jeden samochod. Kazdy samochod jest osobnym watkiem i nieustannie probuje przejezdzac miedzy miastami.

W repozytorium sa przygotowane dwa warianty programu:

- `most_semafory` - wariant a), bez zmiennych warunkowych, tylko mutex i semafory.
- `most_warunkowe` - wariant b), z wykorzystaniem zmiennych warunkowych.

## Uzyte mechanizmy

Oba warianty uzywaja:

- `pthread_create()` do tworzenia watkow samochodow.
- `pthread_join()` do czekania na zakonczenie watkow.
- `pthread_mutex_lock()` i `pthread_mutex_unlock()` do ochrony wspolnego stanu programu.
- `usleep()` do symulowania czasu pobytu w miescie i czasu przejazdu przez most.
- `atoi()` do odczytywania liczb z argumentow linii polecen.

Wariant a), czyli `most_semafory`, dodatkowo uzywa:

- `sem_init()` do inicjalizacji semaforow.
- `sem_wait()` do usypiania samochodow czekajacych w kolejce.
- `sem_post()` do budzenia jednego samochodu po zwolnieniu mostu.
- `sem_destroy()` do sprzatania semaforow.

Wariant b), czyli `most_warunkowe`, dodatkowo uzywa:

- `pthread_cond_wait()` do usypiania samochodow czekajacych na most.
- `pthread_cond_broadcast()` do budzenia samochodow po zwolnieniu mostu.
- `pthread_cond_destroy()` do sprzatania zmiennej warunkowej.

## Kompilacja

```bash
make
```

Powstana dwa programy:

```bash
./most_semafory
./most_warunkowe
```

## Uruchomienie

Program wymaga liczby samochodow.

Wariant bez zmiennych warunkowych:

```bash
./most_semafory 10
```

Wariant ze zmiennymi warunkowymi:

```bash
./most_warunkowe 10
```

Bez drugiego argumentu program dziala bez konca, zgodnie z trescia zadania.

Do testow mozna podac opcjonalny limit przejazdow kazdego samochodu:

```bash
./most_semafory 10 20
./most_warunkowe 10 20
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

Program trzyma caly stan symulacji w zmiennych globalnych, takich jak `autaWMiescieA`, `autaWMiescieB`, `kolejkaA`, `kolejkaB` i `autoNaMoscie`.

Dostep do tych zmiennych jest chroniony jednym mutexem `mutexStanu`. Dzieki temu dwa watki nie zmieniaja jednoczesnie tych samych licznikow.

W wariancie semaforowym samochody czekaja na dwoch semaforach: osobnym dla kolejki od strony A i osobnym dla kolejki od strony B. Po zwolnieniu mostu program wybiera jedna strone, rezerwuje wjazd dla jednego samochodu i budzi dokladnie jeden watek przez `sem_post()`.

W wariancie ze zmiennymi warunkowymi samochody czekaja w `pthread_cond_wait()`. Po zwolnieniu mostu program budzi czekajace watki przez `pthread_cond_broadcast()`, a kazdy obudzony watek ponownie sprawdza, czy moze wjechac.

Zmienna `czyjaKolej` przechowuje kierunek, ktory ma pierwszenstwo, gdy kolejki sa po obu stronach mostu. Po przejezdzie samochodu program oddaje pierwszenstwo przeciwnej stronie, jesli ktos tam czeka. To ogranicza ryzyko zaglodzenia jednej strony.

## Czyszczenie

```bash
make clean
```

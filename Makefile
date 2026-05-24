CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pthread
TARGETS = most_warunkowe most_semafory

.PHONY: all clean

all: $(TARGETS)

most_warunkowe: src/most_warunkowe.c
	$(CC) $(CFLAGS) src/most_warunkowe.c -o most_warunkowe

most_semafory: src/most_semafory.c
	$(CC) $(CFLAGS) src/most_semafory.c -o most_semafory

clean:
	rm -f $(TARGETS) bridge bridge_condition bridge_semaphore

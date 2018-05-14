# autor - Petr Zubalik
# email - xzubal04@stud.fit.vutbr.cz
# datum - 27. 4. 2017
# projekt - IOS, projekt 2
# soubor - proj2.c

CC=gcc
CFLAGS= -std=gnu99 -Wall -Wextra -Werror -pedantic -pthread

all: proj2

proj2: proj2.o 
	$(CC) $(CFLAGS) proj2.o -o proj2

proj2.o: proj2.c 
	$(CC) $(CFLAGS) -c proj2.c

clean:
	rm -f proj2 *.o proj2.zip proj2.out

zip:
	zip proj2.zip proj2.c proj2.h Makefile

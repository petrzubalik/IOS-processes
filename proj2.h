/**
 * autor - Petr Zubalik
 * email - xzubal04@stud.fit.vutbr.cz
 * datum - 27. 4. 2017
 * projekt - IOS, projekt 2
 * soubor - proj2.h
**/

#ifndef PROJ2_H
#define PROJ2_H

void set_memory();

void set_semaphores();

void clean_memory(int);

void clean_semaphores(int);

void clean_resources();

void child(int, FILE *);

void adult(int, FILE *);


#endif /* PROJ2_H */
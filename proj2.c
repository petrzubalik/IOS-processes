/**
 * autor - Petr Zubalik
 * email - xzubal04@stud.fit.vutbr.cz
 * datum - 27. 4. 2017
 * projekt - IOS, projekt 2
 * soubor - proj2.c
**/



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include "proj2.h"

#define SEM_MUTEX "/xzubal04_mutex"
#define SEM_CHILDREN_QUEUE "/xzubal04_children_queue"
#define SEM_ADULTS_QUEUE "/xzubal04_adults_queue"
#define SEM_FINISH "/xzubal04_finish"
#define SEM_PRINT "/xzubal04_print"

/* globalni promenne */
int children = 0; 
int * children_ptr = NULL; 	// pocet deti v centru

int adults = 0;
int * adults_ptr = NULL;	// pocet dospelych v centru

int waiting = 0;
int * waiting_ptr = NULL;	// pocet deti cekajich na vstup do centra

int leaving = 0;
int * leaving_ptr = NULL;	// pocet dospelych cekajicich na odchod z centra

int event_counter = 0;
int *event_counter_ptr = NULL;	// pocet udalosti, ktere se v prubehu staly

int adult_working_time;			// maximalni cas, po ktery budou dospely v centru
int child_working_time;			// maximalni cas, po ktery budou deti v centru
int let_in_count = 0;			// pocet deti, ktere maji byt vpusteny po prichodu dospeleho

int remaining_adults = 0;		
int * remaining_adults_ptr = NULL;	// pocet dospelych, kteri jeste neodesli z centra

int remaining_processes = 0;
int * remaining_processes_ptr = NULL;	// pocet procesu, ktere se jeste neukoncili

//semafory
sem_t *child_queue, *adult_queue, *mutex, *finish, *sem_print;		// semafor pro: cekajici deti, cekajici dospele, pristup do pameti
																	// 				ukonceni vsech procesu, tisk zprav 

/* ****************************************************************************** */

// funkce pro ziskani sdilene pameti
// kontrola spravnosti systemoveho volani (pripadne ukonceni programu)
void set_memory()
{	
	if ((children = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0)	// alokuje sdilenou pamet + kontrola spravnosti
	{	
		perror("shmget");		// tiskne chybove hlaseni
		exit(2);				// ukonceni s chybovym kodem 2
	}
	if ((children_ptr = (int *) shmat(children, NULL, 0)) == (void *) -1)		// pripoji promennou k dane sdilene pameti + kontrola spravnosti
	{	
		perror("smhat");		// tiskne chybove hlaseni
		exit(2);				// ukonceni s chybovym kodem 2
	}
	
	// pro vsechny promenne stejny postup

	if ((adults = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0)
	{
		clean_memory(1);
		perror("shmget");
		exit(2);
	}
	if ((adults_ptr = (int *) shmat(adults, NULL, 0)) == (void *) -1)
	{	
		clean_memory(1);
		perror("shmat");
		exit(2);
	}
	if ((waiting = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0)
	{	
		clean_memory(2);
		perror("shmget");
		exit(2);
	}
	if ((waiting_ptr = (int *) shmat(waiting, NULL, 0)) == (void *) -1)
	{
		clean_memory(2);
		perror("shmat");
		exit(2);
	}
	if ((leaving = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0)
	{
		clean_memory(3);
		perror("shmget");
		exit(2);
	}
	if ((leaving_ptr = (int *) shmat(leaving, NULL, 0)) == (void *) -1)
	{
		clean_memory(3);
		perror("shmat");
		exit(2);
	}
	if ((event_counter = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0)
	{	
		clean_memory(4);
		perror("shmget");
		exit(2);
	}
	if ((event_counter_ptr = (int *) shmat(event_counter, NULL, 0)) == (void *) -1) 
	{	
		clean_memory(4);
		perror("shmat");
		exit(2);
	}
	if ((remaining_adults = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0)
	{
		clean_memory(5);
		perror("shmget");
		exit(2);
	}
	if ((remaining_adults_ptr = shmat(remaining_adults, NULL, 0)) == (void *) -1)
	{
		clean_memory(5);
		perror("shmat");
		exit(2);
	}
	if ((remaining_processes = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) < 0)
	{
		clean_memory(6);
		perror("shmget");
		exit(2);
	}
	if ((remaining_processes_ptr = shmat(remaining_processes, NULL, 0)) == (void *) - 1)
	{
		clean_memory(6);
		perror("shmat");
		exit(2);
	}
}

/* **************************************************************************************** */ 

// inicializace semaforu + kontrola spravnosti 
// pokud dojde k chybe, ukonceni programu
void set_semaphores()
{
	child_queue = sem_open(SEM_CHILDREN_QUEUE, O_CREAT, 0666, 0);	// inicializuje a otevre semafor
	if (child_queue == SEM_FAILED)
	{
		perror("sem_open");		// tiskne chybove hlaseni
		clean_memory(7);		// vycisti pamet
		exit(2);				// ukonceni s chybovym kodem 2
	} 
	sem_close(child_queue);		// uzavre semafor  

	// pro vsechny semafory stejny postup

	adult_queue = sem_open(SEM_ADULTS_QUEUE, O_CREAT, 0666, 0);
	if (adult_queue == SEM_FAILED)
	{
		perror("sem_open");
		clean_memory(7);
		clean_semaphores(1);
		exit(2);
	}
	sem_close(adult_queue);

	mutex = sem_open(SEM_MUTEX, O_CREAT, 0666, 1);
	if (mutex == SEM_FAILED)
	{
		perror("sem_open");
		clean_memory(7);
		clean_semaphores(2);
		exit(2);
	}
	sem_close(mutex);

	finish = sem_open(SEM_FINISH, O_CREAT, 0666, 0);
	if (finish == SEM_FAILED)
	{
		perror("sem_open");
		clean_memory(7);
		clean_semaphores(3);
		exit(2);
	}
	sem_close(finish);

	sem_print = sem_open(SEM_PRINT, O_CREAT, 0666, 1);
	if (sem_print == SEM_FAILED)
	{
		perror("sem_open");
		clean_memory(7);
		clean_semaphores(4);
		exit(2);
	}
	sem_close(sem_print);
}

/* ******************************************************************** */

// funkce uvolni pamet, ktera byla alokovana pro sdilene promenne
// parametr rika, kolik promennych se ma uvolnit
void clean_memory(int err_count)
{	
	if (err_count >= 1)
	{	
		shmdt(children_ptr);				// 'odpoji' promennou od pameti
		shmctl(children, IPC_RMID, NULL);	// odstrani sdilenou pamet
	}

	// pro vsechny promenne stejny postup

	if (err_count >= 2)
	{
		shmdt(adults_ptr);
		shmctl(adults, IPC_RMID, NULL);
	}
	if (err_count >= 3)
	{	
		shmdt(waiting_ptr);
		shmctl(waiting, IPC_RMID, NULL);
	}
	if (err_count >= 4)
	{	
		shmdt(leaving_ptr);	
		shmctl(leaving, IPC_RMID, NULL);
	}
	if (err_count >= 5)
	{
		shmdt(event_counter_ptr);
		shmctl(event_counter, IPC_RMID, NULL);
	}
	if (err_count >= 6)
	{
		shmdt(remaining_adults_ptr);
		shmctl(remaining_adults, IPC_RMID, NULL);
	}
	if (err_count >= 7)
	{
		shmdt(remaining_processes_ptr);
		shmctl(remaining_processes, IPC_RMID, NULL);
	}
}

/* ****************************************************************** */

//funkce uvolni semafory, ktere byly inicializovany
// parametr rika, kolik semaforu se ma uvolnit
void clean_semaphores(int err_count)
{	
	if (err_count >= 1)
	{	
		sem_unlink(SEM_CHILDREN_QUEUE);		// uvolni dany semafor
	}

	// pro vsechny semafory stejny postup

	if (err_count >= 2)
	{
		sem_unlink(SEM_ADULTS_QUEUE);
	}
	if (err_count >= 3)
	{
		sem_unlink(SEM_MUTEX);
	}
	if (err_count >= 4)
	{
		sem_unlink(SEM_FINISH);
	}

	if (err_count >= 5)
	{
		sem_unlink(SEM_PRINT);
	}
}

/* ********************************************************************* */

// funkce zdroje, ktere jsme potrebovali pro beh programu
void clean_resources()
{
	clean_memory(7);		// uvolni pamet
	clean_semaphores(5);	// uvolni semafory
}

// funkce reprezentujici chovani 'adult' procesu
// param1 rika, o kolikaty 'adult' proces se jedna
// param2 do ktereho souboru se bude zapisovat
void adult(int number, FILE *f)
{	// otevre semafory potrebne pro tuto funkci
	adult_queue = sem_open(SEM_ADULTS_QUEUE, O_RDWR);		
	child_queue = sem_open(SEM_CHILDREN_QUEUE, O_RDWR);
	mutex = sem_open(SEM_MUTEX, O_RDWR);
	sem_print = sem_open(SEM_PRINT, O_RDWR);
	finish = sem_open(SEM_FINISH, O_RDWR);

	sem_wait(mutex);		// uzavre sdilenou pamet
	
	(*adults_ptr)++;				// inkrementace poctu 'adult' procesu
	sem_wait(sem_print);			// uzavre semafor pro tisk
	(*event_counter_ptr)++;			// inkrementace poctu akci
	fprintf(f, "%d\t: A %d\t: enter\n", *event_counter_ptr, number);		// tisk zpravy
	sem_post(sem_print);			// otevreni semaforu pro tisk

	// pokud jsou nejaci cekajici, vpustim je do centra po prichodu dospeleho
	if (*waiting_ptr)				
	{	// vypocet, kolik deti muzu pustit
		let_in_count = (3 < (*waiting_ptr) ? 3 : *waiting_ptr);
		for (int i = 0; i < let_in_count; i++)
		{	// pousteni deti do centra
			sem_post(child_queue);
		}
	}

	sem_post(mutex);		// uvolni sdilenou pamet

	// simulace aktivity dospeleho v centru
	if (adult_working_time > 0)
	{
		useconds_t waiting_time_adult = random() % adult_working_time * 1000;
		usleep(waiting_time_adult);
	}

	sem_wait(mutex);			// uzavre sdilenou pamet
	sem_wait(sem_print);		// uzavre semafor pro tisk
	(*event_counter_ptr)++;		// inkrementace poctu akci
	fprintf(f, "%d\t: A %d\t: trying to leave\n", *event_counter_ptr, number);			// tisk zpravy
	sem_post(sem_print);		// otevre semafor pro tisk

	// pokud je v centru dostatecny pocet 'adult' procesu muzu odejit
	if ((*children_ptr) <= 3 * ((*adults_ptr)  - 1)) 	
	{	
		(*remaining_adults_ptr)--;		// dekrementace poctu 'adult' procesu, kteri maji jeste opustit centrum
		(*adults_ptr)--;				// dekrementace poctu 'adult' procesu, kteri jsou v centru
		
		 sem_wait(sem_print);			// uzavre semafor pro tisk
		(*event_counter_ptr)++;			// inkrementace poctu akci
		fprintf(f, "%d\t: A %d\t: leave\n", *event_counter_ptr, number);		// tisk zpravy
		sem_post(sem_print);			// otevre semafor pro tisk
		
		(*remaining_processes_ptr)--;		// dekrementace poctu procesu, ktere se maji dokoncit
		if (*remaining_adults_ptr == 0)
		{
			for (int i = 0; i < *waiting_ptr; i++)
			{
				sem_post(child_queue);
			}
		}
		
		sem_post(mutex);		// uvolni sdilenou pamet
	} else 
	{	// jinak musim cekat, dokud neodejde prislusny pocet deti
		(*leaving_ptr)++;		// inkrementace poctu 'adult' procesu, cekajicich na odchod

		sem_wait(sem_print);		// uzavre semafor pro tisk
		(*event_counter_ptr)++;		// inkrementace poctu akci
		fprintf(f, "%d\t: A %d\t: waiting: %d: %d\n", *event_counter_ptr, number, *adults_ptr ,*children_ptr);		// tisk zpravy
		sem_post(sem_print);		// otevre semafor pro tisk

		sem_post(mutex);		// uvolni sdilenou pamet
		sem_wait(adult_queue);	// uzavre semafor cekajicih na odchod

		sem_wait(mutex);				// zamkne sdilenou pamet
		(*remaining_adults_ptr)--;		// dekrementace poctu zbyvajicih 'adult' procesu
		(*adults_ptr)--;				// dekrementace poctu 'adult' procesu, kteri jsou v centru
		(*leaving_ptr)--;				// dekrementace poctu 'adult' procesu, cekajicich na odchod

		sem_wait(sem_print);			// uzavre semafor pro tisk
		(*event_counter_ptr)++;			// inkrementace poctu akci
		fprintf(f, "%d\t: A %d\t: leave\n", *event_counter_ptr, number);		// tisk zpravy
		(*remaining_processes_ptr)--;		// dekrementace poctu procesu, ktere se maji dokoncit

		if (*remaining_adults_ptr == 0)
		{
			for (int i = 0; i < *waiting_ptr; i++)
			{
				sem_post(child_queue);
			}
		}
		
		sem_post(sem_print);				// uvolni semafor pro tisk
		sem_post(mutex);					// uvolni sdilenou pamet
	}

	// pokud dobehly vsehny procesy, muzu je ukoncit
	if (*remaining_processes_ptr == 0)
	{	// otevreni semafory pro ukonceni
		sem_post(finish);
	}
	// uzavreni semaforu pro ukonceni
	sem_wait(finish);
	(*event_counter_ptr)++;			// inkrementace poctu akci
	fprintf(f, "%d\t: A %d\t: finished\n", *event_counter_ptr, number);		// tisk zpravy
	sem_post(finish);		// uvolneni semaforu pro ukonceni
	
	// uplne uzavreni semaforu potrebnych pro funkci
	sem_close(finish);	
	sem_close(sem_print);
	sem_close(mutex);
	sem_close(child_queue);
	sem_close(adult_queue);

	exit(0);		// ukonceni procesu

}

// funkce reprezentujici chovani 'child' procesu
// parametr rika, o kolikaty 'child' proces se jedna
// param2 do ktereho souboru se bude zapisovat
void child(int number, FILE *f)
{	// otevreni semaforu potrebnych pro funkci
	adult_queue = sem_open(SEM_ADULTS_QUEUE, O_RDWR);
	child_queue = sem_open(SEM_CHILDREN_QUEUE, O_RDWR);
	mutex = sem_open(SEM_MUTEX, O_RDWR);
	sem_print = sem_open(SEM_PRINT, O_RDWR);
	finish = sem_open(SEM_FINISH, O_RDWR);

	sem_wait(mutex);		// uzavre sdilenou pamet
	
	// pokud je v centru dostatek 'adult' procesu, muzu vstoupit
	if (*children_ptr < 3 * (*adults_ptr) || *remaining_adults_ptr == 0)	// pokud uz nema prijit zadny dospeli, muzu vstoupit taky
	{
		(*children_ptr)++;		// inkrementace poctu deti v centru

		sem_wait(sem_print);		// uzavre semafor pro tisk
		(*event_counter_ptr)++;		// inkrementace poctu akci
		fprintf(f, "%d\t: C %d\t: enter\n", *event_counter_ptr, number);		// tisk zpravy
		sem_post(sem_print);		// otevre semafor pro tisk

		sem_post(mutex);			// otevre sdilenou pamet
	} else 
	{	// jinak musim cekat, az nejaky dospely prijde
		sem_wait(sem_print);		// otevre semafor pro tisk
		(*event_counter_ptr)++;		// inkrementace poctu akci
		(*waiting_ptr)++;			// inkrementace poctu cekajich 'child' procesu na vstup
		fprintf(f, "%d\t: C %d\t: waiting: %d: %d\n", *event_counter_ptr, number, *adults_ptr, *children_ptr);		// tisk zpravy
		sem_post(sem_print);		// otevre semafor pro tisk

		sem_post(mutex);			// otevre sdilenou pamet
		sem_wait(child_queue);		// uzavre centrum pro 'child' proces

		sem_wait(mutex);			// uzavre sdilenou pamet

		(*children_ptr)++;			// inkrementace poctu 'child' procesu v centru
		(*waiting_ptr)--;			// dekrementace poctu 'child' procesu cekajicich na vstup

		sem_wait(sem_print);		// uzavre semafor pro tisk
		(*event_counter_ptr)++;		// inkrementace poctu akci
		fprintf(f, "%d\t: C %d\t: enter\n", *event_counter_ptr, number);		// tisk zpravy
		sem_post(sem_print);		// otevre semafor pro tisk

		sem_post(mutex);			// otevre sdilenou pamet
	}
	
	// simulace aktivity 'child' procesu v centru
	if (child_working_time > 0)
	{
		useconds_t waiting_time_children = random() % child_working_time * 1000;
		usleep(waiting_time_children);
	}

	sem_wait(mutex);				// uzavre sdilenou pamet

	sem_wait(sem_print);			// uzavre semafor pro tisk
	(*event_counter_ptr)++;			// inkrementace poctu akci
	fprintf(f, "%d\t: C %d\t: trying to leave\n", *event_counter_ptr, number);		// tisk zpravy
	(*event_counter_ptr)++;			// inkrementace poctu akci
	fprintf(f, "%d\t: C %d\t: leave\n", *event_counter_ptr, number);		// tisk zpravy
	(*remaining_processes_ptr)--;		// dekrementace poctu procesu, ktere se maji ukoncit
	sem_post(sem_print);				// otevreni semaforu pro tisk

	(*children_ptr)--;					// dekrementace poctu 'child' procesu v centru
	
	// pokud odejde urcity pocet deti, pustim ven i dospeleho
	if ((*leaving_ptr != 0) && (*children_ptr <= (3 * (*adults_ptr - 1) )))
	{	// pousteni dospeleho ven
		sem_post(adult_queue);
	}

	sem_post(mutex);	// otevre sdilenou pamet 

	// pokud dobehly vsechny procesy, muzu je ukoncit
	if (*remaining_processes_ptr == 0)
	{	// otevreni semaforu pro ukonceni procesu
		sem_post(finish);
	}

	sem_wait(finish);		// uzavreni semaforu pro ukonceni procesu

	(*event_counter_ptr)++;		// inkrementace poctu akci
	fprintf(f, "%d\t: C %d\t: finished\n", *event_counter_ptr, number);		// tisk zpravy
	sem_post(finish);			// otevreni semaforu pro ukonceni
	
	// uplne uzavreni semaforu, potrebnych pro funkci
	sem_close(finish);
	sem_close(sem_print);
	sem_close(mutex);
	sem_close(child_queue);
	sem_close(adult_queue);

	exit(0);	// ukonceni procesu

}

int main(int argc, char const *argv[])
{	
	// vyprazdneni bufferu
	setbuf(stderr, NULL);

	// pocty procesu ('adult' a 'child')
	int adults_count;
	int children_count;
	// doba, po ktere se ma generovat dalsi proces
	int adult_generate_time;
	int child_generate_time;

	// zpracovani argumentu
	if (argc != 7)
	{
		fprintf(stderr, "Nebyl zadan pozadovany pocet argumentu!\n");
		exit(1);
	} else {

		adults_count = atoi(argv[1]);
		if (adults_count <= 0)
		{
			fprintf(stderr, "Pocet procesu 'adult' musi byt kladne cele cislo!\n");
			exit(1);
		}

		children_count = atoi(argv[2]);
		if (children_count <= 0)
		{
			fprintf(stderr, "Pocet procesu 'child' musi byt kladne cele cislo!\n");
			exit(1);
		}

		adult_generate_time = atoi(argv[3]);
		if (adult_generate_time < 0 || adult_generate_time > 5000)
		{
		 	fprintf(stderr, "Byl zadan spatny cas pro generovani 'adult' procesu!\n");
		 	exit(1);
		}

		child_generate_time = atoi(argv[4]);
		if (child_generate_time < 0 || child_generate_time > 5000)
		{
			fprintf(stderr, "Byl zadan spatny cas pro generovani 'child' procesu!\n");
			exit(1);
		} 

		adult_working_time = atoi(argv[5]);
		if (adult_working_time < 0 || adult_working_time > 5000)
		{
			fprintf(stderr, "Byla zadana spatna pracovni doba pro procesy 'adult'!\n");
			exit(1);
		}

		child_working_time = atoi(argv[6]);
		if (child_working_time < 0 || child_working_time > 5000)
		{
			fprintf(stderr, "Byla zadana spatna pracovni doba pro procesy 'child'!\n");
			exit(1);
		}
	}

	// pomocne promenne pro pid procesu
	int pid = 0;
	int pid_child = 0;
	int pid_adult = 0;

	// soubor, do ktereho se bude zapisovat
	FILE *f;
	f = fopen("proj2.out", "w+");

	// vyprazdneni bufferu pro soubor
	setbuf(f, NULL);
	


	// pole pid procesu, ktere budu chtit 'zabit'
	pid_t children_pids[children_count];
	pid_t adults_pids[adults_count];

	// alokovani pameti a inicializace semaforu
	set_memory();
	set_semaphores();
	// otevreni semaforu pro tisk
	sem_open(SEM_PRINT, O_RDWR);

	*remaining_adults_ptr = adults_count;		// pocet 'adult' procesu, ktere se maji vygenerovat
	*remaining_processes_ptr = adults_count + children_count;		// pocet vsech procesu

	// pomocne promenne pro ulozeni casu, po kterem se maji generovat procesy
	useconds_t generating_time_children;
	useconds_t generating_time_adults;

	// prvni vygenerovani detskeho procesu
	if ((pid = fork()) < 0)
	{	// pokud se to nepodari, uvolnim zdroje a koncim
		clean_resources();	
		perror("fork");
		exit(2);
	} else if (pid == 0)	
	{	// detsky proces ktery vytvari 'children'
		for (int i = 0; i < children_count; i++)
		{	
			// cekaci doba pro vytvoreni noveho 'child' procesu (* 1000 protoze chceme ms)
			if (child_generate_time > 0)
			{
				generating_time_children = random() % child_generate_time * 1000;
				usleep(generating_time_children);
			}
			pid_child = fork();		// samotne vytvoreni 'child' procesu
			if (pid_child == 0)		// pro 'child proces'
			{	
				sem_wait(sem_print);			// uzavreni semaforu pro tisk
				(*event_counter_ptr)++;			// inkrementace poctu akci
				fprintf(f, "%d\t: C %d\t: started\n", *event_counter_ptr, i+1);			// tisk zpravy
				sem_post(sem_print);			// otevreni semaforu pro tisk

				child(i+1, f);						// zavolani funkce, ktera reprezentuje vstup a odchod z centra
			
			} else if (pid_child < 0)
			{	// pokud dojde k chybe, zabiju procesy, ktere sem uz vytvoril
				for (int j = 0; j < i; j++)
				{	// zabiti
					kill(children_pids[j], SIGKILL);
				}
				// uvolneni pameti
				clean_memory(7);	
				perror("fork");		// tisk zpravy
			} else if (pid_child > 0)		// puvodni procesy, na ktere pak musim pockat
			{
				children_pids[i] = pid_child;	// ulozeni jejich pid do pole
			}
			
		}

	} else if (pid > 0)		// generovani 'adult' procesu
	{					
		pid = fork();		// generovani detskeho procesu, ktery bude generovat 'adult' procesy

		if (pid == 0)		// detsky proces, ktery generuje dalsi procesy
		{	
			for (int k = 0; k < adults_count; k++)
			{	// cekaci doba pro vytvoreni noveho 'adult' procesu (* 1000 protoze chceme ms)
				if (adult_generate_time > 0)
				{
					generating_time_adults = random() % adult_generate_time * 1000;
					usleep(generating_time_adults);
				}
				pid_adult = fork();		// vytvoreni noveho procesu
				if (pid_adult < 0)
				{	// pokud doslo k chybe, zabiju procesy ktere sem vytvoril
					for (int j = 0; j < k; j++)
					{	// zabiti
						kill(adults_pids[j], SIGKILL);
					}
					clean_memory(7);	// uvolneni pameti
					perror("fork");		// tisk chybove zpravy
					exit(2);			// konec
				} else if (pid_adult == 0)		// pokud se jedna o 'adult' proces
				{	
					sem_wait(sem_print);	// uzavreni semaforu pro tisk
					(*event_counter_ptr)++;		// inkrementace poctu akci
					fprintf(f, "%d\t: A %d\t: started\n", *event_counter_ptr, k+1);		// tisk zpravy
					sem_post(sem_print);		// otevreni semaforu pro tisk

					adult(k+1, f);		// volani funkce, ktera reprezentuje vstup a odchod z centra
					
				} else if (pid_adult > 0)	// puvodni proces na ktery musim pockat
				{	// ulozim jeho pid do pole
					adults_pids[k] = pid_adult;
				}
		
			}
		} else if (pid < 0)	// pokud dojde k chybe pri generovani prvniho detskeho procesu, koncim
		{
			perror("fork");		// chybove hlaseni
			clean_resources();	// uvolneni zdroju
			exit(2);			// konec
		}
	}

	// cekani na puvodni procesy
	for (int i = 0; i < children_count; i++)
	{	
		waitpid(children_pids[i], NULL, 0);
	}

	for (int i = 0; i < adults_count; i++)
	{	
		waitpid(adults_pids[i], NULL, 0);
	}
	// cekani na uplne prvni proces
	waitpid(pid, NULL, 0);
	// uvolneni zdroju 
	clean_resources();
	// konec programu
	return 0;
}	
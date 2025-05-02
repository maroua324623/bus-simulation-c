#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define NB_BUS_X 5
#define NB_BUS_Y 4
#define NB_TRAJETS 10

// Sémaphores et mutex
sem_t mutex;
sem_t tunnel_mutex;

// Barrière maison
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int total;
} my_barrier_t;

my_barrier_t barrier;

void barrier_init(my_barrier_t *barrier, int total) {
    pthread_mutex_init(&barrier->mutex, NULL);
    pthread_cond_init(&barrier->cond, NULL);
    barrier->count = 0;
    barrier->total = total;
}

void barrier_wait(my_barrier_t *barrier) {
    pthread_mutex_lock(&barrier->mutex);
    barrier->count++;
    if (barrier->count >= barrier->total) {
        barrier->count = 0;
        pthread_cond_broadcast(&barrier->cond);
    } else {
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }
    pthread_mutex_unlock(&barrier->mutex);
}

void barrier_destroy(my_barrier_t *barrier) {
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
}

// Variables globales
int nb_x = 0;
int nb_y = 0;
const char *sens = NULL;

typedef struct {
    int bus_id;
    char ville_depart;
} Bus;

void random_sleep() {
    usleep((rand() % 500 + 1000) * 1000);  // entre 1s et 1.5s
}


void *bus_thread(void *arg) {
    Bus *bus = (Bus *)arg;
    for (int i = 1; i <= NB_TRAJETS; i++) {
        // Trajet aller
        if (bus->ville_depart == 'X') {
            sem_wait(&mutex);
            while (sens && strcmp(sens, "Y->X") == 0) {
                sem_post(&mutex);
                printf("Bus %d de X : Attente pour entrer dans le tunnel (Trajet %d)\n", bus->bus_id, i);
                usleep(100000); // 0.1s
                sem_wait(&mutex);
            }
            if (nb_x == 0) {
                sem_wait(&tunnel_mutex);
                sens = "X->Y";
            }
            nb_x++;
            printf("Bus %d de X : X -> Y (Trajet %d)\n", bus->bus_id, i);
            sem_post(&mutex);

            random_sleep();

            sem_wait(&mutex);
            nb_x--;
            if (nb_x == 0) {
                sens = NULL;
                sem_post(&tunnel_mutex);
            }
            sem_post(&mutex);
        } else {
            sem_wait(&mutex);
            while (sens && strcmp(sens, "X->Y") == 0) {
                sem_post(&mutex);
                printf("Bus %d de Y : Attente pour entrer dans le tunnel (Trajet %d)\n", bus->bus_id, i);
                usleep(100000);
                sem_wait(&mutex);
            }
            if (nb_y == 0) {
                sem_wait(&tunnel_mutex);
                sens = "Y->X";
            }
            nb_y++;
            printf("Bus %d de Y : Y -> X (Trajet %d)\n", bus->bus_id, i);
            sem_post(&mutex);

            random_sleep();

            sem_wait(&mutex);
            nb_y--;
            if (nb_y == 0) {
                sens = NULL;
                sem_post(&tunnel_mutex);
            }
            sem_post(&mutex);
        }

        // Trajet retour
        bus->ville_depart = (bus->ville_depart == 'X') ? 'Y' : 'X';
        if (bus->ville_depart == 'X') {
            sem_wait(&mutex);
            while (sens && strcmp(sens, "Y->X") == 0) {
                sem_post(&mutex);
                printf("Bus %d de X : Attente pour entrer dans le tunnel (Trajet %d - Retour)\n", bus->bus_id, i);
                usleep(100000);
                sem_wait(&mutex);
            }
            if (nb_x == 0) {
                sem_wait(&tunnel_mutex);
                sens = "X->Y";
            }
            nb_x++;
            printf("Bus %d de X : Y -> X (Trajet %d - Retour)\n", bus->bus_id, i);
            sem_post(&mutex);

            random_sleep();
            sem_wait(&mutex);
            nb_x--;
            if (nb_x == 0) {
                sens = NULL;
                sem_post(&tunnel_mutex);
            }
            sem_post(&mutex);
        } else {
            sem_wait(&mutex);
            while (sens && strcmp(sens, "X->Y") == 0) {
                sem_post(&mutex);
                printf("Bus %d de Y : Attente pour entrer dans le tunnel (Trajet %d - Retour)\n", bus->bus_id, i);
                usleep(100000);
                sem_wait(&mutex);
            }
            if (nb_y == 0) {
                sem_wait(&tunnel_mutex);
                sens = "Y->X";
            }
            nb_y++;
            printf("Bus %d de Y : X -> Y (Trajet %d - Retour)\n", bus->bus_id, i);
            sem_post(&mutex);

            random_sleep();

            sem_wait(&mutex);
            nb_y--;
            if (nb_y == 0) {
                sens = NULL;
                sem_post(&tunnel_mutex);
            }
            sem_post(&mutex);
        }

        barrier_wait(&barrier);
    }
    pthread_exit(NULL);
}
int main() {
    pthread_t threads[NB_BUS_X + NB_BUS_Y];
    Bus buses[NB_BUS_X + NB_BUS_Y];

    srand((unsigned int)time(NULL));
    sem_init(&mutex, 0, 1);
    sem_init(&tunnel_mutex, 0, 1);
    barrier_init(&barrier, NB_BUS_X + NB_BUS_Y);

    for (int i = 0; i < NB_BUS_X; i++) {
        buses[i].bus_id = i + 1;
        buses[i].ville_depart = 'X';
        pthread_create(&threads[i], NULL, bus_thread, &buses[i]);
    }

    for (int i = 0; i < NB_BUS_Y; i++) {
        buses[NB_BUS_X + i].bus_id = NB_BUS_X + i + 1;
        buses[NB_BUS_X + i].ville_depart = 'Y';
        pthread_create(&threads[NB_BUS_X + i], NULL, bus_thread, &buses[NB_BUS_X + i]);
    }

    for (int i = 0; i < NB_BUS_X + NB_BUS_Y; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&mutex);
    sem_destroy(&tunnel_mutex);
    barrier_destroy(&barrier);

    printf("Tous les bus ont terminé leurs trajets.\n");
    return 0;
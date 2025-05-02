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

// Directions constants
#define DIR_X_TO_Y "X->Y"
#define DIR_Y_TO_X "Y->X"

// Sémaphores et mutex
sem_t mutex;
sem_t tunnel_mutex;

// Barrière maison
typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int total;
} my_barrier_t;

my_barrier_t barrier;

void barrier_init(my_barrier_t *barrier, int total)
{
    pthread_mutex_init(&barrier->mutex, NULL);
    pthread_cond_init(&barrier->cond, NULL);
    barrier->count = 0;
    barrier->total = total;
}

void barrier_wait(my_barrier_t *barrier)
{
    pthread_mutex_lock(&barrier->mutex);
    barrier->count++;
    if (barrier->count >= barrier->total)
    {
        barrier->count = 0;
        pthread_cond_broadcast(&barrier->cond);
    }
    else
    {
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }
    pthread_mutex_unlock(&barrier->mutex);
}

void barrier_destroy(my_barrier_t *barrier)
{
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
}

// Variables globales
int nb_x = 0;
int nb_y = 0;
const char *current_direction = NULL;

typedef struct
{
    int bus_id;
    char current_city;
} Bus;

void random_sleep()
{
    usleep((rand() % 500 + 1000) * 1000); // entre 1s et 1.5s
}

void enter_tunnel(Bus *bus, const char *direction, int trip_num, int is_return)
{
    sem_wait(&mutex);

    // Attendre si la direction n'est pas bonne
    while (current_direction && strcmp(current_direction, direction) != 0)
    {
        sem_post(&mutex);
        printf("Bus %d de %c : Attente pour %s (Trajet %d%s)\n",
               bus->bus_id, bus->current_city, direction, trip_num,
               is_return ? " - Retour" : "");
        usleep(100000); // 0.1s
        sem_wait(&mutex);
    }

    // Premier bus dans cette direction
    if ((direction == DIR_X_TO_Y && nb_x == 0) || (direction == DIR_Y_TO_X && nb_y == 0))
    {
        sem_wait(&tunnel_mutex);
        current_direction = direction;
    }

    // Mettre à jour le compteur
    if (direction == DIR_X_TO_Y)
    {
        nb_x++;
    }
    else
    {
        nb_y++;
    }

    printf("Bus %d de %c : %s (Trajet %d%s)\n",
           bus->bus_id, bus->current_city, direction, trip_num,
           is_return ? " - Retour" : "");

    sem_post(&mutex);
}

void exit_tunnel(Bus *bus, const char *direction)
{
    sem_wait(&mutex);

    // Mettre à jour le compteur
    if (direction == DIR_X_TO_Y)
    {
        nb_x--;
    }
    else
    {
        nb_y--;
    }

    // Dernier bus dans cette direction
    if ((direction == DIR_X_TO_Y && nb_x == 0) || (direction == DIR_Y_TO_X && nb_y == 0))
    {
        current_direction = NULL;
        sem_post(&tunnel_mutex);
    }

    sem_post(&mutex);
}

void *bus_thread(void *arg)
{
    Bus *bus = (Bus *)arg;

    for (int i = 1; i <= NB_TRAJETS; i++)
    {
        // Trajet aller
        const char *out_direction = (bus->current_city == 'X') ? DIR_X_TO_Y : DIR_Y_TO_X;
        enter_tunnel(bus, out_direction, i, 0);
        random_sleep();
        exit_tunnel(bus, out_direction);

        // Changer de ville après le trajet aller
        bus->current_city = (bus->current_city == 'X') ? 'Y' : 'X';

        // Trajet retour
        const char *return_direction = (bus->current_city == 'X') ? DIR_Y_TO_X : DIR_X_TO_Y;
        enter_tunnel(bus, return_direction, i, 1);
        random_sleep();
        exit_tunnel(bus, return_direction);

        // Changer de ville après le trajet retour
        bus->current_city = (bus->current_city == 'X') ? 'Y' : 'X';

        barrier_wait(&barrier);
    }

    pthread_exit(NULL);
}

int main()
{
    pthread_t threads[NB_BUS_X + NB_BUS_Y];
    Bus buses[NB_BUS_X + NB_BUS_Y];

    srand((unsigned int)time(NULL));
    sem_init(&mutex, 0, 1);
    sem_init(&tunnel_mutex, 0, 1);
    barrier_init(&barrier, NB_BUS_X + NB_BUS_Y);

    // Créer les bus de la ville X
    for (int i = 0; i < NB_BUS_X; i++)
    {
        buses[i].bus_id = i + 1;
        buses[i].current_city = 'X';
        if (pthread_create(&threads[i], NULL, bus_thread, &buses[i]) != 0)
        {
            perror("Erreur création thread");
            exit(EXIT_FAILURE);
        }
    }

    // Créer les bus de la ville Y
    for (int i = 0; i < NB_BUS_Y; i++)
    {
        buses[NB_BUS_X + i].bus_id = NB_BUS_X + i + 1;
        buses[NB_BUS_X + i].current_city = 'Y';
        if (pthread_create(&threads[NB_BUS_X + i], NULL, bus_thread, &buses[NB_BUS_X + i]) != 0)
        {
            perror("Erreur création thread");
            exit(EXIT_FAILURE);
        }
    }

    // Attendre la fin des threads
    for (int i = 0; i < NB_BUS_X + NB_BUS_Y; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            perror("Erreur join thread");
            exit(EXIT_FAILURE);
        }
    }

    sem_destroy(&mutex);
    sem_destroy(&tunnel_mutex);
    barrier_destroy(&barrier);

    printf("Tous les bus ont terminé leurs trajets.\n");
    return 0;
}
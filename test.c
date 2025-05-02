#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// --- Déclarations globales ---

// Sémaphores et mutex pour synchronisation
sem_t tunnel_sem;        // Sémaphore pour contrôler l'accès unique au tunnel
sem_t x_to_y_sem;        // Sémaphore pour file d'attente des bus allant de X vers Y
sem_t y_to_x_sem;        // Sémaphore pour file d'attente des bus allant de Y vers X
pthread_mutex_t counter_mutex;  // Mutex pour protéger les compteurs et la direction

// Compteurs de bus en attente selon la direction
int x_to_y_count = 0;
int y_to_x_count = 0;

// Direction actuelle du tunnel : "X->Y", "Y->X", ou NULL si libre
char* direction = NULL;

// Structure représentant un bus
typedef struct {
    int id;         // Identifiant du bus
    char* city;     // Ville d'origine ("X" ou "Y")
} Bus;

// --- Fonction de simulation d'un trajet (pause aléatoire) ---
void simulate_travel() {
    int sleep_time = 1000000 + (rand() % 500000); // Entre 1 et 1.5 secondes
    usleep(sleep_time);
}

// --- Comportement du bus ---
void* bus_behavior(void* arg) {
    Bus* bus = (Bus*)arg;

    // Définir les points de départ/arrivée selon la ville d'origine
    char* depart_aller = bus->city;
    char* arrivee_aller = (bus->city[0] == 'X') ? "Y" : "X";
    char* depart_retour = arrivee_aller;
    char* arrivee_retour = depart_aller;

    for (int i = 1; i <= 10; i++) { // Chaque bus fait 10 allers-retours

        // --- Étape 1 : Préparer le trajet aller ---
        pthread_mutex_lock(&counter_mutex);
        if (bus->city[0] == 'X') {
            x_to_y_count++;
            if (direction == NULL || direction[0] == 'X') {
                direction = "X->Y";
                sem_post(&x_to_y_sem); // Autorise un bus à avancer
            }
        } else {
            y_to_x_count++;
            if (direction == NULL || direction[0] == 'Y') {
                direction = "Y->X";
                sem_post(&y_to_x_sem); // Autorise un bus à avancer
            }
        }
        pthread_mutex_unlock(&counter_mutex);

        // --- Étape 2 : Attendre l'autorisation pour entrer dans le tunnel ---
        if (bus->city[0] == 'X') {
            sem_wait(&x_to_y_sem);
        } else {
            sem_wait(&y_to_x_sem);
        }

        // --- Étape 3 : Traverser le tunnel ---
        sem_wait(&tunnel_sem);
        printf("Bus %d de %s : %s -> %s (Trajet %d)\n", bus->id, bus->city, depart_aller, arrivee_aller, i);
        simulate_travel();
        sem_post(&tunnel_sem);

        // --- Étape 4 : Sortie du tunnel - gestion de l'équité ---
        pthread_mutex_lock(&counter_mutex);
        if (bus->city[0] == 'X') {
            x_to_y_count--;
            if (x_to_y_count == 0 && y_to_x_count > 0) {
                direction = "Y->X";            // Changer de direction
                sem_post(&y_to_x_sem);        // Libérer un bus Y->X
            } else if (x_to_y_count > 0) {
                sem_post(&x_to_y_sem);        // Continuer dans le même sens
            }
        } else {
            y_to_x_count--;
            if (y_to_x_count == 0 && x_to_y_count > 0) {
                direction = "X->Y";
                sem_post(&x_to_y_sem);
            } else if (y_to_x_count > 0) {
                sem_post(&y_to_x_sem);
            }
        }
        pthread_mutex_unlock(&counter_mutex);

        // --- Étape 5 : Préparer le trajet retour ---
        pthread_mutex_lock(&counter_mutex);
        if (bus->city[0] == 'X') {
            y_to_x_count++;
            if (direction == NULL || direction[0] == 'Y') {
                direction = "Y->X";
                sem_post(&y_to_x_sem);
            }
        } else {
            x_to_y_count++;
            if (direction == NULL || direction[0] == 'X') {
                direction = "X->Y";
                sem_post(&x_to_y_sem);
            }
        }
        pthread_mutex_unlock(&counter_mutex);

        // --- Étape 6 : Attente pour le retour ---
        if (bus->city[0] == 'X') {
            sem_wait(&y_to_x_sem);
        } else {
            sem_wait(&x_to_y_sem);
        }

        // --- Étape 7 : Traversée retour ---
        sem_wait(&tunnel_sem);
        printf("Bus %d de %s : %s -> %s (Trajet %d)\n", bus->id, bus->city, depart_retour, arrivee_retour, i);
        simulate_travel();
        sem_post(&tunnel_sem);

        // --- Étape 8 : Sortie du tunnel - équité retour ---
        pthread_mutex_lock(&counter_mutex);
        if (bus->city[0] == 'X') {
            y_to_x_count--;
            if (y_to_x_count == 0 && x_to_y_count > 0) {
                direction = "X->Y";
                sem_post(&x_to_y_sem);
            } else if (y_to_x_count > 0) {
                sem_post(&y_to_x_sem);
            }
        } else {
            x_to_y_count--;
            if (x_to_y_count == 0 && y_to_x_count > 0) {
                direction = "Y->X";
                sem_post(&y_to_x_sem);
            } else if (x_to_y_count > 0) {
                sem_post(&x_to_y_sem);
            }
        }
        pthread_mutex_unlock(&counter_mutex);
    }

    return NULL;
}

// --- Programme principal ---
int main() {
    srand(time(NULL)); // Initialisation de la génération aléatoire

    // Initialisation des sémaphores et mutex
    sem_init(&tunnel_sem, 0, 1);     // 1 bus à la fois dans le tunnel
    sem_init(&x_to_y_sem, 0, 0);     // File d'attente vide au départ
    sem_init(&y_to_x_sem, 0, 0);
    pthread_mutex_init(&counter_mutex, NULL);

    // Création des threads bus
    const int X_BUSES = 5;
    const int Y_BUSES = 4;
    pthread_t buses[X_BUSES + Y_BUSES];
    Bus bus_data[X_BUSES + Y_BUSES];

    // Création des bus partant de X
    for (int i = 0; i < X_BUSES; i++) {
        bus_data[i].id = i + 1;
        bus_data[i].city = "X";
        pthread_create(&buses[i], NULL, bus_behavior, &bus_data[i]);
    }

    // Création des bus partant de Y
    for (int i = 0; i < Y_BUSES; i++) {
        bus_data[i + X_BUSES].id = i + 1;
        bus_data[i + X_BUSES].city = "Y";
        pthread_create(&buses[i + X_BUSES], NULL, bus_behavior, &bus_data[i + X_BUSES]);
    }

    // Attente de la fin des threads
    for (int i = 0; i < X_BUSES + Y_BUSES; i++) {
        pthread_join(buses[i], NULL);
    }

    // Libération des ressources
    sem_destroy(&tunnel_sem);
    sem_destroy(&x_to_y_sem);
    sem_destroy(&y_to_x_sem);
    pthread_mutex_destroy(&counter_mutex);

    return 0;
}

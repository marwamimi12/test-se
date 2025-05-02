#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

// Constantes
#define NB_BUS_X 5
#define NB_BUS_Y 4
#define NB_TRAJETS 10
#define MIN_SLEEP_MS 1000
#define MAX_SLEEP_MS 1500

// Directions
#define DIR_X_TO_Y 0
#define DIR_Y_TO_X 1

// Structure pour les donnees du bus
typedef struct {
    int id;              // Identifiant du bus
    char ville_origine;  // 'X' ou 'Y'
} Bus;

// Variables globales
sem_t tunnel_sem;        // Controle l'acces au tunnel (1 bus a la fois)
sem_t x_to_y_sem;        // File d'attente des bus allant de X vers Y
sem_t y_to_x_sem;        // File d'attente des bus allant de Y vers X
pthread_mutex_t mutex;   // Protection des variables partagees

// Compteurs de bus en attente pour chaque direction
int x_to_y_count = 0;
int y_to_x_count = 0;

// Direction actuelle du tunnel (NULL si libre)
char* direction = NULL;

// Fonction pour generer un temps de sommeil aleatoire (trajet)
void simuler_trajet() {
    int sleep_time = (rand() % (MAX_SLEEP_MS - MIN_SLEEP_MS + 1)) + MIN_SLEEP_MS;
    usleep(sleep_time * 1000);  // Conversion en microsecondes
}

// Fonction executee par chaque thread de bus
void* bus_thread(void* arg) {
    Bus* bus = (Bus*)arg;
    char depart_aller, arrivee_aller, depart_retour, arrivee_retour;
    
    // Determiner les points de depart/arrivee selon la ville d'origine
    if (bus->ville_origine == 'X') {
        depart_aller = 'X';
        arrivee_aller = 'Y';
        depart_retour = 'Y';
        arrivee_retour = 'X';
    } else {
        depart_aller = 'Y';
        arrivee_aller = 'X';
        depart_retour = 'X';
        arrivee_retour = 'Y';
    }
    
    for (int i = 1; i <= NB_TRAJETS; i++) {
        // --- TRAJET ALLER ---
        
        // Preparation du trajet aller
        pthread_mutex_lock(&mutex);
        if (bus->ville_origine == 'X') {
            // Bus partant de X, direction X->Y
            x_to_y_count++;
            if (direction == NULL || direction[0] == 'X') {
                direction = "X->Y";
                sem_post(&x_to_y_sem); // Autoriser un bus a avancer
            }
        } else {
            // Bus partant de Y, direction Y->X
            y_to_x_count++;
            if (direction == NULL || direction[0] == 'Y') {
                direction = "Y->X";
                sem_post(&y_to_x_sem); // Autoriser un bus a avancer
            }
        }
        pthread_mutex_unlock(&mutex);
        
        // Attente de l'autorisation pour entrer dans le tunnel
        if (bus->ville_origine == 'X') {
            sem_wait(&x_to_y_sem);
        } else {
            sem_wait(&y_to_x_sem);
        }
        
        // Traversee du tunnel
        sem_wait(&tunnel_sem);
        printf("Bus %d de %c : %c -> %c (Trajet %d)\n", 
               bus->id, bus->ville_origine, depart_aller, arrivee_aller, i);
        simuler_trajet();
        sem_post(&tunnel_sem);
        
        // Sortie du tunnel - Gestion de l'equite
        pthread_mutex_lock(&mutex);
        if (bus->ville_origine == 'X') {
            x_to_y_count--;
            if (x_to_y_count == 0 && y_to_x_count > 0) {
                direction = "Y->X";          // Changer de direction
                sem_post(&y_to_x_sem);       // Liberer un bus Y->X
            } else if (x_to_y_count > 0) {
                sem_post(&x_to_y_sem);       // Continuer dans le meme sens
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
        pthread_mutex_unlock(&mutex);
        
        // --- TRAJET RETOUR ---
        
        // Preparation du trajet retour
        pthread_mutex_lock(&mutex);
        if (bus->ville_origine == 'X') {
            // Bus d'origine X, maintenant a Y, direction Y->X
            y_to_x_count++;
            if (direction == NULL || direction[0] == 'Y') {
                direction = "Y->X";
                sem_post(&y_to_x_sem);
            }
        } else {
            // Bus d'origine Y, maintenant a X, direction X->Y
            x_to_y_count++;
            if (direction == NULL || direction[0] == 'X') {
                direction = "X->Y";
                sem_post(&x_to_y_sem);
            }
        }
        pthread_mutex_unlock(&mutex);
        
        // Attente pour le retour
        if (bus->ville_origine == 'X') {
            sem_wait(&y_to_x_sem);
        } else {
            sem_wait(&x_to_y_sem);
        }
        
        // Traversee retour
        sem_wait(&tunnel_sem);
        printf("Bus %d de %c : %c -> %c (Trajet %d)\n", 
               bus->id, bus->ville_origine, depart_retour, arrivee_retour, i);
        simuler_trajet();
        sem_post(&tunnel_sem);
        
        // Sortie du tunnel - equite retour
        pthread_mutex_lock(&mutex);
        if (bus->ville_origine == 'X') {
            // Bus d'origine X, rentrant a X
            y_to_x_count--;
            if (y_to_x_count == 0 && x_to_y_count > 0) {
                direction = "X->Y";
                sem_post(&x_to_y_sem);
            } else if (y_to_x_count > 0) {
                sem_post(&y_to_x_sem);
            }
        } else {
            // Bus d'origine Y, rentrant a Y
            x_to_y_count--;
            if (x_to_y_count == 0 && y_to_x_count > 0) {
                direction = "Y->X";
                sem_post(&y_to_x_sem);
            } else if (x_to_y_count > 0) {
                sem_post(&x_to_y_sem);
            }
        }
        pthread_mutex_unlock(&mutex);
        
        // Petite pause entre les trajets pour favoriser l'entrelacement
        usleep(50000);  // 50ms
    }
    
    free(bus);
    return NULL;
}

int main() {
    pthread_t threads[NB_BUS_X + NB_BUS_Y];
    int thread_count = 0;

    printf("Debut des trajets : \n");    
    // Initialisation du generateur de nombres aleatoires
    srand(time(NULL));
    
    // Initialisation des semaphores et mutex
    sem_init(&tunnel_sem, 0, 1);     // 1 bus à la fois dans le tunnel
    sem_init(&x_to_y_sem, 0, 0);     // Files d'attente vides au départ
    sem_init(&y_to_x_sem, 0, 0);
    pthread_mutex_init(&mutex, NULL);
    
    // Creation des threads pour les bus de la ville X
    for (int i = 1; i <= NB_BUS_X; i++) {
        Bus* bus = malloc(sizeof(Bus));
        bus->id = i;
        bus->ville_origine = 'X';
        pthread_create(&threads[thread_count++], NULL, bus_thread, (void*)bus);
        usleep(10000);  // 10ms de delai pour favoriser l'entrelacement
    }
    
    // Creation des threads pour les bus de la ville Y
    for (int i = 1; i <= NB_BUS_Y; i++) {
        Bus* bus = malloc(sizeof(Bus));
        bus->id = i;
        bus->ville_origine = 'Y';
        pthread_create(&threads[thread_count++], NULL, bus_thread, (void*)bus);
        usleep(10000);  // 10ms de delai pour favoriser l'entrelacement
    }
    
    // Attendre la fin de tous les threads
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Destruction des semaphores et mutex
    sem_destroy(&tunnel_sem);
    sem_destroy(&x_to_y_sem);
    sem_destroy(&y_to_x_sem);
    pthread_mutex_destroy(&mutex);
    
    printf("Tous les trajets ont ete effectues.\n");
    
    return 0;
}

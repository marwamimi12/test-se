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
/*Ce programme simule le comportement de bus effectuant des allers-retours entre deux villes (X et Y) en empruntant un tunnel 
à sens unique partagé. Chaque bus est représenté par un thread, et le tunnel est protégé par un sémaphore pour garantir qu’un 
seul bus y circule à la fois. Les bus doivent attendre leur tour selon la direction en cours (X→Y ou Y→X), ce qui est géré par 
des sémaphores de file d’attente et un mutex qui protège les compteurs de bus en attente. 
Lorsqu’il n’y a plus de bus dans la direction actuelle et que des bus attendent dans l’autre sens, la direction du tunnel est 
changée pour permettre l’équité. Chaque bus effectue 10 allers-retours entre les villes, simulant un trajet à chaque passage 
dans le tunnel avec une pause aléatoire pour imiter un temps de traversée. L’objectif du programme est de gérer la concurrence 
d’accès au tunnel de manière sûre, équitable et efficace en utilisant les mécanismes de synchronisation POSIX (pthread, sem_t).
1. Initialisation
Le programme commence par initialiser :

Trois sémaphores :

tunnel_sem : permet à un seul bus d’entrer dans le tunnel à la fois.

x_to_y_sem et y_to_x_sem : files d’attente pour les bus allant de X vers Y, ou de Y vers X.

Un mutex counter_mutex : protège les variables partagées x_to_y_count, y_to_x_count et direction.

🚌 2. Création des bus (threads)
On crée des bus venant de la ville X (5) et de la ville Y (4), chacun dans un thread.

Chaque bus répète 10 trajets (aller-retour entre X et Y).

🚦 3. Avant chaque trajet
Le bus s’annonce en ajoutant +1 à la file d’attente correspondant à sa direction (protégé par le mutex).

Si aucun autre bus n’est en cours, il définit la direction du tunnel (direction = "X->Y" ou "Y->X").

Il "réveille" le premier bus de sa direction avec sem_post().

🚧 4. Attente et entrée dans le tunnel
Le bus attend son tour via sem_wait() sur la file d’attente de sa direction.

Ensuite, il prend le tunnel (sem_wait(&tunnel_sem)), ce qui empêche les autres d’y entrer.

Il imprime le trajet effectué et simule le passage avec un usleep() aléatoire.

🛑 5. Sortie du tunnel
Après le trajet, le bus libère le tunnel (sem_post(&tunnel_sem)).

Il diminue le compteur de sa direction.

Si sa direction est vide mais qu’il y a des bus en attente dans l’autre sens, il change la direction et débloque un bus opposé.

🔁 6. Retour (trajectoire inverse)
Il répète exactement le même processus pour le trajet retour (Y → X ou X → Y).

Il continue ainsi jusqu’à avoir effectué 10 allers-retours.

✅ 7. Fin du programme
Le main() attend la fin de tous les threads avec pthread_join().

Puis il détruit les sémaphores et le mutex pour libérer les ressources.
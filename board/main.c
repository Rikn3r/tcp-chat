#include <stdio.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>

#include "../utils/client-structures.h"
#include "../utils/utils.h"
#include "./threads.h"

int get_msgid() {
    int buffer_size = 100;
    char msgid_str[buffer_size];
    char *envvar = "MSGID";
    // On s'assure que envvar existe
    if(!getenv(envvar)){
        fprintf(stderr, "La variable %s n'a pas été trouvée.\n", envvar);
        exit(1);
    }

    // On s'assure que le buffer est assez grand pour accueillir la valeur de la variable d'environnement.
    if(snprintf(msgid_str, buffer_size, "%s", getenv(envvar)) >= buffer_size){
        fprintf(stderr, "La taille du buffer est trop petite : %d\n", buffer_size);
        exit(1);
    }
    //On s'assure que la variable d'environnement est bien un entier
    if (!isNumber(msgid_str)) {
        fprintf(stderr, "La valeur de %s n'est pas un entier \n", msgid_str);
        exit(1);
    }
    return atoi(msgid_str);
}

int main(int argc, char const *argv[])
{
    int msgid = get_msgid();
    pthread_t thread1, thread2, thread3;
    int status;
    
    // Creation des thread pour recevoir les messsages
    // Thread 1: gestionnaire des signaux
    status = pthread_create(&thread1, NULL, collectSignals, (void*) (size_t) msgid);
    if (status != 0) {
        perror("Erreur pendant la création du thread 1");
    }
    // Thread 2: affichage des messages
    status = pthread_create(&thread2, NULL, collectMessages, (void*) (size_t) msgid);
    if (status != 0) {
        perror("Erreur pendant la création du thread 2");
    }
    // Thread 3: Gestion de l'activité des membres
    status = pthread_create(&thread3, NULL, collectMoves, (void*) (size_t) msgid);
    if (status != 0) {
        perror("Erreur pendant la création du thread 3");
    }
    
    
    pthread_join(thread1, NULL);
    
    
    pthread_kill(thread2, SIGKILL);
    pthread_kill(thread3, SIGKILL);

    return EXIT_SUCCESS;
}

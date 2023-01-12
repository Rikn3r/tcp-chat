#include <signal.h>
#include <errno.h>
#include "../utils/utils.h"
#include "../utils/request.h"
#include "../utils/signals.h"
#include "../utils/client-structures.h"
#include "../utils/colors.h"
#include "user_management.h"
#include "request_management.h"

extern int errno;
static int IS_RUNNING = true;
int sock_s;
struct user *shared_memory; //Variable de mémoire partagée

static void handler(int sig, siginfo_t *info, void *ctx) {
    printf("Received signal %s (%d)\n", get_signal_name(sig), sig);
    IS_RUNNING = false;
    sigaction(sig, &noaction, NULL);
    shutdown(sock_s, SHUT_RD);
    close(sock_s);
    /* Attend la fin des boucles ou de ce qui est en cours d'exécution. */
    sleep(1);
    /* Désallocatrion de la mémoire partagée */
    munmap(shared_memory, MAX_USERS_CONNECTED*sizeof(char*));
    kill(0, sig);
}

static void handle_signals(int signals[], int count) {
    struct sigaction action;
    memset(&action, '\0', sizeof(action));
    action.sa_sigaction = &handler;

    for (int i=0; i<count; i++) {
        sigaction(signals[i], &action, NULL);
    }
}

void broadcastMessage(char message[REQUEST_DATA_MAX_LENGTH], struct user *shared_memory, int sender_memory_index){

    tcpData broadcast_message;
    broadcast_message.type = 1;
    strcpy(broadcast_message.username, shared_memory[sender_memory_index].username);
    strcpy(broadcast_message.message, message);

    /* Envoi d'un message à tous les utilisateurs connectés */
    for (size_t i = 0; i < MAX_USERS_CONNECTED; i++)
    {
        if (shared_memory[i].sock != 0) {
            send(shared_memory[i].sock, &broadcast_message, sizeof(broadcast_message),0);
        }
    }
}

void* message_receiver(void* args) {
    struct tcp_informations *arguments = args; // Informations provenant du fil de communication
    int sock_c = (*arguments).sock_c; // Socket client
    char message[REQUEST_DATA_MAX_LENGTH]; // Message reçu
    int message_length; // Message length 
    int memory_index = -1; // Index de l'utilisateur dans la mémoire partagée

    /* Attendre de recevoir un message */
    while (IS_RUNNING) {
        message_length = recv(sock_c, message, REQUEST_DATA_MAX_LENGTH, MSG_DONTWAIT);
        if (message_length < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { // pas de message, attendez et réessayez
            sleep(1);
            errno = 0;
            continue;
        }
        if (message_length < 1)
            break;
        message[message_length] = '\0';

        if (memory_index == -1) { // Utilisateur non connecté
            // Vérifier si le jeton correspond à un utilisateur
            for (size_t i = 0; i < MAX_USERS_CONNECTED; i++)
            {
                if(strcmp(message,(*arguments).shared_memory[i].token) == 0) {
                    (*arguments).shared_memory[i].sock = sock_c;
                    memory_index = i;
                }
            }
            if (memory_index == -1) { // Aucune correspondance trouvée
                tcpData msg = {7, "", "Vous devez vous connecter pour envoyer des messages !"};
                send(sock_c, &msg, sizeof(msg), 0);
            } else {
                tcpData msg = {5, "", "Vous avez été connecté avec succès !"};
                send(sock_c, &msg, sizeof(msg), 0);
            }
        } else { //User connected
            if (strcmp(message, "/deconnexion") == 0) { // deconnexion
                /* Envoyer /deconnexion pour déconnecter le client*/
                tcpData msg = {6, "", "/deconnexion"};
                send(sock_c, &msg, sizeof(msg), 0);
                break;
            }
            // Message normal
            printf("Message received (%ld): %s\n", strlen(message), message);
            broadcastMessage(message, (*arguments).shared_memory, memory_index);
        }
    }
    printf("[client_thread] - Fermermeture de la connection TCP %d\n", memory_index);
    /* Ferme la connexion et quitte */
    close(sock_c);
    if (memory_index != -1 && (*arguments).shared_memory[memory_index].sock == sock_c) { // Vérifiez si utilisateur toujours connecté et vide la mémoire partagée.
        (*arguments).shared_memory[memory_index].sock = 0;
        strcpy((*arguments).shared_memory[memory_index].token,"");
        strcpy((*arguments).shared_memory[memory_index].username,"");
    }
    pthread_exit(NULL);
}

void *communication(void* args){
    struct user *shared_memory = args;
    struct sockaddr_in adr_s; //Adresse du serveur
    struct tcp_informations thread_infos;
    sock_s = socket( AF_INET , SOCK_STREAM, 0 );//Socket du Serveur
    int sock_c = 0; //Socket du Client
    pthread_t client_thread; //Fil de discussion pour l'attente des messages du client

    /* Addresses serveurs */
    bzero(&adr_s,sizeof(adr_s));
    adr_s.sin_family = AF_INET;
    adr_s.sin_port = htons(TCP_PORT);
    adr_s.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* Lancement du serveur */
    if (bind(sock_s, (struct sockaddr *)&adr_s, sizeof(adr_s)) == -1){
        perror(C_RED "[Communication] - Impossible de se connecter au serveur" C_RESET) ;
        exit(EXIT_FAILURE);
    }

    if (listen(sock_s ,REQUEST_DATA_MAX_LENGTH) == -1){
        perror(C_RED "[Communication] - L'écoute a échoué" C_RESET);
        exit(EXIT_FAILURE);
    }

    while (IS_RUNNING) {
        /* Attente d'une connexion*/
        if ((sock_c = accept(sock_s, (struct sockaddr*) NULL, NULL)) < 0 && IS_RUNNING)
            perror(C_RED "[Communication] - L'acceptation a échoué" C_RESET);
        if (!IS_RUNNING)
            break;
        
        /* Thread pour recevoir tous les messages de ce client */
        thread_infos.shared_memory = shared_memory;
        thread_infos.sock_c = sock_c;
        printf(C_GRN "[Communication] - Création d'un thread de réception des messages" C_RESET);
        if (pthread_create(&client_thread, NULL, message_receiver, &thread_infos)) // Creation du Thread
            perror(C_RED "[Client_thread] - Erreur lors de la création du thread de réception des messages" C_RESET);
        else
            printf(C_GRN "\n[Communication] - Le thread a été crée !\n" C_RESET);
    }

    /* Terminer correctement le fil de communication  */
    pthread_exit(NULL);
}

void *request_manager(void* args){
    struct user *shared_memory = args; //Utilisateurs connectés
    unsigned int sock, lg = sizeof(struct sockaddr_in); 
    struct sockaddr_in adr_s, adr_c; //Adresses du serveur et du client

    //Paramètre de fonction pour la création des threads
    void* thread_function = NULL;

    //Objet envoyé au thread 
    struct request_processing arguments;

    //Objet de la demande
    struct request request;

    //Thread pour le traitement des requêtes
    pthread_t request_thread;

    /* Initialisation du Socket  */
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bzero(&adr_s,sizeof(adr_s)); 
    adr_s.sin_family = AF_INET;
    adr_s.sin_port = htons(UDP_PORT);
    adr_s.sin_addr.s_addr = htonl(INADDR_ANY);
    bind (sock, (struct sockaddr *) &adr_s, sizeof(adr_s)); 

    while (IS_RUNNING) {
        //En attente d'un nouveau message
        if (recvfrom (sock, &request, sizeof(struct request), 0, (struct sockaddr *) &adr_c, &lg)){

            arguments.adr_client = adr_c;
            arguments.request = request;
            arguments.shared_memory = shared_memory;
            arguments.sock = sock;

            switch (request.type){
                case 1: //log in
                    printf(C_CYN "[Gestion des requêtes] - Création d'un thread de connexion...\n" C_RESET);
                    thread_function = login; // fonction pour le login
                    break;
                case -1://log out
                    printf(C_CYN "[Gestion des requêtes] - Création d'un thread de déconnexion...\n" C_RESET);
                    thread_function = logout; // fonction pour le log out
                    break;
                case 2://creation of account
                    printf(C_CYN "[Gestion des requêtes] - Création d'un thread de création de compte... \n" C_RESET);
                    thread_function = account_creation; // fonction pour la creation de compte
                    break;
                case -2://deletion of account
                    printf(C_CYN "[Gestion des requêtes] - Création d'un thread de suppréssion de compte...\n" C_RESET);
                    thread_function = account_deletion; // fonction pour la suppression de compte
                    break;
                default://connected users list
                    printf(C_CYN "[Gestion des requêtes] - Création d'un thread des utilisateurs connectés...\n" C_RESET);
                    thread_function = connected_users; // fonction pour envoyer la liste des users connectés
                    break;
            }
            /* Création d'un thread pour le traitement des requetes */
            if (pthread_create( &request_thread, NULL, thread_function, &arguments))
                printf(C_RED "\nErreur lors de la création du request_thread\n" C_RESET);
            else
                pthread_detach(request_thread);
        }
    }
    /* fermeture socket */
    close(sock);
    /* sortir du thread */
    pthread_exit(NULL);
}

int main(int argc, char const *argv[])
{
    pthread_t com, req; //processus de communication et processus de gestionnaire des requetes
    
    //Random
    srand(time(NULL));

    // ajout d'un gestionnaire de signaux 
    int signals[6] = {SIGSTOP, SIGABRT, SIGINT, SIGQUIT, SIGTERM, SIGTSTP};
    handle_signals(signals, sizeof(signals)/sizeof(signals[0]));

    /* Initialisation de la mémoire partagée des utilisateurs connectés */
    shared_memory = mmap(NULL, MAX_USERS_CONNECTED*sizeof(struct user), (PROT_READ | PROT_WRITE), (MAP_SHARED | MAP_ANONYMOUS), -1, 0); //Shared memory init
    //Initialisation du tableau des utilisateurs avec une chaîne vide comme nom d'utilisateur et adresse IP.
    for (size_t i = 0; i < MAX_USERS_CONNECTED; i++){
        strcpy(shared_memory[i].username,"");
        strcpy(shared_memory[i].token,"");
        shared_memory[i].sock = 0;
    }
    
    /* Création du thread de communication */
    printf("Creation d'un thread de communication... ");
    if (pthread_create(&com, NULL, communication, (void*)shared_memory)) {
        printf("\nErreur lors de la création du thread\n");
        exit(EXIT_FAILURE);
    } else
        printf(C_GRN"Crée\n" C_RESET);

    /* Création d'un thread de gestion des requête */
    printf("Création d'un thread de gestion des requête ... ");
    if (pthread_create(&req, NULL, request_manager, (void*)shared_memory)) {
        printf("\nErreur lors de la création du thread\n");
        exit(EXIT_FAILURE);
    } else
       printf(C_GRN"Crée\n" C_RESET);
    
    printf("Le serveur est démaré correctement\n");

    /* Join des trhead de communication et de gestion des requetes */
    pthread_join(com, NULL);
    pthread_join(req, NULL);

    /* Désallocation de la mémoire partagée*/
    munmap(shared_memory, MAX_USERS_CONNECTED*sizeof(char*));

    printf("Serveur terminé.\n");
    return 0;
}

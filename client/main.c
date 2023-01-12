#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>

#include <sys/msg.h>
#include <unistd.h>

#include "commands.h"
#include "client_methods.h"
#include "../utils/utils.h"
#include "../utils/request.h"
#include "../utils/signals.h"
#include "../utils/client-structures.h"
#include "../utils/colors.h"

#if __APPLE__
    #define OPEN_BOARD "open ./output/board --env MSGID=%i"
#else
    #define OPEN_BOARD "export MSGID=%i; gnome-terminal -- \"./output/board\""
#endif

int msgid;
int tcp_socket;

static void handler(int sig, siginfo_t *info, void *ctx) {
    printf(C_RED "Signal reçu %s (%d)" C_RESET "\n", get_signal_name(sig), sig);
    // Ferme la board et la file de message
    kill_board(msgid);
    msgctl(msgid, IPC_RMID, NULL);
    close(tcp_socket);
    exit(-1);
}

static void handle_signals(int signals[], int count) {
    struct sigaction action;
    memset(&action, '\0', sizeof(action));
    action.sa_sigaction = &handler;

    for (int i=0; i<count; i++) {
        sigaction(signals[i], &action, NULL);
    }
}

void *receive_msg()
{
    int len;
    // thread client toujours prêt à recevoir un message
    tcpData message;
    while ((len = recv(tcp_socket, &message, sizeof(message), 0)) > 0) {
        message.message[len] = '\0';
        /* Si la connexion est terminée */
        if(strcmp(message.message, COMMANDE_DECONNEXION) == 0){
            printf(C_GRN "Vous êtes maintenant déconnectés" C_RESET "\n");
            break;
        }

        /* Envoyer le message dans la file*/
        switch (message.type) {
            case 1: 
                sendMessage(msgid, message.username, message.message);
                break;
            case 4: // STOP
                kill_board(msgid);
                msgctl(msgid, IPC_RMID, NULL);
                break;
            case 5: // connecté
                printf(C_GRN "Vous êtes maintenant connectés" C_RESET "\n");
                sendSignal(msgid, 1);
                break;
            case 6: // déconnecté
                printf(C_GRN "Vous êtes maintenant déconnectés" C_RESET "\n");
                sendSignal(msgid, 2);
                break;
            case 7: // COnnexion obligatoire
                printf(C_RED "Il faut vous connecter avant de réaliser cette action" C_RESET "\n");
                break;
            default:
                printf(C_RED "Message inconnu de la part du serveur: [%ld] %s\n" C_RESET, message.type, message.message);
                break;
        }
    }
    return NULL;
}


void *TCP_connexion(void* args){
    char message[REQUEST_DATA_MAX_LENGTH]; 
    tcp_socket = socket( AF_INET, SOCK_STREAM,0); 
    struct sockaddr_in adr_s; 
    int exit_status = 0; 
    pthread_t receiver; 
    char token[TOKEN_SIZE]; 
    strcpy(token, "");

    /* Initialisation de l'adresse du serveur*/
    bzero(&adr_s,sizeof(adr_s));
    adr_s.sin_port = htons(TCP_PORT);
    adr_s.sin_family= AF_INET;
    adr_s.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* Etablissement de la connexion */
    if ((connect(tcp_socket ,(struct sockaddr *)&adr_s,sizeof(adr_s))) == -1 ) {
        perror("La connexion au socket à échouée");
        kill(0, SIGINT);
        pthread_exit(NULL);
    }

    // Création d'un thread pour recevoir les messages
    pthread_create(&receiver, NULL, receive_msg, NULL);
    AfficherAide(); 

    /* Envoie des messages */
    while (exit_status == 0) {
        saisieString(message, REQUEST_DATA_MAX_LENGTH);
        if (detectionCommande(message, &exit_status, &(*token), tcp_socket) == 0){ //S'il n'y a pas de commande
            write(tcp_socket, message, strlen(message));
        }
    }

    /* Fermeture du client*/
    close(tcp_socket);
    printf("[Connexion TCP] - La connexion s'est terminée !\n");
    pthread_exit(NULL);
}

static int create_msg_pipe() {
    int msgid = msgget(IPC_PRIVATE, IPC_CREAT|IPC_EXCL|0640);
    if (msgid == -1) {
        perror("Impossible de créer un tunnel de message :");
        msgctl(msgid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    return msgid;
}


int main(int argc, char const *argv[]) {
    pthread_t tcp_connect; 
    
    int signals[6] = {SIGSTOP, SIGABRT, SIGINT, SIGQUIT, SIGTERM, SIGTSTP};
    handle_signals(signals, sizeof(signals)/sizeof(signals[0]));

    msgid = create_msg_pipe();   

    // Lance la console board dans un autre terminal
    char command[200];
    sprintf(command, OPEN_BOARD, msgid);
    if (system(command) != 0) {
        fprintf(stderr, C_RED "Impossible d'ouvrir la console board dans un autre client\n" C_RESET);
        return EXIT_FAILURE;
    }
  
    /* Creation of TCP connexion manager */
    printf("Création d'un thread TCP... ");
    if (pthread_create( &tcp_connect, NULL, TCP_connexion, NULL)) {
        perror("\nErreur durant la création du thread");
        kill_board(msgid);
        msgctl(msgid, IPC_RMID, NULL);
    } else
        printf("Thread créé\n");

    /* Join TCP connexion manager thread */
    pthread_join(tcp_connect, NULL);

    kill_board(msgid);
    sleep(2);

    msgctl(msgid, IPC_RMID, NULL);

    return 0;
}

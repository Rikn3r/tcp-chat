#include "../utils/colors.h"

#include "commands.h"

void AfficherAide() {
    printf(ANSI_COLOR_BLUE);
    printf("%s -> Se connecter au serveur\n", COMMANDE_CONNEXION);
    printf("%s -> Se créer un compte\n", COMMANDE_INSCRIPTION);
    printf("%s -> Obtenir la liste des utilisateurs connectés\n", COMMANDE_LISTE);
    printf("%s -> Se déconnecter du serveur\n", COMMANDE_DECONNEXION);
    printf("%s -> Supprimer un compte\n", COMMANDE_SUPPRIMER);
    printf("%s -> Quitter le programme\n", COMMANDE_QUITTER);
    printf(ANSI_COLOR_RESET);
}

int is_command(char* message, char* command){
    if((strncmp(message,command,strlen(command)) == 0) 
        && (message[strlen(command)] == ' ' || message[strlen(command)] == '\0')){
        return 1;
    }
    return 0;
}

void connexion(char message[REQUEST_DATA_MAX_LENGTH],char* token, struct sockaddr_in adr_s, int udp_socket, int tcp_socket){
    struct request request;
    unsigned int lg = sizeof(adr_s);
    /* Construction de la requête */
    request.type = 1;
    strcpy(request.data,&message[strlen(COMMANDE_CONNEXION)+1]);

    /* requête de connexion (UDP) */
    sendto (udp_socket, (void *) &request, sizeof(struct request), 0, (struct sockaddr *) &adr_s, sizeof(adr_s)); 

    /* Réception du token */
    if (recvfrom (udp_socket, &request, sizeof(struct request), 0, (struct sockaddr *) &adr_s, &lg) > 0) {
        if (request.type == 0) { 
            strcpy(token, request.data);
            write(tcp_socket, request.data, strlen(request.data));
        } else { // Erreur -> pas le bon nom d'utilisateur/mdp
            printf(ANSI_COLOR_RED "%s\n" ANSI_COLOR_RESET, request.data);
        }
    }
}

void deconnexion(char* token, struct sockaddr_in adr_s, int udp_socket, int tcp_socket, int* exit_status){
    struct request request;
    unsigned int lg = sizeof(adr_s);
    /* Construction de la requête */
    request.type = -1;
    strcpy(request.data,token);

    /* Envoie de la requête */
    sendto (udp_socket, (void *) &request, sizeof(struct request), 0, (struct sockaddr *) &adr_s, sizeof(adr_s));

    /* Confirmation de réception*/
    if (recvfrom (udp_socket, &request, sizeof(struct request), 0, (struct sockaddr *) &adr_s, &lg) > 0) {
        if (request.type == 0) { //Envoi de la déconnexion au serveur TCP
            write(tcp_socket, COMMANDE_DECONNEXION, strlen(COMMANDE_DECONNEXION));
            *exit_status = 1;
        } else { 
            printf(ANSI_COLOR_RED "%s\n" ANSI_COLOR_RESET, request.data);
        }
    }
}

void creerCompte(char message[REQUEST_DATA_MAX_LENGTH], struct sockaddr_in adr_s, int udp_socket){
    struct request request;
    unsigned int lg = sizeof(adr_s);
    /* Construction de la requête */
    request.type = 2;
    strcpy(request.data,&message[strlen(COMMANDE_INSCRIPTION)+1]);

    /* requête de connexion (UDP) */
    sendto (udp_socket, (void *) &request, sizeof(struct request), 0, (struct sockaddr *) &adr_s, sizeof(adr_s)); 

    /* Reception du token */
    if (recvfrom (udp_socket, &request, sizeof(struct request), 0, (struct sockaddr *) &adr_s, &lg)>0){
        if (request.type == 0) {
            printf(ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\n",request.data);
        } else { 
            printf(ANSI_COLOR_RED "Syntaxe: /inscription NomUtilisateur Mdp\n");
            printf("%s" ANSI_COLOR_RESET "\n",request.data);
        }
    }
}

void supprimerCompte(char message[REQUEST_DATA_MAX_LENGTH], struct sockaddr_in adr_s, int udp_socket){
    struct request request;
    unsigned int lg = sizeof(adr_s);
    request.type = -2;
    strcpy(request.data,&message[strlen(COMMANDE_SUPPRIMER)+1]);

    sendto (udp_socket, (void *) &request, sizeof(struct request), 0, (struct sockaddr *) &adr_s, sizeof(adr_s)); 

    if (recvfrom (udp_socket, &request, sizeof(struct request), 0, (struct sockaddr *) &adr_s, &lg)>0){
        if(request.type == 0){
            printf(ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\n",request.data);
        }else{ 
            printf(ANSI_COLOR_RED "Syntaxe: /supprimer NomUtilisateur Mdp\n");
            printf("%s\n" ANSI_COLOR_RESET, request.data);
        }
    }
}

void utilisateursConnectes(struct sockaddr_in adr_s, int udp_socket){
    struct request request;
    unsigned int lg = sizeof(adr_s);
    int counter = 1;

    request.type = 0;
    strcpy(request.data,"");

    sendto (udp_socket, (void *) &request, sizeof(struct request), 0, (struct sockaddr *) &adr_s, sizeof(adr_s));

    if (recvfrom (udp_socket, &request, sizeof(struct request), 0, (struct sockaddr *) &adr_s, &lg) > 0) {
        if (request.type == 0 && strlen(request.data) > 0) { 

            printf(C_YEL "Utilisateur 1: ");
            for (size_t i = 0; i < strlen(request.data); i++)
            {
                if (request.data[i] == '\t') {
                    counter++;
                    printf("\nUtilisateur %d: ", counter);
                } else {
                    printf("%c", request.data[i]);
                }
            }
            printf(ANSI_COLOR_RESET "\n");
        } else if (request.type == 0) {
            printf("Personne n'est connecté\n");
        } else { 
            printf(ANSI_COLOR_RED "%s\n" ANSI_COLOR_RESET, request.data);
        }
    }

}

int detectionCommande(char message[
    
], int* exit_status, char* token, int tcp_sock){
    /* ---Connexion UDP --- */
    struct sockaddr_in adr_s, adr_c; 
    unsigned int sock; 

    if (strlen(message) > 0 && message[0] == '/') {
        /* Initialisation des adresses*/
        //Client
        bzero(&adr_c,sizeof(adr_c));
        adr_c.sin_family = AF_INET; 
        adr_c.sin_port = htons(UDP_PORT);
        adr_c.sin_addr.s_addr = htonl(INADDR_ANY);
        //Serveur
        bzero(&adr_s,sizeof(adr_s));
        adr_s.sin_family = AF_INET;
        adr_s.sin_port = htons(UDP_PORT);
        adr_s.sin_addr.s_addr = htonl(INADDR_ANY);

        /* Création du socket et liaison  */
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        bind(sock, (struct sockaddr *) &adr_c, sizeof(adr_c));

        if (is_command(message,COMMANDE_CONNEXION)) {
            if (strcmp(token,"") != 0) {
                printf(ANSI_COLOR_RED "You etes deja connectés" ANSI_COLOR_RESET "\n");
                return 1;
            }
            if (message[strlen(COMMANDE_CONNEXION)] == '\0') {
                printf(ANSI_COLOR_RED "Syntaxe: /connexion NomUtilisateur Mdp" ANSI_COLOR_RESET "\n");
            } else {
                //Necessite le TCP_socket pour envoyer le message
                connexion(message,&(*token), adr_s, sock, tcp_sock);
            }
        } else if (is_command(message,COMMANDE_DECONNEXION)) {
            //Necessite le TCP_socket pour envoyer le message
            deconnexion(token, adr_s, sock, tcp_sock, exit_status);
        } else if (is_command(message,COMMANDE_INSCRIPTION)) {
            creerCompte(message,adr_s,sock);
        } else if (is_command(message,COMMANDE_SUPPRIMER)) {
            if (strcmp(token, "") != 0) {
                printf(ANSI_COLOR_RED "Vous devez être déconnectés pour supprimer un compte" ANSI_COLOR_RESET "\n");
                return 1;
            }
            supprimerCompte(message,adr_s,sock);
        } else if (is_command(message,COMMANDE_LISTE)) {
            utilisateursConnectes(adr_s,sock);
        } else if (is_command(message,COMMANDE_QUITTER)) {
            *exit_status = 1;
        } else if (is_command(message,COMMANDE_AIDE)) {
            AfficherAide();
        } else {
            printf(ANSI_COLOR_RED "Commande non reconnue\n" ANSI_COLOR_RESET);
        }
        return 1;
    }
    return 0;
}
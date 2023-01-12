#pragma once

#include "../utils/utils.h"
#include "../utils/request.h"
#include <string.h>

/* Definition des constantes */
//Creation de compte
#define COMMANDE_INSCRIPTION "/inscription"
//Connexion
#define COMMANDE_CONNEXION "/connexion"
//Pour obtenir la liste des utilisateurs
#define COMMANDE_LISTE "/liste_utilisateurs"
//Deconnexion
#define COMMANDE_DECONNEXION "/deconnexion"
//Suppression de compte
#define COMMANDE_SUPPRIMER "/supprimer"
//Pour sortir du programme
#define COMMANDE_QUITTER "/quitter"
//Affiche l'aide
#define COMMANDE_AIDE "/aide"

void AfficherAide();
int is_command(char* message, char* command);
void connexion(char message[REQUEST_DATA_MAX_LENGTH],char* token, struct sockaddr_in adr_s, int udp_socket, int tcp_socket);
void deconnexion(char* token, struct sockaddr_in adr_s, int udp_socket, int tcp_socket, int* exit_status);
void creerCompte(char message[REQUEST_DATA_MAX_LENGTH], struct sockaddr_in adr_s, int udp_socket);
void supprimerCompte(char message[REQUEST_DATA_MAX_LENGTH], struct sockaddr_in adr_s, int udp_socket);
void utilisateursConnectes(struct sockaddr_in adr_s, int udp_socket);
int detectionCommande(char message[REQUEST_DATA_MAX_LENGTH], int* exit_status, char* token, int tcp_sock);
#include "request_management.h"

void *login(void* args){
    struct request_processing *parent_info = args;

    char token[TOKEN_SIZE];
    char username[MAX_USER_USERNAME_LENGTH], password[MAX_USER_PASSWORD_LENGTH];
    int separator_pos;

    char data[REQUEST_DATA_MAX_LENGTH];
    strcpy(data,(*parent_info).request.data);//Mettre les données de la demande dans les données

    printf("\t[Thread Connection] - Données reçues  (length : %ld): %s\n", strlen(data), data); //Log
    
    /* Parse les données en nom d'utilisateur et mot de passe */
    //Recherche de la position du séparateur
    for (separator_pos = 0; separator_pos < strlen(data) && data[separator_pos] != USER_PASSWORD_REQUEST_SEPARATOR; separator_pos++);
    
    // Vérifie la longueur des paramètres de la chaîne
    if (separator_pos >= MAX_USER_USERNAME_LENGTH || strlen(data)-separator_pos > MAX_USER_PASSWORD_LENGTH){
        (*parent_info).request.type = -1; //There is an error
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client));
        pthread_exit(NULL);
    } else if (separator_pos == 0 || separator_pos == strlen(data) || separator_pos == strlen(data)-1){
        (*parent_info).request.type = -1; //There is an error
        strcpy((*parent_info).request.data,"ERROR: Le login ou le mdp est vide");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client));
        pthread_exit(NULL);
    }

    /* Copie le nom d'utilisateur et le mot de passe à partir des données */
    strncpy(username,data,separator_pos);
    username[separator_pos]='\0';
    strncpy(password,&data[separator_pos]+1,strlen(data)-separator_pos);

    if(findNickname(username,password,ACCOUNT_FILE,1) != 1){
        (*parent_info).request.type = -1; 
        strcpy((*parent_info).request.data, "ERROR: Mauvais login/mot2passe");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
    }else{
        /* Adding user to the shared memory */
        switch (add_user((*parent_info).shared_memory,username,&(*token))){
        case 0://All went right
            (*parent_info).request.type = 0;
            strcpy((*parent_info).request.data,token);
            sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
            break;
        case 1://Utilisateur déjà connecté
            (*parent_info).request.type = -1; 
            strcpy((*parent_info).request.data,"ERROR: Utilisateur déjà connecté");
            sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
            break;
        default://Mémoire partagée pleine de l'utilisateur connecté
            (*parent_info).request.type = -1; 
            strcpy((*parent_info).request.data,"ERROR: Nombre maximal de connexions simultanées atteint");
            sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
            break;
        }
    }
    pthread_exit(NULL);
}

void *logout(void* args){
    /* Request informations */
    struct request_processing *parent_info = args;

    char token[TOKEN_SIZE];//Token got when log in

    /* Token size verification */
    if(strlen((*parent_info).request.data) != (TOKEN_SIZE-1)){ //Token doesn't have the right format
        (*parent_info).request.type = -1; 
        strcpy((*parent_info).request.data,"ERROR: Le token n'as pas le bon format");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
    }

    printf("[Thread Deconnection] - Données reçues (length : %ld): %s\n", strlen(token), token); //Log
    
    /* Sending response */
    strcpy(token,(*parent_info).request.data);//Setting token variable

    switch (remove_user((*parent_info).shared_memory,token)){//removing user by token
    case 0://Tout s'est bien passé
        (*parent_info).request.type = 0;
        strcpy((*parent_info).request.data,"Utilisateur déconnecté");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
        break;
    default://Utilisateur non trouvé
        (*parent_info).request.type = -1; 
        strcpy((*parent_info).request.data,"ERROR: Utilisateur n'existe pas ou n'a pas été trouvé");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
        break;
    }
    pthread_exit(NULL);
}

void *account_creation(void* args){
    /* Request informations */
    struct request_processing *parent_info = args;
    int separator_pos;//Index of the separator
    char username[MAX_USER_USERNAME_LENGTH], password[MAX_USER_PASSWORD_LENGTH];//Username and password got from request

    char data[REQUEST_DATA_MAX_LENGTH];
    strcpy(data,(*parent_info).request.data);//Put request data in data

    printf("[Thread Creation Compte] - Données reçues (length : %ld): %s\n", strlen(data), data); //Log
    
    /* parse data to username and password */
    //Get pos of separator
    for (separator_pos = 0; separator_pos < strlen(data) && data[separator_pos] != USER_PASSWORD_REQUEST_SEPARATOR; separator_pos++);

    //Check string param length
    if (separator_pos >= MAX_USER_USERNAME_LENGTH || strlen(data)-separator_pos > MAX_USER_PASSWORD_LENGTH){
        (*parent_info).request.type = -1; //There is an error
        strcpy((*parent_info).request.data,"ERROR: Le login ou le mdp est trop long");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client));
        pthread_exit(NULL);
    }else if (separator_pos == 0 || separator_pos == strlen(data) || separator_pos == strlen(data)-1){
        (*parent_info).request.type = -1; //There is an error
        strcpy((*parent_info).request.data,"ERROR: Le login ou le mdp est vide");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client));
        pthread_exit(NULL);
    }

    /* Copy username and password from data */
    strncpy(username,data,separator_pos);
    username[separator_pos]='\0';
    strncpy(password,&data[separator_pos]+1,strlen(data)-separator_pos);
    
    /* Sending response */
    if(creation(username,password,ACCOUNT_FILE) != 1){
        (*parent_info).request.type = -1; 
        strcpy((*parent_info).request.data,"ERROR: Ce nom est déjà utilisé");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
    }else{
        (*parent_info).request.type = 0; 
        strcpy((*parent_info).request.data,"Compte crée");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
    }

    pthread_exit(NULL);
}

void *account_deletion(void* args){
    /* Request informations */
    struct request_processing *parent_info = args;
    int separator_pos;//Index of the separator
    char username[MAX_USER_USERNAME_LENGTH], password[MAX_USER_PASSWORD_LENGTH];//Username and password got from request

    char data[REQUEST_DATA_MAX_LENGTH];
    strcpy(data,(*parent_info).request.data);//Put request data in data

    printf("[Thread suppresion Compte] - Données reçues (length : %ld): %s\n", strlen(data), data); //Log
    
    /* parse data to username and password */
    //Get pos of separator
    for (separator_pos = 0; separator_pos < strlen(data) && data[separator_pos] != USER_PASSWORD_REQUEST_SEPARATOR; separator_pos++);

    //Vérifier la longueur des paramètres de la chaîne
    if (separator_pos >= MAX_USER_USERNAME_LENGTH || strlen(data)-separator_pos > MAX_USER_PASSWORD_LENGTH){
        (*parent_info).request.type = -1; //Une erreur s'est produite
        strcpy((*parent_info).request.data,"ERROR: Le login ou le mdp est trop long");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client));
        pthread_exit(NULL);
    }else if (separator_pos == 0 || separator_pos == strlen(data) || separator_pos == strlen(data)-1){
        (*parent_info).request.type = -1; //Une erreur s'est produite
        strcpy((*parent_info).request.data,"ERROR: Le login ou le mdp est vide");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client));
        pthread_exit(NULL);
    }

    /* Copier le nom d'utilisateur et le mot de passe à partir des données */
    strncpy(username,data,separator_pos);
    username[separator_pos]='\0';
    strncpy(password,&data[separator_pos]+1,strlen(data)-separator_pos);
    
    if(findNickname(username,password,ACCOUNT_FILE,1) != 1){
        (*parent_info).request.type = -1; 
        strcpy((*parent_info).request.data,"ERROR: Mauvais login/mot2passe");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
    }else{
        /* Sending response */
        if(delete(username,ACCOUNT_FILE) != 1){
            (*parent_info).request.type = -1; 
            strcpy((*parent_info).request.data,"ERROR: Une erreur s'est produite");
            sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
        }else{
            (*parent_info).request.type = 0; 
            strcpy((*parent_info).request.data,"Compte supprimé");
            sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
        }
    }
    
    pthread_exit(NULL);
}

void *connected_users(void* args){
    /* Requete des informations */
    struct request_processing *parent_info = args;
    
    char connected_list[REQUEST_DATA_MAX_LENGTH]; //String de tous les noms d'utilisateurs connectés
    strcpy(connected_list,"");
    int bool_empty_list = 1; //S'il y a au moins un utilisateur connecté

    printf("[Thread- UtilisateurConnecté] - Données reçues (length : %ld): %s\n", strlen((*parent_info).request.data), (*parent_info).request.data); //Log

    /* Vérification de la taille */
    if(MAX_USERS_CONNECTED*MAX_USER_USERNAME_LENGTH > REQUEST_DATA_MAX_LENGTH){ //Vérification de la taille
        //La demande peut ne pas contenir tous les noms d'utilisateur
        (*parent_info).request.type = -1; 
        strcpy((*parent_info).request.data,"Request data is too short (modify it in server side)");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
    }

    /* construction de la réponse*/
    for (size_t i = 0; i < MAX_USERS_CONNECTED; i++)
    {
        if (strcmp((*parent_info).shared_memory[i].username,"") != 0){
            bool_empty_list = 0; //Détection d'un utilisateur
            strcat(connected_list,(*parent_info).shared_memory[i].username);
            connected_list[strlen(connected_list)+1] = '\0'; //Ajout d'un caractère de fin de chaîne
            connected_list[strlen(connected_list)] = USER_PASSWORD_FILE_SEPARATOR; //Ajout d'un séparateur (identique au nom d'utilisateur/mot de passe)
        }
    }
    connected_list[strlen(connected_list)-1] = '\0'; //remplacement du dernier séparateur par un caractère de fin de chaîner
    
    /* Envoi de la réponse */
    if (bool_empty_list){ //Liste vide
        (*parent_info).request.type = 0; 
        strcpy((*parent_info).request.data,"");
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
    }else{
        (*parent_info).request.type = 0; 
        strcpy((*parent_info).request.data,connected_list);
        sendto ((*parent_info).sock, (void *) &(*parent_info).request, sizeof(struct request), 0, (struct sockaddr *) &(*parent_info).adr_client, sizeof((*parent_info).adr_client)); 
    }

    pthread_exit(NULL);
}

#include "threads.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/msg.h>

#include "../utils/colors.h"

void* collectMessages(void* arg) {
    int msgid = (size_t) arg;
    messageSignal req;
    size_t size = sizeof(req) - sizeof(long);
    while (1) {
        if (msgrcv(msgid, &req, size, 1, 0) == -1) {
            perror("P1: Erreur pendant la réception de la requête");
            return NULL;
        }
        printf(C_CYN "%s:" C_RESET " %s\n", req.username, req.message);
    }
    pthread_exit(0);
}

void* collectMoves(void* arg) {
    int msgid = (size_t) arg;
    moveSignal req;
    size_t size = sizeof(req) - sizeof(long);
    while (1) {
        if (msgrcv(msgid, &req, size, 2, 0) == -1) {
            perror("P2: Erreur pendant la réception de la requête");
            return NULL;
        }
        if (req.is_join)
            printf("%s à rejoint le chat\n", req.username);
        else
            printf("%s à quitté le chat\n", req.username);
    }
    pthread_exit(0);
}

void* collectSignals(void* arg) {
    int msgid = (size_t) arg;
    stopSignal req;
    size_t size = sizeof(req) - sizeof(long);
    while (1) {
        if (msgrcv(msgid, &req, size, 3, 0) == -1) {
            perror("P3: Erreur pendant la réception de la requête");
            return NULL;
        }
        switch (req.signal) {
            case 0:
                printf("Fermeture de la console...\n");
                pthread_exit(0);
                return NULL;
            case 1:
                printf("Vous avez rejoint le chat\n");
                break;
            case 2:
                printf("Vous avez quitté le chat\n");
                break;
            default:
                printf("Signal inconnu\n");
        }
    }
    pthread_exit(0);
}
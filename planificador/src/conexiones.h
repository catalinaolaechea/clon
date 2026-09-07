#ifndef CONEXIONES_H
#define CONEXIONES_H

#include <pthread.h>

// Conexiones salientes del Planificador. Se abren una sola vez en el arranque y
// viven toda la ejecucion: no se abren ni se cierran por operacion.
//
// Cada socket tiene su propio mutex porque a partir del Check 2 los hilos de
// atencion de Cores van a escribir sobre estos fd en paralelo, y un paquete se
// envia en varios write(): sin el mutex dos hilos intercalan bytes y el
// receptor lee un mensaje corrupto. Tomar el mutex del socket antes de
// enviar_paquete() y soltarlo despues de leer la respuesta.
typedef struct {
    int fd_placa;
    pthread_mutex_t mutex_placa;

    int fd_storage;
    pthread_mutex_t mutex_storage;
} t_conexiones_planificador;

extern t_conexiones_planificador conexiones_planificador;

#endif

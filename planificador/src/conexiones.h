#ifndef CONEXIONES_H
#define CONEXIONES_H

#include <pthread.h>
#include <utils/protocolo.h>

#define ERROR_CONEXION -1

// Conexiones salientes del Planificador: se abren en el arranque y viven toda la ejecucion.
typedef struct {
    int fd_placa;                   // -1 mientras no se conecto
    pthread_mutex_t mutex_placa;    // un paquete son varios write(): sin esto dos hilos intercalan bytes
    int fd_storage;                 // -1 mientras no se conecto
    pthread_mutex_t mutex_storage;  // uno por socket para no bloquear Placa y Storage entre si
} t_conexiones_planificador;

extern t_conexiones_planificador conexiones_planificador;

int conectar_a_modulo(char* ip, char* puerto, t_modulo modulo_destino);  // devuelve el fd o ERROR_CONEXION
void inicializar_conexiones(void);  // conecta Placa y Storage en ese orden, aborta si alguna falla

#endif

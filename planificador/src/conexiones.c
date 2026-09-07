#include "conexiones.h"

// Los fd arrancan en -1 (el mismo valor que deja liberar_conexion) para que un
// uso antes del arranque falle de entrada en vez de escribir sobre el fd 0.
t_conexiones_planificador conexiones_planificador = {
    .fd_placa      = -1,
    .mutex_placa   = PTHREAD_MUTEX_INITIALIZER,
    .fd_storage    = -1,
    .mutex_storage = PTHREAD_MUTEX_INITIALIZER
};

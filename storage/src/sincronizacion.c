//! solo aca van a vivir los mutex, el unico juego de "candados" para todo el Storage (por ahora)
#include "sincronizacion.h"

// deja el candado listo para usar desde que arranca el proceso sin llamar a "pthread_mutex_init()" del main, al ser globales, estaticos y sin otros atributos es suficiente y evita la posibilidad que alguien haya lockeado antes que el main haya inicializado el candado, si necesitara que fueran dinamicos o compartido entre procesos usaria la funcion 
pthread_mutex_t mutex_filesystem = PTHREAD_MUTEX_INITIALIZER;  
pthread_mutex_t mutex_journal = PTHREAD_MUTEX_INITIALIZER;

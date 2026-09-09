//! declaramos los 2 mutex ("candado" de una sola llave, el hilo hace "lock()" asi que en caso de haber 2 hilos no se usen al mismo tiempo) que van a proteger las estructuras compartidas de 2 "SAVE_CHECKPOINT" simultaneos y evitar bugs que conlleva guardar un checkpoint 

#ifndef STORAGE_SINCRONIZACION_H_ 
#define STORAGE_SINCRONIZACION_H_ 

#include <pthread.h>  // biblioteca POSIX para hilos, para importar el tipo "pthread_mutex_t" y las funciones "pthread_mutex_lock()/unlock()"

//! 2 candados porque son 2 recursos != con reglas de acceso != entre si. El volumen (superbloque + FAT + directorio) es de acceso ALEATORIO de bloque en bloque, el journal es de escritura SECUENCIAL en orden sin intercalarse con un candado unico (si plantearamos un unico mutex, que es posible, perderiamos la concurrencia)
extern pthread_mutex_t mutex_filesystem;  // protege el volumen y la region del directorio "volumen.dat"
extern pthread_mutex_t mutex_journal;     // protege el journal y garantiza que espacios en "journal.dat" se escriban de forma secuencial 

//! porque estos archivos aparte y no dentro del servidor? porque los mutex NO son del servidor, son del filesystem, el servidor multihilo CREA el posible problema (de golpe varios hilos tocando lo mismo) pero lo que hay que proteger que es el volumen y journal lo escribe el "filesystem.c" mas adelante

#endif 


//! contrato del servidor

#ifndef STORAGE_SERVIDOR_PLANIFICADOR_H_ 
#define STORAGE_SERVIDOR_PLANIFICADOR_H_

#include <pthread.h>      
#include <commons/log.h>  // vamos a necesitar del tipo t_log porque recibimos un logger que creo el main 

//! para evitar los numeros sueltos o "magic numbers"
#define STORAGE_ERROR_SERVIDOR -1  // de error en general pero especificamente vienen de "iniciar_servidor()" de utils cuando falla
#define STORAGE_ERROR_CLIENTE  -1  // lo mismo pero de "esperar_cliente()"
#define STORAGE_HILO_CREADO     0  // si, el exito para crear un hilo es 0 y no 1 asi esta diseñado 

pthread_t storage_servidor_iniciar(char* puerto, t_log* logger);  // recibe el puerto de donde escuchar que viene del .config del modulo y una instancia de logger. Abre el socket de escucha y larga el hilo que esta aceptando conexiones para siempre, devuelve dicha instancia de "pthread_t" para que main pueda hacer un "pthread_join()" y se quede esperandolo asi vive el proceso, en caso de no poder abrir el puerto corta el programa


//! IMPORTANTE: cuando se crea un hilo, al terminar queda un pedacito de memoria con su valor de retorno, con join si nos interesa cuando termine vamos nosotros a buscarlo y bloquea al que llama y se ve que devolvio, y el detach deja que el sistema lo haga y sigue de largo a las llamadas pero no se ve que devolvio 
#endif 
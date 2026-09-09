#include <stdio.h>
#include <stdlib.h>  // para EXIT_SUCCESS y EXIT_FAILURE
#include <commons/log.h>  // t_log, log_info, log_destroy y log_level_from_string
#include <utils/log.h>  // "iniciar_logger()" wrapper nuestro 
#include "config.h"  // contrato con todo lo q se necesita para poder usar las funciones denotadas ahi
#include <pthread.h>
#include "servidor_planificador.h"

#define STORAGE_ARCHIVO_LOG "runtime/storage.log"  // los archivos .log van en /runtime porque son artefactos que se generan al correr 

int main(int argc, char* argv[]) {  // "argc" = cantidad argumentos que vinieron, "argv" = texto de cada uno
    //! 1) enunciado pide invocar "./bin/storage [Archivo Config]", osea 2 argumentos, el nombre del programa (que siempre viene) y el path del .config
    if (argc != 2) {  // tonc dado el caso que no llego el path, o algun error al recibir uno de los 2 argumentos atajamos el error 
        fprintf(stderr, "Uso: %s [archivo de configuracion]\n", argv[0]); 
        return EXIT_FAILURE; 
    }

    //! 2) en una instancia de .config mandamos a cargar el mismo con el path del storage
    t_storage_config* config = storage_config_cargar(argv[1]);

    if (config == NULL) {
        return EXIT_FAILURE;  // atajamos estos errores pero no los mandamos al logger porque todavia no lo creamos (IMRPORTANTE: lo reservamos para cuando corra el modulo, lo demas lo atajamos antes de hacerlo correr pero suficientemente documentado para saber el tipo de error)
    }

    //! 3) creamos el logger
    t_log_level nivel = log_level_from_string(config -> log_level); // el tipo "t_log_level" es un enum, guardamos especificamente el nivel que tenemos guardado en nuestro .config del modulo alli 
    t_log* logger = iniciar_logger(STORAGE_ARCHIVO_LOG, STORAGE_NOMBRE_MODULO, nivel);

    log_info(logger, "Modulo Storage iniciado");
    storage_config_loguear(config, logger); 

    //! 4) dejamos el PATH_STORAGE listo para trabajar, si no se pudo velamos todo ambos el .config y el logger, ya que sin poderse crear ese directorio no hay donde guardar el volumen ni el journal 
    if(!storage_config_asegurar_path(config, logger)) {
        log_destroy(logger);
        storage_config_destruir(config);
        return EXIT_FAILURE;
    }

    //todo issue-24  ----------------------------------------
    //! 5) levantamos el servidor multihilo. Le pasamos el puerto que leimos del .config y el logger ya creado.
    // La funcion abre el socket, larga el hilo que acepta conexiones y nos devuelve el identificador de ese hilo
    pthread_t hilo_servidor = storage_servidor_iniciar(config->puerto_escucha, logger);

    //! 6) y aca nos quedamos clavados. El hilo de escucha tiene un "while(1)" adentro asi que nunca termina, y
    // ese es justamente el punto: si el main siguiera de largo llegaria al "return" de abajo, y cuando el hilo
    // principal termina se muere el proceso ENTERO, servidor y conexiones incluidas. El join lo deja esperando
    // sin quemar CPU, que es la forma de mantener vivo al Storage
    pthread_join(hilo_servidor, NULL);

    //! 7) todo lo que se pidio lo liberamos. En los hechos no se llega nunca aca (el join de arriba no vuelve),
    // pero lo dejamos escrito igual: el dia que le agreguemos un cierre ordenado por SIGINT, esto ya va a estar
    log_destroy(logger);
    storage_config_destruir(config);
    return EXIT_SUCCESS;
} 
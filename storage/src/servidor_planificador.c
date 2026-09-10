//! el servidor del Storage. Aca vive toda la maquinaria de red: abrir el puerto, aceptar conexiones y atender a cada una en su propio hilo. De todo esto, main.c solo ve "storage_servidor_iniciar()".

#include "servidor_planificador.h"  

#include <stdlib.h>  // malloc, free, exit, EXIT_FAILURE
#include <stdint.h>  // el tipo "uint8_t" del op_code

#include <utils/sockets.h>        // iniciar_servidor, esperar_cliente, liberar_conexion
#include <utils/handshake.h>      // recibir_handshake
#include <utils/protocolo.h>      // t_modulo, modulo_to_string y el texto del log obligatorio
#include <utils/serializacion.h>  // recibir_operacion, recibir_buffer, t_buffer y los CONEXION_*

static t_log* logger;    // el logger que nos pasa el main, lo guardamos para no arrastrarlo en cada firma
static int fd_servidor;  // el socket de escucha, el que esta "atado" al puerto y recibe las conexiones


//! aca va a ir el switch de los mensajes que nos mande el Planificador. Por ahora no hay ninguno definido
// (el MENSAJE_PRUEBA es de SIS-35 y los SAVE/LOAD/DELETE_CHECKPOINT son del check 3), asi que lo unico que
// hacemos es avisar por log que llego algo que no sabemos atender, sin cortar la conexion por eso
static void atender_mensaje(int fd, uint8_t op_code, t_buffer* payload) {
    (void) fd;       // todavia no le respondemos nada al Planificador, pero lo vamos a necesitar en SIS-35
    (void) payload;  // idem: el contenido del mensaje recien se lee cuando haya mensajes que leer

    switch (op_code) {
        default:
            log_warning(logger, "Codigo de operacion desconocido recibido: %u", op_code);
    }
}


//! el hilo de ATENCION: corre uno por cada conexion. Recibe el descriptor envuelto en un "void*" porque esa
// es la firma que exige pthread_create, no se le puede pasar un int pelado
static void* atender_planificador(void* ctx) {
    int fd = *(int*) ctx;  // desenvolvemos el descriptor...
    free(ctx);             // ...y liberamos la cajita que nos mandaron, ya no hace falta

    //! MODULO_INVALIDO (que vale 0) no es un capricho: si el cliente se corta antes de mandar el handshake,
    // "recibir_handshake" vuelve sin haber tocado esta variable, y si no la inicializamos estariamos
    // logueando basura de la pila justo aca abajo, en el mensaje de rechazo
    t_modulo modulo = MODULO_INVALIDO;
    t_canal canal;
    char* identificador;  // lo pide la firma de utils; solo el Core manda uno real, el Planificador no

    if (recibir_handshake(fd, MODULO_STORAGE, &modulo, &canal, &identificador) != CONEXION_OK) {
        //! utils ya rechazo y le contesto al cliente por nosotros: su tabla dice que al Storage solo le
        // entra el Planificador por CANAL_UNICO. Lo unico que nos toca es dejar asentado el intento
        log_warning(logger, "Handshake rechazado en el socket %d (modulo: %s)", fd, modulo_to_string(modulo));
        liberar_conexion(&fd);
        return NULL;
    }

    //! log OBLIGATORIO del check 1, con el texto exacto que pide el enunciado ("## Modulo: <NOMBRE>").
    // La macro y el nombre capitalizado salen de utils/protocolo.h: no los escribimos a mano para que los
    // 4 modulos loguen igual y no se nos escape una diferencia de mayusculas
    log_info(logger, LOG_CONEXION_RECIBIDA, modulo_to_string(modulo));

    //! de aca en mas el hilo se queda en este while esperando mensajes, hasta que el Planificador se
    // desconecte o se rompa la conexion. Mientras tanto el accept loop sigue aceptando a otros
    while (1) {
        uint8_t op_code;
        int resultado = recibir_operacion(fd, &op_code);

        if (resultado == CONEXION_DESCONECTADO) {
            log_info(logger, "El Planificador se desconecto del socket %d", fd);
            break;
        }

        if (resultado != CONEXION_OK) {
            log_error(logger, "Error al recibir la operacion en el socket %d", fd);
            break;
        }

        //! IMPORTANTE: todo mensaje viaja como [op_code][tamanio][payload], asi que despues de leer el
        // op_code hay que consumir SI O SI el payload, aunque todavia no lo usemos. Si lo dejaramos ahi
        // tirado en el socket, la proxima vuelta del while leeria el primer byte del payload creyendo que
        // es un op_code, y el protocolo entero se desincroniza
        t_buffer* payload;
        resultado = recibir_buffer(fd, &payload);

        if (resultado != CONEXION_OK) {
            log_error(logger, "Error al recibir el payload del op_code %u en el socket %d", op_code, fd);
            break;
        }

        atender_mensaje(fd, op_code, payload);
        eliminar_buffer(payload);
    }

    liberar_conexion(&fd);  // cerramos el socket de ESTE cliente, el de escucha sigue vivo
    return NULL;            // al devolver NULL el hilo termina, y como esta detacheado se limpia solo
}


//! el "accept loop": el hilo que se queda para siempre aceptando conexiones
static void* escuchar_planificador(void* arg) {
    (void) arg;  // no recibimos nada, pero la firma de pthread_create obliga a declarar el parametro

    while (1) {
        int fd_cliente = esperar_cliente(fd_servidor);  // se duerme aca hasta que alguien se conecta

        if (fd_cliente == STORAGE_ERROR_CLIENTE) {
            //! "continue" y NO "break": que falle un accept suelto no puede matar al servidor. El issue pide
            // que el Planificador pueda reiniciarse y reconectar, y eso exige que la escucha nunca muera
            log_error(logger, "Error al aceptar una conexion entrante");
            continue;
        }

        //! pthread_create solo deja pasar un "void*", asi que el descriptor viaja en memoria dinamica.
        // No podemos pasarle "&fd_cliente" porque esa variable se pisa en la proxima vuelta del while: el
        // hilo terminaria leyendo el fd de OTRA conexion. La memoria la libera el propio hilo de atencion
        int* ctx = malloc(sizeof(int));
        *ctx = fd_cliente;

        pthread_t hilo_atencion;
        int resultado = pthread_create(&hilo_atencion, NULL, atender_planificador, ctx);

        if (resultado != STORAGE_HILO_CREADO) {
            log_error(logger, "No se pudo crear el hilo de atencion para el socket %d", fd_cliente);
            free(ctx);
            liberar_conexion(&fd_cliente);
            continue;
        }

        //! detach: no lo vamos a esperar con join (nos trabaria el loop y atenderiamos de a uno), asi que le
        // avisamos al sistema que lo limpie solo cuando termine. Sin esto, cada conexion que se cierra iria
        // dejando restos que se acumulan
        pthread_detach(hilo_atencion);
    }

    return NULL;  // inalcanzable, pero la firma de pthread_create pide devolver un void*
}


//! unica funcion publica: la que llama el main
pthread_t storage_servidor_iniciar(char* puerto, t_log* logger_recibido) {
    logger = logger_recibido;  // lo guardamos en la static, asi lo ven todas las funciones de arriba

    fd_servidor = iniciar_servidor(puerto);

    if (fd_servidor == STORAGE_ERROR_SERVIDOR) {
        //! si no podemos abrir el puerto no hay nada que hacer: un Storage al que nadie puede conectarse no
        // le sirve a nadie, y seguir andando seria fingir que esta todo bien. Cortamos aca
        log_error(logger, "No se pudo iniciar el servidor en el puerto %s", puerto);
        exit(EXIT_FAILURE);
    }

    pthread_t hilo_escucha;
    int resultado = pthread_create(&hilo_escucha, NULL, escuchar_planificador, NULL);

    if (resultado != STORAGE_HILO_CREADO) {
        log_error(logger, "No se pudo crear el hilo de escucha del servidor");
        exit(EXIT_FAILURE);
    }

    log_info(logger, "Servidor del Storage escuchando en el puerto %s", puerto);

    return hilo_escucha;  // se lo devolvemos al main para que le haga join y no se muera el proceso
}
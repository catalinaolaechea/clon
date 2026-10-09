#include "inicializador.h"
#include "conexiones.h"
#include "dispatch.h"
#include "interrupcion.h"

int main(int argc, char* argv[]) {

    if (argc < 3) {
        fprintf(stderr, "Uso: %s [Archivo Config] [Identificador]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    archivo_config = strdup(argv[1]);
    identificador  = atoi(argv[2]);

    inicializar_config();
    inicializar_log();

    log_info(core_logger, "Core %d inicializado. Planificador=%s:%s - Placa=%s:%s", identificador, configuracion.ip_planificador, configuracion.puerto_planificador, configuracion.ip_placa, configuracion.puerto_placa);

    conectar_a_planificador_y_placa();
    conectar_canal_interrupt();   // segundo socket a Planificador, para interrupciones
    iniciar_hilo_interrupt();     // hilo que escucha ese socket

    esperar_dispatch();

    log_info(core_logger, "Core %d finalizando", identificador);
    detener_hilo_interrupt();     
    liberar_conexion(&fd_planificador);
    liberar_conexion(&fd_placa);
    liberar_conexion(&fd_interrupt);
    log_destroy(core_logger);
    config_destroy(core_config);
    free(archivo_config);

    return EXIT_SUCCESS;
}
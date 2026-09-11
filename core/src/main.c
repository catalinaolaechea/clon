
#include "inicializador.h"
#include "conexiones.h"
 
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

    probar_round_trip();  // Check 1: ida y vuelta de un paquete con Planificador y Placa

    log_info(core_logger, "Core %d finalizando", identificador);
    liberar_conexion(&fd_planificador);
    liberar_conexion(&fd_placa);
    log_destroy(core_logger);
    config_destroy(core_config);
    free(archivo_config);
     
    return EXIT_SUCCESS;
}

#include "inicializador.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s [Archivo Config] [Identificador]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
 
    archivo_config = strdup(argv[1]);
    identificador  = strdup(argv[2]);
 
    inicializar_config();
    inicializar_log();
 
    log_info(core_logger, "Core %s inicializado. Planificador=%s:%s - Placa=%s:%s", identificador, IP_PLANIFICADOR, PUERTO_PLANIFICADOR, IP_PLACA, PUERTO_PLACA);
    
    log_info(core_logger, "Core %s finalizando.", identificador);
    log_destroy(core_logger);
    config_destroy(core_config);
    free(archivo_config);
    free(identificador);
 
    return EXIT_SUCCESS;
}

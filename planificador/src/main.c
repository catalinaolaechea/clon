#include "inicializador.h"
#include "servidores_core.h"

int main(int argc, char* argv[]) {
    
    if(argc != 3){
        printf("Uso: %s [archivo config] [path job inicial]\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    archivo_config = argv[1];
    path_job_inicial = argv[2];

    inicializar_config();
    inicializar_log();

    // Siguiente ISSUE

    log_destroy(planificador_logger);
    config_destroy(planificador_config);

    return EXIT_SUCCESS;
}
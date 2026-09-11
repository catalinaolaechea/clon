#include "inicializador.h"
#include "conexiones.h"
#include "servidores_core.h"

int main(int argc, char* argv[]) {
    
    if(argc < 2){
        printf("Uso: %s [archivo config] [path job inicial]\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    archivo_config = argv[1];
    //path_job_inicial = argv[2];

    inicializar_config();
    inicializar_log();

    inicializar_conexiones();  // antes del servidor: si falla aborta y no levanta nada

    probar_round_trip();  // Check 1: ida y vuelta de un paquete con Placa y Storage

    pthread_t hilo_servidor = iniciar_servidor_cores();
    pthread_join(hilo_servidor, NULL);
    log_destroy(planificador_logger);
    config_destroy(planificador_config);

    return EXIT_SUCCESS;
}
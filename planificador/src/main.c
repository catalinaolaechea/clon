#include "inicializador.h"
#include "conexiones.h"
#include "servidores_core.h"
#include "estados.h"
#include "jobs.h"
#include "largo_plazo.h"
#include "corto_plazo.h"
#include "page_fault.h"
#include "servicios.h"

int main(int argc, char* argv[]) {
    
    if(argc < 3){
        printf("Uso: %s [archivo config] [path job inicial]\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    archivo_config = argv[1];
    path_job_inicial = argv[2];

    inicializar_config();
    inicializar_log();

    inicializar_conexiones();  // antes del servidor: si falla aborta y no levanta nada

    probar_round_trip();  // Check 1: ida y vuelta de un paquete con Placa y Storage

    inicializar_estados();  // Check 2: inicializa las colas de estados y el mutex

    inicializar_largo_plazo();
    inicializar_corto_plazo();  // antes de arrancar el largo plazo y el servidor: los dos le avisan
    inicializar_page_faults();  // antes del servidor: los hilos de los Cores le encolan
    inicializar_servicios();    // idem

    crear_job(path_job_inicial);

    iniciar_largo_plazo();
    iniciar_corto_plazo();
    iniciar_page_faults();
    iniciar_servicios();

    pthread_t hilo_servidor = iniciar_servidor_cores();

    pthread_join(hilo_servidor, NULL);
    destruir_largo_plazo();
    destruir_corto_plazo();
    destruir_page_faults();
    destruir_servicios();
    destruir_estados(); 
    log_destroy(planificador_logger);
    config_destroy(planificador_config);

    return EXIT_SUCCESS;
}
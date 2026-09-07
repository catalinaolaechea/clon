#include "inicializador.h"
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

    int socket_placa = crear_conexion(config_planificador.ip_placa,config_planificador.puerto_placa);

    int conexion = enviar_handshake(socket_placa,MODULO_PLANIFICADOR,CANAL_UNICO); 

    pthread_t hilo_servidor = iniciar_servidor_cores();
    pthread_join(hilo_servidor, NULL);
    log_destroy(planificador_logger);
    config_destroy(planificador_config);

    return EXIT_SUCCESS;
}
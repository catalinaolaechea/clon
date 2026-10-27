#include "inicializador.h"
#include "atender_core.h"
#include "atender_planificador.h"
#include "consola.h"
#include "jobs.h"
#include "memoria.h"
#include "offload.h"

static void* atender_cliente(void* socket);

// [Enunciado pág. 21 - Módulo Placa / Lineamiento e Implementación]
// La Placa administra la memoria del sistema: guarda las instrucciones de los Jobs y el espacio
// de trabajo sobre el que operan. Se ejecuta como: ./bin/placa [Archivo Config]
// Al iniciar crea un servidor multihilo que atiende de forma concurrente al Planificador y a los
// Cores. Como se pueden conectar Cores nuevos en cualquier momento, la escucha queda siempre activa.
int main(int argc, char* argv[]) {

    if(argc < 2){
        //printf("Uso: %s [archivo config]",argv[0]);
        exit(EXIT_FAILURE);
    }

    archivo_config = strdup(argv[1]);

    inicializar_config();
    inicializar_log();


    if(!validar_config() || !inicializar_memoria() || !inicializador_offload()){
        log_error(placa_logger, "Configuración invalida, la placa no arranca");
        log_destroy(placa_logger);
        config_destroy(placa_config);
        free(archivo_config);
        return EXIT_FAILURE;

    }

    lista_core = list_create();
    lista_jobs = list_create();

    int server_placa = iniciar_servidor(PUERTO_ESCUCHA);

    log_info(placa_logger,"Servidor listo para recibir a los clientes");

    //consola
    pthread_t hilo_consola;
    pthread_create(&hilo_consola, NULL, atender_consola, NULL);
    pthread_detach(hilo_consola);


    while(1){
        int socket_cliente = esperar_cliente(server_placa);

        int* socket_cliente_ptr = malloc(sizeof(int));

        *socket_cliente_ptr = socket_cliente;

        pthread_t hilo_clientes;
        pthread_create(&hilo_clientes,NULL, atender_cliente,socket_cliente_ptr);
        pthread_detach(hilo_clientes);

    }

    return EXIT_FAILURE;
}

// [Enunciado pág. 21 - Servidor multihilo] + [pág. 24 - Log obligatorio de conexión]
// Cada conexión nueva se identifica con el handshake y se deriva a su propio hilo: uno para el
// Planificador y uno por cada Core. Log obligatorio: "## Módulo: <NOMBRE_MODULO_CONECTADO>"
static void* atender_cliente(void* socket){

    int socket_cliente = *(int*) socket;
    free(socket);

    t_modulo cliente;
    t_canal canal;
    int identificador;

    int resultado = recibir_handshake(socket_cliente, MODULO_PLACA, &cliente, &canal, &identificador);

    if (resultado != CONEXION_OK) {
        log_error(placa_logger, "## Error al recibir handshake del Core en el socket %d", socket_cliente);
        liberar_conexion(&socket_cliente);
        return NULL;
    }


    switch (cliente)
        {
        case MODULO_PLANIFICADOR:{

            log_info(placa_logger,"## Módulo: %s", "Planificador");

            int* socket_planificador_ptr = malloc(sizeof(int));
            *socket_planificador_ptr = socket_cliente;

            pthread_t hilo_planificador;
            pthread_create(&hilo_planificador,NULL,atender_planificador,socket_planificador_ptr);
            pthread_detach(hilo_planificador);

            break;
        }
        case MODULO_CORE:{

            t_core_placa* core = malloc(sizeof(t_core_placa));
            core->socket_core = socket_cliente;
            core->id_core = identificador;

            log_info(placa_logger,"## Se conecto el Módulo Core %d",core->id_core);

            pthread_mutex_lock(&mutex_lista_core);
                list_add(lista_core,core);
            pthread_mutex_unlock(&mutex_lista_core);


            pthread_t hilo_cpu;
            pthread_create(&hilo_cpu,NULL ,atender_core, core);
            pthread_detach(hilo_cpu);
            break;
        }

        default:

            log_info(placa_logger,"el cliente se desconecto");

            liberar_conexion(&socket_cliente);

            break;
        }


    return EXIT_SUCCESS;

}

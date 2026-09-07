#include "inicializador.h"

int main(int argc, char* argv[]) {
    
    if(argc < 2){
        //printf("Uso: %s [archivo config]",argv[0]);
        exit(EXIT_FAILURE);
    }

    archivo_config = strdup(argv[1]);

    inicializar_log();
    inicializar_config();

    int server_placa = iniciar_servidor(PUERTO_ESCUCHA);

    log_info(placa_logger,"Servidor listo para recibir a los clientes");    


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

void* atender_cliente(void* socket){

    int socket_cliente = *(int*) socket;
    free(socket);

    t_modulo cliente;
    t_canal canal;
    int identificador;

    int resultado = recibir_handshake(socket_cliente, MODULO_PLACA, &cliente, &canal, &identificador);

    switch (cliente)
        {
        case MODULO_PLANIFICADOR:{ 
            
            log_info(placa_logger, "Se conecto el planificador");
            
            int* socket_planificador_ptr = malloc(sizeof(int));
            *socket_planificador_ptr = socket_cliente;

            pthread_t hilo_planificador;
            pthread_create(&hilo_planificador,NULL,atender_planificador,socket_planificador_ptr);
            pthread_detach(hilo_planificador);

            break;            
        }
        case MODULO_CORE:{
            log_info(placa_logger, "Se conecto el core");
            
            //recibir id del cpu 
            /*int id_cpu = recibir_id();
            t_core_placa* cpu = malloc(size(t_core_placa));
            */

            int* socket_core_ptr = malloc(sizeof(int));
            *socket_core_ptr = socket_cliente;

            pthread_t hilo_cpu;
            pthread_create(&hilo_cpu,NULL ,atender_core, socket_core_ptr);
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

void* atender_core(void* core){

    int socket_cliente = *(int*) core;
    free(core);

    while(1){
        uint8_t operacion_core;
        int codigo_validacion = recibir_operacion(socket_cliente,&operacion_core);
        

        switch (operacion_core)
        {
        case FETCH_INSTRUCCION:{
            break;

        }

        case RESPUESTA_INSTRUCCION:{
            break;
            
        }
        case OBTENER_MARCO:{
            break;
            
        }
        case RESPUESTA_MARCO:{
            break;
            
        }
        case PAGE_FAULT:{
            break;
            
        }        
        case DESLOCKEAR_PAGINAS:{
            break;
            
        }      
        
        /*case CONEXION_ERROR:{

            log_error(placa_logger, "## CORE #agregarID# desconectada");
            liberar_conexion(&socket_cliente);
            break;
        }*/
        
        default:
            log_warning(placa_logger, "Operación desconocida recibida de CORE: %d",operacion_core);
            break;

        }
    
    }

    return EXIT_SUCCESS;
    
}

void* atender_planificador(void* planificador){

    int socket_cliente = *(int*) planificador;
    free(planificador);

    while(1){
        uint8_t operacion_planificador;
        int codigo_validacion = recibir_operacion(socket_cliente,&operacion_planificador);

        log_info(placa_logger,"Estoy aca ");
        switch (operacion_planificador)
        {
        case CREAR_JOB:{
            break;

        }

        case FINALIZAR_JOB:{
            break;
            
        }
        case CARGAR_PAGINA:{
            break;
            
        }
        case CONEXION_ERROR: {

            log_error(placa_logger,"## Planificador desconectado");
            liberar_conexion(&socket_cliente);
            exit(EXIT_FAILURE);
            break;
        }
        default:
            log_warning(placa_logger, "Operación desconocida recibida del PLANIFICADOR: %d", operacion_planificador);
            break;

        }

    
    }

    return EXIT_SUCCESS;
    
}

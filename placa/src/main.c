#include "inicializador.h"

int main(int argc, char* argv[]) {
    
    if(argc < 2){
        //printf("Uso: %s [archivo config]",argv[0]);
        exit(EXIT_FAILURE);
    }

    archivo_config = strdup(argv[1]);

    inicializar_log();
    inicializar_config();

    lista_core = list_create();

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

void* atender_consola(void* arg){
    char* linea;

    while((linea = readline("Placa> ")) !=NULL){
        if(strlen(linea)>0){
            add_history(linea);
            procesar_comando(linea);
        }
        free(linea);
    }
    return NULL;
}

void procesar_comando(char* linea){
    char** args = string_split(linea, " ");

    if(strcmp(args[0], "INFO") ==0){
        comando_info();
    }else if(strcmp(args[0],"TLS") ==0){
        comando_tls();
    }else {
        printf("Comando desconocido: %s\n",args[0]);
    }

    string_array_destroy(args);
}

void comando_info(){
    pthread_mutex_lock(&mutex_lista_core);
    int cantidad_cores = list_size(lista_core);
    pthread_mutex_unlock(&mutex_lista_core);

    printf("Cores conectados: %d\n", cantidad_cores);

    /*Falta
    porcentaje de memoria/offload
    frames libres/lockeados/totales
    cantidad de jobs
    
    */
}

void comando_tls(){
    pthread_mutex_lock(&mutex_lista_core);

    for(int i = 0; i < list_size(lista_core); i++){
        t_core_placa* core = list_get(lista_core,i);
        printf("Core conectado - ID: %d\n",core->id_core);
    }

    pthread_mutex_unlock(&mutex_lista_core);

    /*Falta
    Listar jobs(no cores) con paginas de conjunto residente/totales
    */
}



void* atender_cliente(void* socket){

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

            log_info(placa_logger,"## Módulo: %s %d", "Core",core->id_core);

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

void* atender_core(void* args){

    t_core_placa* core = (t_core_placa*) args;

    int socket_cliente = core->socket_core;

    while(1){
        uint8_t operacion_core;
        int codigo_validacion = recibir_operacion(socket_cliente,&operacion_core);
        

        if (codigo_validacion == CONEXION_ERROR) {
            log_error(placa_logger, "Operación desconocida recibida de CORE %d: %d", core->id_core, operacion_core);
            break;
        }

        if (codigo_validacion == CONEXION_DESCONECTADO) {
            log_info(placa_logger, " ## Core %d se ha desconectado", core->id_core);
            break;
        }


        switch (operacion_core)
        {
        case MENSAJE_PRUEBA:{
            log_info(placa_logger, "Recibido MENSAJE_PRUEBA del Core con identificador: %d", core->id_core);
            
            t_paquete* eco = crear_paquete(MENSAJE_PRUEBA_ECO);
            enviar_paquete(core->socket_core,eco);
            eliminar_paquete(eco);
            
            break;

        }
        case FETCH_INSTRUCCION:{
            
            log_info(placa_logger, "## JID: #id# - Obtener instrucción: #pc# - Instrucción ");

            break;

        }

        /*case LEER_DATO:{

            break;
            
        }
        case ESCRIBIR_DATO:{

            break;
        }
            
        */

        case OBTENER_MARCO:{
            break;
            
        }

        /*case RESPUESTA_MARCO:{
            break;
            
        }*/

        case PAGE_FAULT:{
            break;
            
        }        
        case DESLOCKEAR_PAGINAS:{
            break;
            
        }      
        
        default:
            log_warning(placa_logger, "Operación desconocida recibida de CORE: %d",operacion_core);
            break;

        }
    
    }

    // saco al core de la lista y libero, UNA sola vez, acá afuera del while
    pthread_mutex_lock(&mutex_lista_core);
    list_remove_element(lista_core, core);
    pthread_mutex_unlock(&mutex_lista_core);

    liberar_conexion(&socket_cliente);
    free(core);

    return NULL;

}

void* atender_planificador(void* planificador){

    int socket_cliente = *(int*) planificador;
    free(planificador);

    while(1){
        uint8_t operacion_planificador;
        int codigo_validacion = recibir_operacion(socket_cliente,&operacion_planificador);

        if (codigo_validacion == CONEXION_ERROR) {
            log_error(placa_logger, "Operación desconocida recibida del PLANIFICADOR: %d", operacion_planificador);
            break; 
        }

        if (codigo_validacion == CONEXION_DESCONECTADO) {
            log_info(placa_logger, "## Planificador desconectado");
            break;
        }


        switch (operacion_planificador)
        {
        case MENSAJE_PRUEBA:{
            log_info(placa_logger, "Recibido MENSAJE_PRUEBA del Planificador");
            break;

        }
        case CREAR_JOB:{

            //falta id
            log_info(placa_logger, "## JIB #id# - Job Creado ");

            break;

        }

        /*case LEER_DATO:{
            break;
            
        }
        case ESCRIBIR_DATO:{
            break;
        
        }
            
        */

        case FINALIZAR_JOB:{
            

            
            break;
            
        }
        case CARGAR_PAGINA:{
            break;
            
        }
        default:
            log_warning(placa_logger, "Operación desconocida recibida del PLANIFICADOR: %d", operacion_planificador);
            break;

        }

    
    }

    liberar_conexion(&socket_cliente);

    return EXIT_SUCCESS;
    
}

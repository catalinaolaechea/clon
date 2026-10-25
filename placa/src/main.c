#include "inicializador.h"

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
    

    if(!validar_config() || !inicializar_memoria()){
        log_error(placa_logger, "Configuración invalidad, la placa no arranca");
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

// [Enunciado pág. 23-24 - Comandos locales]
// La Placa implementa una consola por stdin con los comandos INFO y TLS, que responden en formato
// "human-readable". Se pueden agregar otros comandos siempre que no modifiquen ni contradigan
// el funcionamiento del módulo.
void* atender_consola(void* arg){
    (void) arg;  // firma exigida por pthread_create, no se usa
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
    }

    else if(strcmp(args[0], "CREAR") == 0){
        comando_crear(args);
    }else if(strcmp(args[0], "FINALIZAR") == 0){
        comando_finalizar(args);
    }else if(strcmp(args[0], "FETCH") == 0){
        comando_fetch(args);
    }  

    else {
        printf("Comando desconocido: %s\n",args[0]);
    }

    string_array_destroy(args);
}


// debug: CREAR <jid> <archivo> -> igual que el CREAR_JOB del Planificador
void comando_crear(char** args){
    if (string_array_size(args) != 3) {
        printf("Uso: CREAR <jid> <archivo>\n");
        return;
    }

    uint32_t jid = strtoul(args[1], NULL, 10);
    t_job_placa* job = inicializar_job(jid, args[2]);

    if (job != NULL)
        printf("Job %u creado con %u instrucciones\n", jid, job->cant_instrucciones);
    else
        printf("No se pudo crear el Job %u\n", jid);
}

// debug: FETCH <jid> <pc> -> igual que el FETCH_INSTRUCCION del Core, pero sin red
void comando_fetch(char** args){
    if (string_array_size(args) != 3) {
        printf("Uso: FETCH <jid> <pc>\n");
        return;
    }

    uint32_t jid = strtoul(args[1], NULL, 10);
    uint32_t pc  = strtoul(args[2], NULL, 10);

    pthread_mutex_lock(&mutex_jobs);
        t_job_placa* job = buscar_job(jid);

        if (job == NULL)
            printf("JID %u inexistente\n", jid);
        else if (pc >= job->cant_instrucciones)
            printf("PC %u fuera de rango (el Job tiene %u instrucciones)\n", pc, job->cant_instrucciones);
        else
            printf("JID %u - PC %u: %s\n", jid, pc, job->instrucciones[pc]);
    pthread_mutex_unlock(&mutex_jobs);
}

// debug: FINALIZAR <jid> -> igual que el FINALIZAR_JOB del Planificador
void comando_finalizar(char** args){
    if (string_array_size(args) != 2) {
        printf("Uso: FINALIZAR <jid>\n");
        return;
    }

    finalizar_job(strtoul(args[1], NULL, 10));
}

// [Enunciado pág. 23 - Comandos locales: INFO]
// Muestra: porcentaje de memoria y de Offload ocupados; cantidad de frames libres, lockeados y
// totales; y cantidad de Jobs actuales.
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

// [Enunciado pág. 24 - Comandos locales: TLS]
// Lista todos los Jobs registrados indicando, para cada uno: ID, cantidad de páginas del conjunto
// residente (las que están cargadas en la Placa) y cantidad de páginas totales.
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

// [Enunciado pág. 21 - Servidor multihilo] + [pág. 24 - Log obligatorio de conexión]
// Cada conexión nueva se identifica con el handshake y se deriva a su propio hilo: uno para el
// Planificador y uno por cada Core. Log obligatorio: "## Módulo: <NOMBRE_MODULO_CONECTADO>"
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

// [Enunciado pág. 22-23 - Operaciones]
// Pedidos que llegan desde un Core: obtener una instrucción (Fetch), obtención de marco (MMU),
// lectura y escritura de datos, y deslockeo de páginas.
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

        // todo mensaje viaja como [op_code][tamanio][payload], asi que hay que consumir el payload
        // si o si, aunque el case todavia no lo use: si queda en el socket, la proxima vuelta lee el
        // primer byte del tamanio creyendo que es un op_code y el protocolo se desincroniza
        t_buffer* payload;
        codigo_validacion = recibir_buffer(socket_cliente, &payload);

        if (codigo_validacion != CONEXION_OK) {
            log_error(placa_logger, "Error al recibir el payload del op_code %d del CORE %d", operacion_core, core->id_core);
            break;
        }

        switch (operacion_core)
        {
        case MENSAJE_PRUEBA:{
            log_info(placa_logger, "Recibido MENSAJE_PRUEBA del Core con identificador: %d", core->id_core);

            t_mensaje_prueba* mensaje = mensaje_prueba_leer(payload);

            if (mensaje == NULL) {
                log_error(placa_logger, "MENSAJE_PRUEBA mal formado del Core %d", core->id_core);
                break;
            }

            mensaje_prueba_loguear(placa_logger, "MENSAJE_PRUEBA recibido del Core", mensaje);

            if (mensaje_prueba_responder_eco(socket_cliente, mensaje) != CONEXION_OK) {
                log_error(placa_logger, "No se pudo responder el eco al Core %d", core->id_core);
            }

            mensaje_prueba_destruir(mensaje);
            break;

        }
        // [Enunciado pág. 16 - Core / Fetch] + [pág. 21 - Memoria de Instrucciones]
        // El Core pide cada instrucción con el Program Counter, que es el número de instrucción
        // relativo al Job (PC = 0 es la primera línea). La Placa responde la instrucción de ese
        // Job; el enunciado no impone una forma específica de obtenerla.
        // Si el JID no existe o el PC se pasa del final, se responde FETCH_ERROR (decisión del
        // grupo, SIS-40) en vez de devolver basura.
        // Log obligatorio (pág. 24): "## JID: <JID> - Obtener instrucción: <PC> - Instrucción: <INSTRUCCIÓN> <...ARGS>"
        case FETCH_INSTRUCCION:{

            uint32_t jid = buffer_read_uint32(payload);
            uint32_t pc = buffer_read_uint32(payload);

            char* instruccion = NULL;
            int motivo_error = -1;

            pthread_mutex_lock(&mutex_jobs);

                t_job_placa* job = buscar_job(jid);

                if(job == NULL){
                    motivo_error = FETCH_ERROR_JID_INEXISTENTE;
                } else if (pc >= job->cant_instrucciones){
                    motivo_error = FETCH_ERROR_PC_FUERA_DE_RANGO;
                } else {
                    instruccion = strdup(job->instrucciones[pc]);
                }

            pthread_mutex_unlock(&mutex_jobs);

            retardo_memoria();

            t_paquete* respuesta;

            if(instruccion != NULL){
                log_info(placa_logger, "## JID: %u - Obtener instrucción: %u - Instrucción: %s", jid,pc,instruccion);
                respuesta = crear_paquete(RESPUESTA_INSTRUCCION);
                buffer_add_string(respuesta->buffer, instruccion);
                free(instruccion);
            }else {
                log_error(placa_logger, "Fetch inválido - JID: %u - PC: %u - Motivo: %d", jid, pc, motivo_error);

                respuesta = crear_paquete(FETCH_ERROR);
                buffer_add_uint32(respuesta->buffer, motivo_error);

            }

            int validacion = enviar_paquete(socket_cliente, respuesta);

            if(validacion != CONEXION_OK){
                log_error(placa_logger, "No se pudo responder el fetch al core %d", core->id_core);
            }

            eliminar_paquete(respuesta);
            break;

        }

        case OBTENER_TAM_PAGINA:{
            t_paquete* respuesta = crear_paquete(RESPUESTA_TAM_PAGINA);
            buffer_add_uint32(respuesta->buffer, TAM_PAGINA);

            if(enviar_paquete(socket_cliente, respuesta) != CONEXION_OK)
                log_error(placa_logger,"No se pudo enviar el tamaño de pagina al Core %d", core->id_core);

            eliminar_paquete(respuesta);

            break;
        }

        // [Enunciado pág. 23 - Lectura de datos] (Check 3)
        // Puede llegar del Core o del Planificador: JID, dirección física y tamaño a leer.
        // Se responde el contenido del rango pedido.
        // [Enunciado pág. 23 - Escritura de datos] (Check 3)
        // Mismos parámetros que la lectura, más el contenido a escribir.
        // Log obligatorio (pág. 24): "## JID: <JID> - <Lectura/Escritura> - Dir. Física: <DIRECCION_FISICA> - Tamaño: <TAMAÑO>"
        /*case LEER_DATO:{

            break;

        }
        case ESCRIBIR_DATO:{

            break;
        }

        */

        // [Enunciado pág. 23 - Obtención de marco] (Check 3)
        // La pide el Core durante la traducción de la MMU, con el JID y el número de página.
        // Se consulta la tabla de páginas del Job:
        //  - si la página está residente, se responde el número de marco y la página queda lockeada;
        //  - si no está residente, se responde que ocurrió un Page Fault.
        case OBTENER_MARCO:{
            break;
            
        }

        /*case RESPUESTA_MARCO:{
            break;
            
        }*/

        // PAGE_FAULT es una RESPUESTA de la Placa al Core (dentro de la obtención de marco), no un
        // pedido que el Core le mande a la Placa. Ver docs/protocolo.md, op_code 44.
        case PAGE_FAULT:{
            break;

        }
        // [Enunciado pág. 23 - Deslockeo de páginas] (Check 3)
        // El Core la envía al terminar con éxito cada ciclo de instrucción, o al devolverle el Job
        // al Planificador (por ejemplo, tras un Page Fault). Se recibe el JID y se liberan los
        // locks de TODAS sus páginas, para que sus marcos vuelvan a poder ser víctimas de reemplazo.
        case DESLOCKEAR_PAGINAS:{
            break;
            
        }      
        
        default:
            //log_warning(placa_logger, "Operación desconocida recibida de CORE: %d",operacion_core);
            break;

        }

        eliminar_buffer(payload);

    }

    // saco al core de la lista y libero, UNA sola vez, acá afuera del while
    pthread_mutex_lock(&mutex_lista_core);
    list_remove_element(lista_core, core);
    pthread_mutex_unlock(&mutex_lista_core);

    liberar_conexion(&socket_cliente);
    //free(core->id_core);
    free(core);

    return NULL;

}

// [Enunciado pág. 22-23 - Operaciones]
// Pedidos que llegan desde el Planificador: creación y finalización de Job, asignación y liberación
// de espacio (ALLOC/FREE), carga de página tras un Page Fault, lectura y escritura de datos de las
// syscalls de memoria, y deslockeo de páginas al terminar esas syscalls.
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

        // mismo motivo que en atender_core: el payload se consume siempre, lo use o no el case
        t_buffer* payload;
        codigo_validacion = recibir_buffer(socket_cliente, &payload);

        if (codigo_validacion != CONEXION_OK) {
            log_error(placa_logger, "Error al recibir el payload del op_code %d del PLANIFICADOR", operacion_planificador);
            break;
        }

        switch (operacion_planificador)
        {
        case MENSAJE_PRUEBA:{
            log_info(placa_logger, "Recibido MENSAJE_PRUEBA del Planificador");

            t_mensaje_prueba* mensaje = mensaje_prueba_leer(payload);

            if (mensaje == NULL) {
                log_error(placa_logger, "MENSAJE_PRUEBA mal formado del Planificador");
                break;
            }

            mensaje_prueba_loguear(placa_logger, "MENSAJE_PRUEBA recibido del Planificador", mensaje);

            t_paquete* eco = mensaje_prueba_empaquetar(mensaje, MENSAJE_PRUEBA_ECO);
            int resultado = enviar_paquete(socket_cliente, eco);
            eliminar_paquete(eco);

            if (resultado != CONEXION_OK) {
                log_error(placa_logger, "No se pudo responder el eco al Planificador");
            }

            mensaje_prueba_destruir(mensaje);
            break;

        }
        // [Enunciado pág. 22 - Creación de Job]
        // Se recibe un JID y el nombre de un archivo de pseudocódigo, relativo al path base del
        // config (PATH_INSTRUCCIONES). Con esos datos se crean las estructuras administrativas del Job.
        // Paginación bajo demanda (pág. 21): el Job arranca SIN marcos asignados.
        // Respuesta: CREAR_JOB_OK, o CREAR_JOB_ERROR si no se pudo abrir el archivo (SIS-43).
        // Log obligatorio (pág. 24): "## JID: <JID> - Job Creado"
        case CREAR_JOB:{

            uint32_t jid = buffer_read_uint32(payload);
            char* archivo = buffer_read_string(payload);

            t_job_placa* job = inicializar_job(jid, archivo);
            
            free(archivo);

            t_paquete* respuesta;
            if(job != NULL){
                log_info(placa_logger, "## JID: %u - Job Creado",jid);
                respuesta = crear_paquete(CREAR_JOB_OK);

            }else {
                respuesta = crear_paquete(CREAR_JOB_ERROR);
            }

            retardo_memoria();

            if(enviar_paquete(socket_cliente, respuesta) != CONEXION_OK){
                log_error(placa_logger, "No se pudo responder CREAR_JOB al Planificador");
            }

            eliminar_paquete(respuesta);


            break;

        }

        // [Enunciado pág. 22 - Asignación de espacio] (Check 3)
        // Se recibe el JID y el tamaño en bytes; se responde la dirección lógica donde empieza el
        // espacio asignado. Cada asignación arranca al inicio de una página. Hay que reservar en
        // el Offload el lugar para respaldar esas páginas: si no alcanza, la operación falla y se
        // le avisa al Planificador para que finalice el Job.
        // [Enunciado pág. 22 - Liberación de espacio] (Check 3)
        // Se recibe el JID y la dirección lógica donde empieza un espacio asignado antes. Se liberan
        // los marcos de sus páginas y el lugar que tenían reservado en el Offload.
        // Log obligatorio (pág. 24): "## JID: <JID> - <Asignación/Liberación> - Dirección Lógica: <DIRECCIÓN_LÓGICA> - Tamaño: <TAMAÑO>"

        // [Enunciado pág. 23 - Lectura / Escritura de datos] (Check 3)
        // Las usa el Planificador en las syscalls de memoria (LOAD_BATCH, LABEL, REPORT y checkpoints).
        /*case LEER_DATO:{
            break;

        }
        case ESCRIBIR_DATO:{
            break;

        }

        */

        // [Enunciado pág. 23 - Finalización de Job]
        // Solo se recibe un JID. Se liberan todos los marcos que tuviera ocupados, el lugar
        // reservado en el Offload y todas sus estructuras asociadas.
        // En el Check 2 todavía no hay marcos ni Offload: solo se liberan las estructuras (SIS-43).
        // Respuesta: FINALIZAR_JOB_OK siempre, aunque el JID no exista (finalizar es idempotente).
        case FINALIZAR_JOB:{
            
            uint32_t jid = buffer_read_uint32(payload);

            finalizar_job(jid);
            
            retardo_memoria();

            t_paquete* respuesta = crear_paquete(FINALIZAR_JOB_OK);
            int validar = enviar_paquete(socket_cliente, respuesta);

            if(validar != CONEXION_OK){
                log_error(placa_logger, "No se pudo responder FINALIZAR_JOB al Planificador");
            }

            eliminar_paquete(respuesta);

            break;
            
        }
        // [Enunciado pág. 23 - Carga de página] (Check 3)
        // La pide el Planificador cuando un Core informa un Page Fault. Se recibe el JID y el número
        // de página, y se carga en la memoria de la Placa. Si no hay marcos libres, hay que elegir
        // una víctima con el algoritmo de reemplazo (pág. 22).
        // Logs obligatorios (pág. 24):
        //   "## JID: <JID> - Subida a Memoria - Página: <NRO_PÁGINA> - Marco: <NRO_MARCO>"
        //   "## JID: <JID> - Bajada al Offload - Página: <NRO_PÁGINA>"
        //   "## Reemplazo - Marco: <NRO_MARCO> - JID Víctima: <JID> - Página Víctima: <NRO_PÁGINA>"
        case CARGAR_PAGINA:{
            break;
            
        }
        default:
            log_warning(placa_logger, "Operación desconocida recibida del PLANIFICADOR: %d", operacion_planificador);
            break;
        }

        eliminar_buffer(payload);
    }

    liberar_conexion(&socket_cliente);

    return EXIT_SUCCESS;
    
}

// [Enunciado pág. 24 - Archivo de Configuración: RETARDO_MEMORIA]
// Tiempo en milisegundos que se debe esperar para dar una respuesta. usleep trabaja en
// microsegundos, por eso el * 1000.
void retardo_memoria(){
    usleep(RETARDO_MEMORIA*1000);
}


t_job_placa* buscar_job(uint32_t jid_buscado){

    bool el_jid(void* elemento){
        t_job_placa* job = (t_job_placa*) elemento;
        return job->jid == jid_buscado;
    }

    t_job_placa* resultado =list_find(lista_jobs,el_jid);

    return resultado;

}


// [Enunciado pág. 21 - Memoria de Instrucciones]
// Los archivos de pseudocódigo son archivos de texto con una instrucción por línea, separadas
// por '\n'. La memoria de instrucciones es ilimitada (a diferencia de la memoria de usuario),
// así que se lee el archivo entero una sola vez, al crear el Job.
//Devuelve las lineas del archivo o null si no se pudo abrir
char** leer_instrucciones(char* path, uint32_t* cantidad){

    FILE* archivo = fopen(path, "r");

    if(archivo ==NULL) return NULL;

    char** instrucciones = string_array_new();
    *cantidad = 0;

    char* linea = NULL;
    size_t capacidad = 0;

    while(getline(&linea, &capacidad, archivo) != -1){

        linea[strcspn(linea, "\r\n")] = '\0';

        if(strlen(linea) == 0) continue;

        string_array_push(&instrucciones,strdup(linea));
        (*cantidad)++;

    }

    free(linea);
    fclose(archivo);
    return instrucciones;

}

t_job_placa* crear_job(uint32_t jid, char** instrucciones, uint32_t cantidad){
    t_job_placa* job = malloc(sizeof(t_job_placa));
    job->jid = jid;
    job->instrucciones = instrucciones;
    job->cant_instrucciones = cantidad;

    job->tabla_paginas = list_create();
    job->asignaciones = list_create();

    return job;

}

// [Enunciado pág. 21 - Memoria de Instrucciones] + [pág. 22 - Creación de Job]
// Cada grupo implementa una estructura que asocie cada Job con su archivo: acá es t_job_placa
// guardado en lista_jobs. El nombre del archivo es relativo a PATH_INSTRUCCIONES.
t_job_placa* inicializar_job(uint32_t jid, char* nombre_archivo){
    char* path = string_from_format("%s%s",PATH_INSTRUCCIONES ,nombre_archivo);

    uint32_t cantidad;
    char** instrucciones = leer_instrucciones(path,&cantidad);

    if(instrucciones == NULL){
        log_error(placa_logger, "No se pudo abrir el archivo de instrucciones: %s", path);
        free(path);
        return NULL;
    }

    free(path);

    t_job_placa* job = crear_job(jid,instrucciones, cantidad);

    pthread_mutex_lock(&mutex_jobs);
        list_add(lista_jobs,job);
    pthread_mutex_unlock(&mutex_jobs);

    return job;

}

t_job_placa* quitar_job(uint32_t jid_buscado){

    bool el_jid(void* elemento){
        
        t_job_placa* job = (t_job_placa*) elemento;

        return job->jid == jid_buscado; 
    }

    return list_remove_by_condition(lista_jobs,el_jid);

}

// [Enunciado pág. 23 - Finalización de Job]
// Se liberan todas las estructuras asociadas al Job. En el Check 3 acá se suman la liberación de
// sus marcos y del lugar reservado en el Offload.
void finalizar_job(uint32_t jid){

    pthread_mutex_lock(&mutex_jobs);
        t_job_placa* job = quitar_job(jid);
    pthread_mutex_unlock(&mutex_jobs);

    if(job == NULL){
        log_warning(placa_logger,"FINALIZAR_JOB de un JID inexistente: %u ", jid);
        return;
    }

    string_array_destroy(job->instrucciones);
    list_destroy_and_destroy_elements(job->tabla_paginas,free);
    list_destroy_and_destroy_elements(job->asignaciones, free);
    free(job);

    log_debug(placa_logger, "## JID: %u - Job Finalizado", jid);

}
#include "memoria.h"
#include "jobs.h" //buscar_job
#include "offload.h"

//memoria fisica
void* memoria = NULL;
int cant_marcos = 0;

//array de cant_marcos
t_marco* marcos = NULL;

pthread_mutex_t mutex_memoria = PTHREAD_MUTEX_INITIALIZER;

//Reloj logico para LRU, cada acceso a una pagina le suma 1 y la pagina guarda ese valor
static uint64_t contador_accesos = 0;


bool inicializar_memoria(){
    memoria = calloc(1, TAM_MEMORIA); //comienzan en 0
    cant_marcos = TAM_MEMORIA/TAM_PAGINA;
    marcos = calloc(cant_marcos,sizeof(t_marco)); //marcos en false

    if(memoria == NULL || marcos == NULL){
        log_error(placa_logger, "No se pudo reservar la memoria de usuario (%d bytes)", TAM_MEMORIA);
        return false;
    }

    log_debug(placa_logger, "Memoria de usuario: %d bytes, %d marcos de %d bytes",TAM_MEMORIA,cant_marcos,TAM_PAGINA);
    return true;
}

int marcos_libres(){
    int libres =0;
    for(int i = 0; i< cant_marcos; i++)
        if(!marcos[i].ocupado) libres++;

    return libres;


}

// [Enunciado pág. 24 - Archivo de Configuración: RETARDO_MEMORIA]
// Tiempo en milisegundos que se debe esperar para dar una respuesta. usleep trabaja en
// microsegundos, por eso el * 1000.
void retardo_memoria(){
    usleep(RETARDO_MEMORIA*1000);
}

// [Enunciado pág. 22 - Reemplazo de páginas]
// Anota un acceso a la página para que LRU y CLOK-M puedan elegir victima

void registrar_acceso(t_entrada_pagina* entrada){
    //avanza el reloj logico, este acceso es el mas de todos
    contador_accesos++;

    //LRU, esta pagina se uso en el instante contador_acceso
    entrada->ultimo_acceso=contador_accesos;

    //CLOCK-M bit de uso en 1, el algoritmo lo vuevleve a 0 cuando pasa el puntero
    entrada->uso = true;
}


//la pagina cae dentor de alguna asignación (alloc) del job ?, se llama mutex_jobs

static bool pagina_asignada(t_job_placa* job, uint32_t nro_pagina){

    //funcion interna
    bool contiene_pagina(void* elemento){
        t_asignacion* asignacion = elemento;
        return nro_pagina >= asignacion->pagina_inicio                               // desde la primera
            && nro_pagina <  asignacion->pagina_inicio + asignacion->cant_paginas;   // hasta la última
    }

    return list_any_satisfy(job->asignaciones, contiene_pagina);
}

// [Enunciado pág. 23 - Obtención de marco]
// La pide la MMU del Core. Presente -> devuelve el marco y la página queda lockeada.
// No presente -> PAGE_FAULT sin tocar nada de la tabla (criterio 2 de SIS-59).
uint8_t obtener_marco(uint32_t jid, uint32_t nro_pagina, uint32_t* marco, int* motivo){

    uint8_t respuesta;

    pthread_mutex_lock(&mutex_jobs); //nadie finalice el job mientras se usa

    pthread_mutex_lock(&mutex_memoria); // nadie toque entradas ni marcos

        t_job_placa* job = buscar_job(jid);

        if(job == NULL){
            //job no existe
            *motivo = MARCO_ERROR_JID_INEXISTENTE;
            respuesta = OBTENER_MARCO_ERROR;
        
        }else if(!pagina_asignada(job,nro_pagina) || nro_pagina >= (uint32_t)list_size(job->tabla_paginas) ){
            //la pagina no esta en ningun alloc del job
            // el segundo chequeo es por seguridad, que no se pase al final de la tabla

            *motivo = MARCO_ERROR_PAGINA_NO_ASIGNADA;
            respuesta = OBTENER_MARCO_ERROR;

        }else {
            // indice de la pagina = numero de pagina
            t_entrada_pagina* entrada = list_get(job->tabla_paginas, nro_pagina);

            if(!entrada->presente){
                //es el job pero no esta en memoria, no se toca nada
                respuesta = PAGE_FAULT;
            } else {
                
                entrada->lockeada = true;
                //ya no puede ser victima de reemplazo

                registrar_acceso(entrada);
                //uso + ultimo acceso

                *marco = entrada->marco;
                //el marco vuelve por el puntero

                respuesta = RESPUESTA_MARCO;

            }
        }

    pthread_mutex_unlock(&mutex_memoria); 
    
    pthread_mutex_unlock(&mutex_jobs);

    return respuesta;




}

// [Enunciado pág. 23 - Deslockeo de páginas]
// Saca el lock de TODAS las páginas del Job: sus marcos vuelven a poder ser víctimas de reemplazo.
// Llamarla dos veces no hace nada malo (idempotente).
bool deslockear_paginas(uint32_t jid){

    pthread_mutex_lock(&mutex_jobs);   
    pthread_mutex_lock(&mutex_memoria);

        t_job_placa* job = buscar_job(jid);

        // se guarda ANTES de soltar los mutex
        bool existe = (job != NULL);

        if(existe){

            // función interna: le saca el lock a una entrada
            void sacar_lock(void* elemento){
                t_entrada_pagina* entrada = elemento;
                entrada->lockeada = false;
            }
            // la aplica a todas las páginas del Job
            list_iterate(job->tabla_paginas, sacar_lock);
        }

    pthread_mutex_unlock(&mutex_memoria);
    pthread_mutex_unlock(&mutex_jobs);

    return existe;
    
}


//revisar que el pedido lectura/escritura sea valido y devuelve la entrada de la pagina
static t_entrada_pagina* validar_acceso(uint32_t jid, uint32_t dir_fisica, uint32_t tamanio, int* motivo){

    //tiene que haber algo que copiar 
    //y la dirección tiene que existir (de 0 a Tam_memoria-1)
    if(tamanio == 0 || dir_fisica >= (uint32_t) TAM_MEMORIA){
        *motivo = MEMORIA_ERROR_FUERA_DE_RANGO;
        return NULL;
    }

    uint32_t nro_marco = dir_fisica/TAM_PAGINA;
    //200/64 = marco 3

    uint32_t desplazamiento = dir_fisica % TAM_PAGINA;
    //200%64 = 8, 8 bytes dentro del marco

    //no puede pasarse del final del marco
    //desde el byte 8 quedan 64-8 = 56 bytes

    if(tamanio > (uint32_t) TAM_PAGINA - desplazamiento){
        *motivo = MEMORIA_ERROR_CRUZA_MARCO;
        return NULL;
    }

    // el marco tiene que estar ocupado y por este job
    t_marco* marco = &marcos[nro_marco];
    if(!marco->ocupado || marco->jid != jid){
        *motivo = MEMORIA_ERROR_MARCO_AJENO;
        return NULL;
    }

    //la pagina que dice el marco tiene que estar en la tabla del job
    // presente en ese marco
    t_job_placa* job = buscar_job(jid);
    if(job == NULL || marco->nro_pagina >= (uint32_t) list_size(job->tabla_paginas)){
        *motivo = MEMORIA_ERROR_MARCO_AJENO;    
        return NULL;
    }

    t_entrada_pagina* entrada = list_get(job->tabla_paginas, marco->nro_pagina);

    if(!entrada->presente || entrada->marco !=nro_marco){
        *motivo = MEMORIA_ERROR_MARCO_AJENO;
        return NULL;
    }

    return entrada;

}

// [Enunciado pág. 23 - Escritura de datos]
// Log obligatorio (pág. 24): "## JID: <JID> - Escritura - Dir. Física: <DIRECCION_FISICA> - Tamaño: <TAMAÑO>"
bool escribir_memoria(uint32_t jid, uint32_t dir_fisica, void* contenido, uint32_t tamanio, int* motivo){

    bool escribio= false;

    pthread_mutex_lock(&mutex_jobs);
    pthread_mutex_lock(&mutex_memoria);

        t_entrada_pagina* entrada = validar_acceso(jid,dir_fisica,tamanio,motivo);

        if(entrada!= NULL){
            //copia los bytes a la RAM
            //para poder sumarle de a un byte

            memcpy((char*) memoria + dir_fisica, contenido, tamanio);

            registrar_acceso(entrada);

            entrada->modificado = true; 

            escribio=true;
        } 

    pthread_mutex_unlock(&mutex_memoria);
    pthread_mutex_unlock(&mutex_jobs);


    if(escribio)

        log_info(placa_logger, "## JID: %u - Escritura - Dir. Física: %u - Tamaño: %u", jid, dir_fisica,tamanio);

    return escribio;

}

// [Enunciado pág. 23 - Lectura de datos]
// Log obligatorio (pág. 24): "## JID: <JID> - Lectura - Dir. Física: <DIRECCION_FISICA> - Tamaño: <TAMAÑO>"
void* leer_memoria(uint32_t jid, uint32_t dir_fisica, uint32_t tamanio, int* motivo){

    void* copia = NULL;

    pthread_mutex_lock(&mutex_jobs);
    pthread_mutex_lock(&mutex_memoria);

        t_entrada_pagina* entrada = validar_acceso(jid, dir_fisica, tamanio, motivo);

        if(entrada != NULL){
            // recién acá se reserva: ya se sabe que tamanio entra en un marco (como mucho 64)
            copia = malloc(tamanio);

            // copia desde la RAM hacia la copia nueva (al revés que en escribir_memoria)
            memcpy(copia, (char*) memoria + dir_fisica, tamanio);

            // leer es "usar" la página, pero NO la modifica
            registrar_acceso(entrada);
        }

    pthread_mutex_unlock(&mutex_memoria);
    pthread_mutex_unlock(&mutex_jobs);

    if(copia!= NULL )
        log_info(placa_logger, "## JID: %u - Lectura - Dir. Física: %u - Tamaño: %u", jid, dir_fisica, tamanio);

    return copia;

}

// Busca "cantidad" páginas libres seguidas en la tabla del Job
static uint32_t buscar_hueco(t_job_placa* job, uint32_t cantidad){

    uint32_t total = list_size(job->tabla_paginas);
    uint32_t libres_seguidas = 0;

    for (uint32_t i = 0; i < total; i++){

        t_entrada_pagina* entrada = list_get(job->tabla_paginas, i);

        if(entrada->slot_offload == SLOT_SIN_ASIGNAR){
            libres_seguidas++;

            if(libres_seguidas == cantidad){
                return i - cantidad + 1;
                //alcanzo el hueco, el hueco empieza aca
            }
        }else { 
            libres_seguidas = 0;
        }
    }

    // no hay hueco donde entre al final
    return total;

}

static void preparar_paginas(t_job_placa* job, uint32_t pagina_inicio, uint32_t cantidad, uint32_t* slots){

    for(uint32_t i = 0; i < cantidad; i++){

        uint32_t nro_pagina = pagina_inicio + i;

        if(nro_pagina < (uint32_t) list_size(job->tabla_paginas)){
            
            t_entrada_pagina* entrada = list_get(job->tabla_paginas,nro_pagina);

            memset(entrada, 0, sizeof(t_entrada_pagina));

            entrada->slot_offload = slots[i];

        }else{
            t_entrada_pagina* entrada = calloc(1, sizeof(t_entrada_pagina));

            entrada->slot_offload = slots[i];

            list_add(job->tabla_paginas,entrada);
        }

    }

}

static void registrar_asignacion (t_job_placa* job, uint32_t inicio, uint32_t cantidad, uint32_t tamanio){

    t_asignacion* asignacion = malloc(sizeof(t_asignacion));
    asignacion->pagina_inicio = inicio;
    asignacion->cant_paginas = cantidad;
    asignacion->tamanio = tamanio;
    list_add(job->asignaciones,asignacion);
}

static bool asignar_en_job(uint32_t jid, uint32_t cant_paginas, uint32_t tamanio, uint32_t* slots, uint32_t* dir_logica){

    t_job_placa* job = buscar_job(jid);

    if(job == NULL){
        log_error(placa_logger, "ALLOC de un JID inexistente: %u", jid);
        return false;
    }else if(!reservar_slots(cant_paginas,slots)){ 
        log_warning(placa_logger, "ALLOC sin lugar en el Offload - JID: %u - Páginas: %u", jid, cant_paginas);
        return false;
    }else {
        
        uint32_t pagina_inicio = buscar_hueco(job,cant_paginas);

        preparar_paginas(job,pagina_inicio,cant_paginas,slots);

        registrar_asignacion(job,pagina_inicio,cant_paginas,tamanio);

        *dir_logica = pagina_inicio * TAM_PAGINA;

        return true;

    }

}

// [Enunciado pág. 22 - Asignación de espacio]
// Agrega al Job las páginas para "tamanio" bytes (no presentes: bajo demanda), les reserva lugar en
// el Offload y devuelve en *dir_logica dónde empieza. Si no se puede, devuelve false sin tocar nada.
// Log obligatorio (pág. 24): "## JID: <JID> - Asignación - Dirección Lógica: <DIR> - Tamaño: <TAM>"
bool asignar_espacio(uint32_t jid, uint32_t tamanio, uint32_t* dir_logica){

    if(tamanio == 0){
        log_error(placa_logger, "ALLOC de 0 bytes - JID %u", jid);
        return false;
    }
    //paginas necesarias, redondeado para arriba

    uint32_t cant_paginas = tamanio / TAM_PAGINA + (tamanio % TAM_PAGINA !=0);

    if(cant_paginas > cant_slots){ 
        log_error(placa_logger, "ALLOC más grande que el Offload - JID: %u - Tamaño: %u", jid, tamanio);
        return false;
    }

    //donde reservar_slots deja los numeros de slots
    uint32_t* slots = malloc(cant_paginas* sizeof(uint32_t));

    pthread_mutex_lock(&mutex_jobs);
    pthread_mutex_lock(&mutex_memoria);

        bool asignado = asignar_en_job(jid,cant_paginas,tamanio,slots,dir_logica);

    pthread_mutex_unlock(&mutex_memoria);
    pthread_mutex_unlock(&mutex_jobs);

    free(slots);

    if(asignado)
        log_info(placa_logger, "## JID: %u - Asignación - Dirección Lógica: %u - Tamaño: %u", jid, *dir_logica, tamanio);

    return asignado;

}


//Liberar cantidad de paginas desde inicio 
static void liberar_paginas(t_job_placa* job, uint32_t inicio, uint32_t cantidad){

    for(uint32_t i = 0; i<cantidad; i++ ){
        t_entrada_pagina* entrada = list_get(job->tabla_paginas, inicio + i);
    
        if(entrada->presente){
            marcos[entrada->marco].ocupado=false;
        }

        liberar_slot(entrada->slot_offload);

        memset(entrada, 0, sizeof(t_entrada_pagina));
        entrada->slot_offload = SLOT_SIN_ASIGNAR;

    }

}

static bool liberar_en_job(uint32_t jid, uint32_t dir_logica, uint32_t* tamanio){

    t_job_placa* job = buscar_job(jid);

    if(job == NULL){
        log_warning(placa_logger, "FREE de un JID inexistente: %u",jid);

        return false;
    }

    bool empieza_ahi(void* elemento){
        t_asignacion* asignacion = elemento;
        return asignacion->pagina_inicio * TAM_PAGINA == dir_logica;
    }

    t_asignacion* asignacion = list_find(job->asignaciones, empieza_ahi);

    if(asignacion == NULL){

        log_warning(placa_logger, "FREE de una dirección que no es inicio de ninguna asignación - JID: %u - Dirección Lógica: %u", jid, dir_logica);
        return false;
    }

    liberar_paginas(job,asignacion->pagina_inicio, asignacion->cant_paginas);

    *tamanio = asignacion->tamanio;

    list_remove_element(job->asignaciones, asignacion);

    free(asignacion);

    return true;

}

// [Enunciado pág. 22 - Liberación de espacio]
// Siempre se responde FREE_OK, aunque no haya nada que liberar (D4).
// Log obligatorio (pág. 24): "## JID: <JID> - Liberación - Dirección Lógica: <DIR> - Tamaño: <TAM>"

void liberar_espacio(uint32_t jid, uint32_t dir_logica){

    uint32_t tamanio = 0;

    pthread_mutex_lock(&mutex_jobs);
    pthread_mutex_lock(&mutex_memoria);

        bool liberado = liberar_en_job(jid, dir_logica, &tamanio);

    pthread_mutex_unlock(&mutex_memoria);
    pthread_mutex_unlock(&mutex_jobs);

    if(liberado)
        log_info(placa_logger, "## JID: %u - Liberación - Dirección Lógica: %u - Tamaño: %u", jid, dir_logica, tamanio);


}


// [Enunciado pág. 23 - Carga de página] / [pág. 22 - Reemplazo]

//devuelve el primer marco libre o -1, si estan todos ocupados

static int buscar_marco_libre(){
    for(int i = 0; i < cant_marcos; i++ )
        if(!marcos[i].ocupado) return i;

    return -1;
}

//devuelve la entrada de la pagina que ocupa el marco,
// o null si el marco esta libre o la tabla no coincide con el marco

static t_entrada_pagina* entrada_de_marco(int nro_marco){

    t_marco* marco = &marcos[nro_marco];

    if(!marco->ocupado) return NULL;

    t_job_placa* job = buscar_job(marco->jid);

    if(job==NULL || marco->nro_pagina >= (uint32_t) list_size(job->tabla_paginas))
        return NULL;

    t_entrada_pagina* entrada = list_get(job->tabla_paginas, marco->nro_pagina);

    //la entrada tiene que coincidir con el marco presente y en este marco
    if(!entrada->presente || entrada->marco != (uint32_t) nro_marco)
        return NULL;

    return entrada;

}


static int elegir_victima(){
    for(int i=0; i < cant_marcos; i++){
        t_entrada_pagina* entrada = entrada_de_marco(i);
        //NULL si el marco esta libre o no coincide
        if(entrada != NULL && !entrada->lockeada)
            return i;
        //primera pagina no lockeada, victima
    }

    return -1;
}


static bool preparar_carga(uint32_t jid, uint32_t nro_pagina, t_carga* carga){

    t_job_placa* job = buscar_job(jid);

    if(job == NULL){
        log_warning(placa_logger, "CARGAR_PAGINA de un JID inexistente: %u", jid);
        return false;
    }

    if(!pagina_asignada(job,nro_pagina) || nro_pagina >= (uint32_t) list_size(job->tabla_paginas)){
        log_warning(placa_logger, "CARGAR_PAGINA de una página no asignada - JID: %u - Página: %u", jid, nro_pagina);
        return false;
    }

    t_entrada_pagina* entrada = list_get(job->tabla_paginas, nro_pagina);

    if(entrada->presente)
        return false;
    
    //setear todo los valores en 0 y solo asignar ese valor
    *carga = (t_carga) { .hay_victima=false };

    carga->marco = buscar_marco_libre();

    if(carga->marco == -1){
        carga->marco = elegir_victima();

        if(carga->marco == -1){
            log_error(placa_logger, "CARGAR_PAGINA sin víctima posible: todas las páginas en memoria están lockeadas - JID: %u - Página: %u", jid, nro_pagina);
            return false;
        }


        t_entrada_pagina* victima = entrada_de_marco(carga->marco);

        carga->hay_victima = true;
        carga->modificada_victima = victima->modificado;

        carga->victima_jid = marcos[carga->marco].jid;
        carga->victima_pagina = marcos[carga->marco].nro_pagina;

        carga->victima_slot = victima->slot_offload;

        victima->presente = false; //la victima ya no esta en RAM

        log_info(placa_logger, "## Reemplazo - Marco: %d - JID Víctima: %u - Página Víctima: %u",carga->marco, carga->victima_jid, carga->victima_pagina);

    }


    marcos[carga->marco] = (t_marco){
        .ocupado =true, 
        .jid = jid,
        .nro_pagina = nro_pagina };
    
    carga->slot = entrada->slot_offload;
    carga->leer_del_offload = entrada->en_offload;

    return true;

}

//bajar la victima (si estaba modificada) y trae la pagina nueva al marco

static void mover_paginas(t_carga* carga){

    void* marco = (char*) memoria + carga->marco *TAM_PAGINA;

    if(carga->hay_victima && carga->modificada_victima){
        escribir_pagina_offload(carga->victima_slot,marco);

        log_info(placa_logger, "## JID: %u - Bajada al Offload - Página: %u", carga->victima_jid, carga->victima_pagina);
    }

    if(carga->leer_del_offload)
        leer_pagina_offload(carga->slot,marco);
    else
        memset(marco,0,TAM_PAGINA);

}

static void terminar_carga(uint32_t jid, uint32_t nro_pagina, t_carga* carga){

    t_job_placa* job = buscar_job(jid);

    if(job == NULL) return;

    t_entrada_pagina* entrada = list_get(job->tabla_paginas,nro_pagina);

    entrada->presente = true;
    entrada->marco = carga->marco;
    entrada->modificado = false;

    registrar_acceso(entrada);

    if(carga->hay_victima && carga->modificada_victima){
        t_job_placa* job_victima = buscar_job(carga->victima_jid);

        if(job_victima !=NULL){
            t_entrada_pagina* victima = list_get(job_victima->tabla_paginas, carga->victima_pagina);
            
            victima->en_offload = true;
            victima->modificado = false;
        }

    }

}

// [Enunciado pág. 23 - Carga de página]
// La pide el Planificador tras un Page Fault. Deja la página en un marco (libre, o el de una víctima).
// Siempre se responde CARGAR_PAGINA_OK: los casos raros quedan en el log (decisión del grupo).
// Logs obligatorios (pág. 24), en este orden: Reemplazo, Bajada al Offload, Subida a Memoria.

void cargar_pagina(uint32_t jid, uint32_t nro_pagina){

    t_carga carga;

    // FASE 1: decidir y reservar
    pthread_mutex_lock(&mutex_jobs);
    pthread_mutex_lock(&mutex_memoria);
        bool hay_que_cargar = preparar_carga(jid, nro_pagina, &carga);
    pthread_mutex_unlock(&mutex_memoria);
    pthread_mutex_unlock(&mutex_jobs);

    if(!hay_que_cargar) return;

    //FASE 2: el disco, sin bloquear a los Cores
    mover_paginas(&carga);

    //FASE 3: dejarla presente
    pthread_mutex_lock(&mutex_jobs);
    pthread_mutex_lock(&mutex_memoria);
        terminar_carga(jid, nro_pagina, &carga);
    pthread_mutex_unlock(&mutex_memoria);
    pthread_mutex_unlock(&mutex_jobs);

    log_info(placa_logger, "## JID: %u - Subida a Memoria - Página: %u - Marco: %d", jid, nro_pagina, carga.marco);

}


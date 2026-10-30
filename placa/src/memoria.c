#include "memoria.h"
#include "jobs.h" //buscar_job

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





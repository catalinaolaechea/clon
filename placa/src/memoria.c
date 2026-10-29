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








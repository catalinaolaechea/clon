#include "jobs.h"

t_list* lista_jobs;

pthread_mutex_t mutex_jobs = PTHREAD_MUTEX_INITIALIZER;

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
static char** leer_instrucciones(char* path, uint32_t* cantidad){

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

static t_job_placa* crear_job(uint32_t jid, char** instrucciones, uint32_t cantidad){
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

static t_job_placa* quitar_job(uint32_t jid_buscado){

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

// [Enunciado pág. 16 - Core / Fetch] + [pág. 21 - Memoria de Instrucciones]
// El PC es el número de instrucción relativo al Job (PC = 0 es la primera línea).
// Si el JID no existe o el PC se pasa del final no hay instrucción: se devuelve NULL con el
// motivo (SIS-40). La usa el FETCH_INSTRUCCION del Core.
char* obtener_instruccion(uint32_t jid, uint32_t pc, int* motivo_error){

    char* instruccion = NULL;

    pthread_mutex_lock(&mutex_jobs);

        t_job_placa* job = buscar_job(jid);

        if(job == NULL){
            *motivo_error = FETCH_ERROR_JID_INEXISTENTE;
        } else if (pc >= job->cant_instrucciones){
            *motivo_error = FETCH_ERROR_PC_FUERA_DE_RANGO;
        } else {
            instruccion = strdup(job->instrucciones[pc]);
        }

    pthread_mutex_unlock(&mutex_jobs);

    return instruccion;

}

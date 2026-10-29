#include "memoria.h"

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

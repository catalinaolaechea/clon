#include "reemplazo.h"
#include "memoria.h"     

static t_algoritmo_reemplazo algoritmo;      
static int puntero_clock = 0;                

void inicializar_reemplazo(){
    algoritmo = (strcmp(ALGORITMO_REEMPLAZO, "LRU") == 0) ? ALGORITMO_LRU : ALGORITMO_CLOCK_M;
    log_debug(placa_logger, "Algoritmo de reemplazo: %s", ALGORITMO_REEMPLAZO);
}

// La página del marco puede ser víctima: está en RAM (no libre ni en tránsito) y no lockeada.
static t_entrada_pagina* candidata(int marco){
    t_entrada_pagina* entrada = entrada_de_marco(marco);
    return (entrada != NULL && !entrada->lockeada) ? entrada : NULL;
}

// LRU: la página usada hace más tiempo (menor ultimo_acceso), de cualquier Job.
static int victima_lru(){

    int victima = -1;
    uint64_t mas_viejo = UINT64_MAX;

    for(int i = 0; i < cant_marcos; i++){
        t_entrada_pagina* entrada = candidata(i);
        if(entrada != NULL && entrada->ultimo_acceso < mas_viejo){
            mas_viejo = entrada->ultimo_acceso;
            victima = i;
        }
    }

    return victima;                          
    // -1 si no hubo ninguna candidata (todas lockeadas)
}

//Clock Modificado: vuelta 1 busca (uso=0, mod=0) sin tocar bits; vuelta 2 busca
// (uso=0, mod=1) poniendo uso=0 en las que pasa; se repite. El puntero sigue donde quedó.
static int victima_clock_m(){

    //si no hay ninguna candidata, no se da ninguna vuelta
    bool hay_candidata = false;
    for(int i = 0; i < cant_marcos && !hay_candidata; i++)
        if(candidata(i) != NULL) hay_candidata = true;
    if(!hay_candidata) return -1;

    while(true){

        // vuelta 1: (uso = 0, modificado = 0), sin modificar nada
        for(int k = 0; k < cant_marcos; k++){
            int i = (puntero_clock + k) % cant_marcos;
            t_entrada_pagina* e = candidata(i);
            if(e != NULL && !e->uso && !e->modificado){
                puntero_clock = (i + 1) % cant_marcos;   // D6: queda en el siguiente
                return i;
            }
        }

        // vuelta 2: (uso = 0, modificado = 1), bajando el bit de uso de las que pasa
        for(int k = 0; k < cant_marcos; k++){
            int i = (puntero_clock + k) % cant_marcos;
            t_entrada_pagina* e = candidata(i);
            if(e == NULL) continue;
            if(!e->uso && e->modificado){
                puntero_clock = (i + 1) % cant_marcos;
                return i;
            }
            e->uso = false;
        }
        // si no encontró, todas las candidatas quedaron con uso = 0: la próxima vuelta 1 o 2 encuentra
    }
}

// [Enunciado pág. 22 - Reemplazo de páginas]
int elegir_victima(){
    switch(algoritmo){
    case ALGORITMO_LRU:     return victima_lru();
    case ALGORITMO_CLOCK_M: return victima_clock_m();
    }
    return -1;
}
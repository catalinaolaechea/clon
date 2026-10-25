#include "inicializador.h"


//variables globalbes
t_config* placa_config;
t_log* placa_logger;
char* archivo_config;

//variables de archiivo 
char* PUERTO_ESCUCHA;
t_log_level LOG_LEVEL;
int TAM_MEMORIA;
int TAM_PAGINA;
int RETARDO_MEMORIA;
char* ALGORITMO_REEMPLAZO;
char* PATH_INSTRUCCIONES;
char* PATH_OFFLOAD;
int TAM_OFFLOAD;
int RETARDO_OFFLOAD;

//listas
t_list* lista_core = NULL;
t_list* lista_jobs;


//memoria fisica
void* memoria = NULL;
int cant_marcos = 0;

//array de cant_marcos
t_marco* marcos = NULL;

//offload
int archivo_offload;
t_bitarray* slots_offload;


//semaforos
pthread_mutex_t mutex_lista_core = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_jobs = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_memoria = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_offload = PTHREAD_MUTEX_INITIALIZER;



void inicializar_log(){
    //placa_logger = log_create("placa.log","LOGGER_PLACA",true,LOG_LEVEL_TRACE);
    placa_logger = iniciar_logger("placa.log","LOGGER_PLACA",LOG_LEVEL);

}

void inicializar_config(){

    placa_config = iniciar_config(archivo_config);

    PUERTO_ESCUCHA = so_config_get_string(placa_config, "PLACA","PUERTO_ESCUCHA");
    LOG_LEVEL = log_level_from_string(so_config_get_string(placa_config, "PLACA","LOG_LEVEL"));
    TAM_MEMORIA = so_config_get_int(placa_config, "PLACA","TAM_MEMORIA");
    TAM_PAGINA = so_config_get_int(placa_config, "PLACA","TAM_PAGINA");
    RETARDO_MEMORIA = so_config_get_int(placa_config, "PLACA","RETARDO_MEMORIA");
    ALGORITMO_REEMPLAZO = so_config_get_string(placa_config, "PLACA","ALGORITMO_REEMPLAZO");
    PATH_INSTRUCCIONES = so_config_get_string(placa_config, "PLACA","PATH_INSTRUCCIONES");
    PATH_OFFLOAD = so_config_get_string(placa_config, "PLACA","PATH_OFFLOAD");
    TAM_OFFLOAD = so_config_get_int(placa_config, "PLACA","TAM_OFFLOAD");
    RETARDO_OFFLOAD = so_config_get_int(placa_config, "PLACA","RETARDO_OFFLOAD");

}

static bool es_potencia_de_2(int n){
    return n > 0 && (n & (n -1 )) == 0;
}


bool validar_config(){

    bool ok = true;

    if(!es_potencia_de_2(TAM_MEMORIA)){
        log_error(placa_logger, "TAM_MEMORIA=%d invalido: debe ser potencia de 2", TAM_MEMORIA);
        ok = false;
    }

    if(!es_potencia_de_2(TAM_PAGINA)){
        log_error(placa_logger, "TAM_PAGINA=%d invalido: debe ser potencia de 2", TAM_PAGINA);
        ok = false;
    }
    if(ok && TAM_PAGINA > TAM_MEMORIA){
        log_error(placa_logger, "TAM_PAGINA=%d no puede ser mayor que TAM_MEMORIA=%d", TAM_PAGINA,TAM_MEMORIA);
        ok = false;
    }

    return ok;

}

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
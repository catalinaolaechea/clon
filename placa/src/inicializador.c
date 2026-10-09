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

void inicializar_log(){
    //placa_logger = log_create("placa.log","LOGGER_PLACA",true,LOG_LEVEL_TRACE);
    placa_logger = iniciar_logger("placa.log","LOGGER_PLACA",LOG_LEVEL);

}

void inicializar_config(){

    placa_config = config_create(archivo_config);

    PUERTO_ESCUCHA = config_get_string_value(placa_config,"PUERTO_ESCUCHA");
    LOG_LEVEL = log_level_from_string(config_get_string_value(placa_config,"LOG_LEVEL"));
    TAM_MEMORIA = config_get_int_value(placa_config,"TAM_MEMORIA");
    TAM_PAGINA = config_get_int_value(placa_config,"TAM_PAGINA");
    RETARDO_MEMORIA = config_get_int_value(placa_config,"RETARDO_MEMORIA");
    ALGORITMO_REEMPLAZO = config_get_string_value(placa_config,"ALGORITMO_REEMPLAZO");
    PATH_INSTRUCCIONES = config_get_string_value(placa_config,"PATH_INSTRUCCIONES");
    PATH_OFFLOAD = config_get_string_value(placa_config,"PATH_OFFLOAD");
    TAM_OFFLOAD = config_get_int_value(placa_config,"TAM_OFFLOAD");
    RETARDO_OFFLOAD = config_get_int_value(placa_config,"RETARDO_OFFLOAD");

}

void inicializar_memoria(){
    memoria = calloc(1, TAM_MEMORIA); //comienzan en 0
    cant_marcos = TAM_MEMORIA/TAM_PAGINA;
    marcos = calloc(cant_marcos,sizeof(t_marco)); //marcos en false

    log_debug(placa_logger, "Memoria de usuario: %d bytes, %d marcos de %d bytes",TAM_MEMORIA,cant_marcos,TAM_PAGINA);

}

int marcos_libres(){
    int libres =0;
    for(int i = 0; i< cant_marcos; i++)
        if(!marcos[i].ocupado) libres++;

    return libres;
    
    
}
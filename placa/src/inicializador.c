#include "inicializador.h"

//variables globalbes
t_config* placa_config;
t_log* placa_logger;
char* archivo_config;

//variables de archiivo 
char* PUERTO_ESCUCHA;
char* LOG_LEVEL;
int TAM_MEMORIA;
int TAM_PAGINA;
int RETARDO_MEMORIA;
char* ALGORITMO_REEMPLAZO;
char* PATH_INSTRUCCIONES;
char* PATH_OFFLOAD;
int TAM_OFFLOAD;
int RETARDO_OFFLOAD;

void inicializar_log(){
    placa_logger = log_create("placa.log","LOGGER_PLACA",true,LOG_LEVEL_TRACE);
}

void inicializar_config(){

    placa_config = config_create(archivo_config);

    PUERTO_ESCUCHA = config_get_string_value(placa_config,"PUERTO_ESCUCHA");
    LOG_LEVEL = config_get_string_value(placa_config,"LOG_LEVEL");
    TAM_MEMORIA = config_get_int_value(placa_config,"TAM_MEMORIA");
    TAM_PAGINA = config_get_int_value(placa_config,"TAM_PAGINA");
    RETARDO_MEMORIA = config_get_int_value(placa_config,"RETARDO_MEMORIA");
    ALGORITMO_REEMPLAZO = config_get_string_value(placa_config,"ALGORITMO_REEMPLAZO");
    PATH_INSTRUCCIONES = config_get_string_value(placa_config,"PATH_INSTRUCCIONES");
    PATH_OFFLOAD = config_get_string_value(placa_config,"PATH_OFFLOAD");
    TAM_OFFLOAD = config_get_int_value(placa_config,"TAM_OFFLOAD");
    RETARDO_OFFLOAD = config_get_int_value(placa_config,"RETARDO_OFFLOAD");

}
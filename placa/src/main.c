#include "inicializador.h"

int main(int argc, char* argv[]) {
    
    if(argc < 2){
        //printf("Uso: %s [archivo config]",argv[0]);
        exit(EXIT_FAILURE);
    }

    archivo_config = strdup(argv[1]);

    inicializar_log();
    inicializar_config();

    int server_placa = iniciar_servidor(PUERTO_ESCUCHA);

    log_info(placa_logger,"Servidor listo para recibir a los clientes");    

    int client_escucha = esperar_cliente(server_placa);

    while(1){
        int cod_op = recibir_operacion(client_escucha);

        switch (cod_op)
        {
        case MENSAJE_PRUEBA :{ 
            //t_list* lista;
            //lista = recibir_paquete(client_escucha);
            log_info(placa_logger, "Llegaron los datos");
            break;
        }
        case -1:
            log_error(placa_logger,"el cliente se desconecto");
            return EXIT_FAILURE;
            
        default:
            break;

        }
    }

    return EXIT_FAILURE;
}

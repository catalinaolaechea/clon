#include <utils/hello.h>

void saludar(char* quien) {
    printf("Hola desde %s!!\n", quien);
}

t_log* iniciar_logger(void)
{
	t_log* nuevo_logger = log_create("log_tp0.log","tp0",1,LOG_LEVEL_INFO);

	return nuevo_logger;
}

t_config* iniciar_config(void)
{
	t_config* nuevo_config = config_create("cliente.config");

	return nuevo_config;
}

void leer_consola(t_log* logger)
{
	char* leido;

	// La primera te la dejo de yapa
	leido = readline("> ");

	log_info(logger,"> %s",leido);

	// El resto, las vamos leyendo y logueando hasta recibir un string vacío
	while(strcmp(leido,"") !=0){
		leido = readline("> ");
		log_info(logger,">> %s",leido);
		free(leido);
	}

	// ¡No te olvides de liberar las lineas antes de regresar!

}

void paquete(int conexion)
{
	// Ahora toca lo divertido!
	char* leido;
	t_paquete* paquete = crear_paquete();

	// Leemos y esta vez agregamos las lineas al paquete
	leido = readline("> ");

	agregar_a_paquete(paquete,leido,(strlen(leido) + 1));

	while(strcmp(leido,"") !=0){
		leido = readline("> ");
		//log_info(logger,">> %s",leido);
		agregar_a_paquete(paquete,leido,(strlen(leido) + 1));

	}
	enviar_paquete(paquete,conexion);

	eliminar_paquete(paquete);

	// ¡No te olvides de liberar las líneas y el paquete antes de regresar!
	
	free(leido);
}

void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	/* Y por ultimo, hay que liberar lo que utilizamos (conexion, log y config) 
	  con las funciones de las commons y del TP mencionadas en el enunciado */
	log_destroy(logger);

	liberar_conexion(conexion);

	config_destroy(config);

}

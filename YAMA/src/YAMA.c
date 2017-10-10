#include "FuncionesYama.h"

int main(int argc, char *argv[]) {

	jobsAPlanificar = list_create();

	struct configuracionYama config;
	logger = log_create("logYama", "YAMA.c", 1, LOG_LEVEL_TRACE);
	//socketFs = conectarseConFs();
	cargarConfiguracionYama(&config,argv[1]);

	levantarServidorYama(config.YAMA_IP,config.YAMA_PUERTO);

	iniciarListasPlanificacion();

	return EXIT_SUCCESS;
}


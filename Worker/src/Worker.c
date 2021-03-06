#include "FuncionesWorker.h"
#include <sys/mman.h>
#include <fcntl.h>

struct configuracionNodo config;

int main(int argc, char *argv[]) {
	limpiarPantalla();
	logger = log_create("logWorker", "Worker.c", 1, LOG_LEVEL_TRACE);
	cargarConfiguracionNodo(&config,argv[1]);

	char* path = obtenerPathActual();
	char* pathTmp = string_from_format("%s/tmp", path);

	struct stat st = {0};

	if (stat(pathTmp, &st) == 0) {
		system("rm -r tmp");
	}

	mkdir(pathTmp, 0700);

	levantarServidorWorker(config.IP_NODO, config.PUERTO_WORKER);

	//t_list* a = list_create();
	//list_add(a,"j1n1b0e0");
	//list_add(a,"j1n1b18e0");
	//list_add(a,"j1n1b22e0");

	//apareo(a,"final");

	/*t_list* listaArchivosTemporales = list_create();
	list_add(listaArchivosTemporales, "/home/utnso/tp-2017-2c-PEQL/Worker/a.txt");
	list_add(listaArchivosTemporales, "/home/utnso/tp-2017-2c-PEQL/Worker/b.txt");
	list_add(listaArchivosTemporales, "/home/utnso/tp-2017-2c-PEQL/Worker/c.txt");
	apareoArchivosLocales(listaArchivosTemporales,"/home/utnso/tp-2017-2c-PEQL/Worker/resultado.txt");
	//Para probar reduccion
	t_list* listaArchivosTemporales = list_create();
	list_add(listaArchivosTemporales, "/home/utnso/tp-2017-2c-PEQL/Worker/Debug/tmp/j1n1b0e0");
	list_add(listaArchivosTemporales, "/home/utnso/tp-2017-2c-PEQL/Worker/Debug/tmp/j1n1b3e0");
	list_add(listaArchivosTemporales, "/home/utnso/tp-2017-2c-PEQL/Worker/Debug/tmp/j1n1b6e0");
	list_add(listaArchivosTemporales, "/home/utnso/tp-2017-2c-PEQL/Worker/Debug/tmp/j1n1b9e0");
	char *archivoPreReduccion = "preReduccion";
	char* destino = "destinoFinal";
	char *path = string_new();
	char cwd[1024];
	string_append(&path, getcwd(cwd, sizeof(cwd)));

	char* aux = string_from_format("%s/tmp/%s", path, archivoPreReduccion); // /home/utnso/tp-2017-2c-PEQL/Worker/Debug/tmp/preReduccion
	apareoArchivosLocales(listaArchivosTemporales, aux);
	char* command = string_from_format("cat %s | perl %s > %s", aux, string_from_format("../scripts/reductorLocal.pl"), string_from_format("%s/tmp/%s", path, destino));
	ejecutarComando(command, clientSocket);
	log_trace(logger, "Reduccion local realizada correctamente");*/


	return EXIT_SUCCESS;
}

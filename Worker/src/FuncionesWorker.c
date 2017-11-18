#include "FuncionesWorker.h"

void ejecutarComando(char * command, int socketAceptado) {
	int status;
	log_trace(logger, "COMANDO:%s\n", command);
	system(command);
	if ((status = system(command)) < 0) {
		log_error(logger, "NO SE PUDO EJECTUAR EL COMANDO EN SYSTEM, FALLA REDUCCION LOCAL");
		free(command);
		empaquetar(socketAceptado, mensajeError, 0, 0);
		exit(1);
	}
}

t_list* crearListaParaReducir() {
	//Aca hay que conectarse al worker capitan, mandar el contenido del archivo reducido y que ese lo aparee
	t_list* archivosAReducir = list_create();
	return archivosAReducir;
}

char *get_line(FILE *fp) {
	char *line = NULL;
	size_t len = 0;
	int r = getline(&line, &len, fp);
	return r != -1 ? line : NULL;
}

void traverse_nodes(t_list* list, void funcion(void*)) {

	t_link_element* next, *t_link_element = list->head;

	while (t_link_element != NULL) {
		next = t_link_element->next;
		funcion(t_link_element->data);
		t_link_element = next;
	}
}

void apareoArchivosLocales(t_list *sources, const char *target) {

	typedef struct {
		FILE *file;
		char *line;
	} t_cont;

	t_cont *map_cont(const char *source) {
		t_cont *cont = malloc(sizeof(cont));
		cont->file = fopen(source, "r");
		cont->line = NULL;
		return cont;
	}

	bool line_set(t_cont *cont) {
		return cont->line != NULL;
	}

	//TODO Corregir que pasa con el fin de archivo
	void read_file(t_cont *cont) {
		if (!line_set(cont)) {
			char *aux = get_line(cont->file);
			free(cont->line);
			cont->line = aux;
		}
	}

	bool compare_lines(t_cont *cont1, t_cont *cont2) {
		return strcmp(cont1->line, cont2->line) <= 0;
	}

	t_list* listaArchivos = list_map(sources, (void*) map_cont);
	FILE* resultado = fopen(target, "w+");

	t_list* list = list_create();
	list_add_all(list, listaArchivos);

	traverse_nodes(list, (void*) read_file);

	while (true) {

		traverse_nodes(list, (void*) read_file);
		t_list* aux = list_filter(list, (void*) line_set);

		list = aux;
		if (list_is_empty(list))
			break;
		list_sort(list, (void*) compare_lines);

		t_cont* cont = list_get(list, 0);

		fputs(cont->line, resultado);
		free(cont->line);
		cont->line = NULL;
	}

	void free_cont(t_cont *cont) {
		fclose(cont->file);
		free(cont);
	}

	list_map(listaArchivos, (void*) free_cont);
	fclose(resultado);
}

void crearScript(char* bufferScript, int etapa) {
	log_trace(logger, "Iniciando creacion de script");
	char mode[] = "0777";
	FILE* script;
	int aux = string_length(bufferScript);
	int auxChmod = strtol(mode, 0, 8);
	char* nombreArchivo;

	if (etapa == mensajeProcesarTransformacion)
		nombreArchivo = "transformador.sh";
	else if (etapa == mensajeProcesarRedLocal)
		nombreArchivo = "reductorLocal.pl";
	else if (etapa == mensajeProcesarRedGlobal)
		nombreArchivo = "reductorGlobal.pl";

	char* ruta = string_from_format("../scripts/%s", nombreArchivo);
	script = fopen(ruta, "w+");
	fwrite(bufferScript, sizeof(char), aux, script);
	auxChmod = strtol(mode, 0, 8);
	if (chmod(ruta, auxChmod) < 0) {
		log_error(logger, "NO SE PUDO DAR PERMISOS DE EJECUCION AL ARCHIVO");
	}
	log_trace(logger, "Script creado con permisos de ejecucion");
	fclose(script);
}

void handlerMaster(int clientSocket) {
	respuesta paquete;
	parametrosAlmacenamiento* almacenamiwnto;
	parametrosTransformacion* transformacion;
	parametrosReduccionLocal* reduccionLocal;
	parametrosReduccionGlobal* reduccionGlobal ;

	char* destino, *contenidoScript, *command, *rutaArchivoFinal, *archivoPreReduccion = "preReduccion";
	t_list* listaArchivosTemporales, *listAux, *listaWorkers;
	char* path = obtenerPathActual();

	paquete = desempaquetar(clientSocket);

	switch (paquete.idMensaje) {
	case mensajeProcesarTransformacion:
		transformacion = (parametrosTransformacion*)paquete.envio;
		log_trace(logger, "Iniciando Transformacion");
		contenidoScript = transformacion->contenidoScript.cadena;
		int numeroBloqueTransformado = transformacion->bloquesConSusArchivos.numBloque;
		int bloqueId = transformacion->bloquesConSusArchivos.numBloqueEnNodo;
		int bytesRestantes = transformacion->bloquesConSusArchivos.bytesOcupados;
		destino = transformacion->bloquesConSusArchivos.archivoTemporal.cadena;
		int offset = bloqueId * mb + bytesRestantes;
		crearScript(contenidoScript, mensajeProcesarTransformacion);
		log_trace(logger, "Aplicar transformacion en %i bytes del bloque %i",
				bytesRestantes, numeroBloqueTransformado);
		string_append(&path, "/tmp");
		command =
				string_from_format(
						"head -c %d < %s | tail -c %d | sh %s | sort > %s/%s",
						offset, config.RUTA_DATABIN, bytesRestantes, "../scripts/transformador.sh", path , destino);
		ejecutarComando(command, clientSocket);
		log_trace(logger, "Transformacion realizada correctamente");
		empaquetar(clientSocket, mensajeTransformacionCompleta, 0, &numeroBloqueTransformado);
		free(transformacion);
		exit(0);
		break;
	case mensajeProcesarRedLocal:
		reduccionLocal = (parametrosReduccionLocal*)paquete.envio;
		log_trace(logger, "Iniciando Reduccion Local %s",reduccionLocal->rutaDestino.cadena);
		int numeroNodo = reduccionLocal->numero;
		contenidoScript = strdup(reduccionLocal->contenidoScript.cadena);
		listAux = list_create();
		listaArchivosTemporales = list_create();
		list_add_all(listAux,reduccionLocal->archivosTemporales);
		void agregarPathAElemento(string* elemento){
			char* ruta = string_from_format("%s/tmp/%s", path, elemento->cadena);
			list_add(listaArchivosTemporales, ruta);
		}
		list_iterate(listAux, (void*) agregarPathAElemento);
		destino = strdup(reduccionLocal->rutaDestino.cadena);
		crearScript(contenidoScript, mensajeProcesarRedLocal);
		char* aux = string_from_format("%s/tmp/%s%i", path, archivoPreReduccion); // /home/utnso/tp-2017-2c-PEQL/Worker/Debug/tmp/preReduccion
		apareoArchivosLocales(listaArchivosTemporales, aux);
		command = string_from_format("cat %s | perl %s > %s", aux, string_from_format("../scripts/reductorLocal.pl"), string_from_format("%s/tmp/%s", path, destino));
		ejecutarComando(command, clientSocket);
		log_trace(logger, "Reduccion local realizada correctamente");
		empaquetar(clientSocket, mensajeRedLocalCompleta, 0, &numeroNodo);
		free(reduccionLocal);
		exit(0);
		break;
	case mensajeProcesarRedGlobal:
		reduccionGlobal = (parametrosReduccionGlobal*)paquete.envio;
		log_trace(logger, "Soy el Worker Encargado");
		destino = reduccionGlobal->archivoTemporal.cadena;
		listaWorkers = list_create();
		list_add_all(listaWorkers, reduccionGlobal->infoWorkers);
		contenidoScript = strdup(reduccionGlobal->contenidoScript.cadena);
		crearScript(contenidoScript, mensajeProcesarRedGlobal);
		rutaArchivoFinal = crearRutaArchivoAReducir(listaWorkers);
		command = string_from_format("cat %s | perl %s > %s", rutaArchivoFinal, string_from_format("../scripts/reductorGlobal.pl"), string_from_format("%s/tmp/%s", path, destino));
		ejecutarComando(command, clientSocket);
		log_trace(logger, "Reduccion global realizada correctamente");
		empaquetar(clientSocket, mensajeRedGlobalCompleta, 0, 0);
		free(reduccionGlobal);
		exit(0);
		break;

	case mensajeProcesarAlmacenamiento:
		log_trace(logger, "Soy el Worker Encargado de almacenar");
		almacenamiwnto = (parametrosAlmacenamiento*)paquete.envio;

		printf("TEMPORAL: %s || ALMACENAMIENTO %s \n\n",almacenamiwnto->archivoTemporal.cadena,almacenamiwnto->rutaAlmacenamiento.cadena);
		empaquetar(clientSocket,mensajeAlmacenamientoCompleto,0,0);
		exit(0);
		break;



	default:
		break;
	}
}

char* obtenerPathActual(){
	char *path = string_new();
	char cwd[1024];
	string_append(&path, getcwd(cwd, sizeof(cwd)));
	return path;
}

char* crearRutaArchivoAReducir(t_list* listaWorkers) {
	log_trace(logger, "Creando ruta de archivo final a reducir");
	t_list* archivosAReducir = list_create();
	int socket;
	char* rutaArchivoAReducir;
	char* path = obtenerPathActual();
	log_trace(logger, "Cantidad de Workers a los que me tengo que conectar: %i", list_size(listaWorkers)-1);
	rutaArchivoAReducir = string_from_format("%s/tmp/%s", path, "ArchivoFinalAReducir");
	int i;
	for (i = 0; i < list_size(listaWorkers); i++) {
		infoWorker* worker = list_get(listaWorkers, i);

		if (config.PUERTO_WORKER != worker->puerto) { // !string_equals_ignore_case(config.IP_NODO, worker->ip.cadena) SE SACA PARA PROBAR LOCAL
			log_trace(logger, "Iniciando conexion con %s:%i", worker->ip.cadena, worker->puerto);
			socket = crearSocket();
			struct sockaddr_in direccion = cargarDireccion(worker->ip.cadena, worker->puerto);
			conectarCon(direccion, socket, idWorker);
			respuesta respuestaHandShake = desempaquetar(socket);

			if (respuestaHandShake.idMensaje != mensajeOk) {
				log_error(logger, "Conexion fallida con Worker");
				exit(1);
			}
			log_trace(logger, "Conexion con Worker establecida");

			string* rutaArchivo = malloc(sizeof(string));
			rutaArchivo->cadena = string_from_format("%s/tmp/%s", path, worker->nombreArchivoReducido.cadena);
			rutaArchivo->longitud = strlen(rutaArchivo->cadena);
			log_trace(logger, "Enviando solicitud de archivo a Worker");

			empaquetar(socket, mensajeSolicitudArchivo, 0, rutaArchivo);

			respuesta respuesta = desempaquetar(socket);
			string* contenidoArchivo = (string*)respuesta.envio;

			log_trace(logger, "Recibo contenido del archivo y lo creo");

			char* preGlobal = string_from_format("%sGLOBAL", rutaArchivo->cadena);
			FILE* archivo = fopen(preGlobal, "w+");
			fwrite(contenidoArchivo->cadena, sizeof(char), contenidoArchivo->longitud, archivo);
			fclose(archivo);
			list_add(archivosAReducir, preGlobal);
			close(socket);
		} else {
			log_trace(logger, "Agrego a la lista mi archivo local");
			char* rutaArchivo = string_from_format("%s/tmp/%s", path, worker->nombreArchivoReducido.cadena);
			list_add(archivosAReducir, rutaArchivo);
		}
	}
	apareoArchivosLocales(archivosAReducir, rutaArchivoAReducir);
	return rutaArchivoAReducir;
}

void handlerWorker(int clientSocket) {
	respuesta solicitudWorker;
	string* rutaArchivoSolicitado = malloc(sizeof(string));
	string* contenidoArchivo = malloc(sizeof(string));

	while (1) {

		solicitudWorker = desempaquetar(clientSocket);
		switch (solicitudWorker.idMensaje) {
		case mensajeSolicitudArchivo:

			rutaArchivoSolicitado = (string*)solicitudWorker.envio;
			log_trace(logger, "Archivo solicitado en: %s", rutaArchivoSolicitado->cadena);

			struct stat fileStat;
			if(stat(rutaArchivoSolicitado->cadena,&fileStat) < 0){
				printf("No se pudo abrir el archivo\n");
			}

			int fd = open(rutaArchivoSolicitado->cadena,O_RDWR);
			int size = fileStat.st_size;

			contenidoArchivo->cadena = mmap(NULL,size,PROT_READ,MAP_SHARED,fd,0);
			contenidoArchivo->longitud = size;

			empaquetar(clientSocket, mensajeRespuestaSolicitudArchivo, 0, contenidoArchivo);

			log_trace(logger, "Envio contenido de archivo");

			if (munmap(contenidoArchivo->cadena, contenidoArchivo->longitud) == -1){
				perror("Error un-mmapping the file");
				exit(EXIT_FAILURE);
			}
			break;
		default:
			break;
		}
	}
}

void levantarServidorWorker(char* ip, int port) {
	int sock;
	sock = crearServidorAsociado(ip, port);

	while (1) {
		struct sockaddr_in their_addr;
		socklen_t size = sizeof(struct sockaddr_in);
		clientSocket = accept(sock, (struct sockaddr*) &their_addr, &size);
		int pid;

		if (clientSocket == -1) {
			close(clientSocket);
			perror("accept");
		}

		respuesta conexionNueva;
		conexionNueva = desempaquetar(clientSocket);

		if (conexionNueva.idMensaje == 1) {
			if (*(int*) conexionNueva.envio == 2) {
				log_trace(logger, "Conexion con Master establecida");
				if ((pid = fork()) == 0) {
					log_trace(logger, "Proceso hijo:%d", pid);
					log_trace(logger, "Esperando instruccion de Master");
					handlerMaster(clientSocket);
				} else if (pid > 0) {
					log_trace(logger, "Proceso Padre:%d", pid);
					//close(clientSocket);
					continue;
				} else if (pid < 0) {
					log_error(logger, "NO SE PUDO HACER EL FORK");
				}
			} else if (*(int*) conexionNueva.envio == idWorker) {
				empaquetar(clientSocket,mensajeOk,0,0);
				log_trace(logger, "Conexion con Worker establecida");
				if ((pid = fork()) == 0) {
					log_trace(logger, "Proceso hijo de worker:%d", pid);
					handlerWorker(clientSocket);
				} else if (pid > 0) {
					log_trace(logger, "Proceso Worker Padre:%d", pid);
				} else if (pid < 0) {
					log_error(logger, "NO SE PUDO HACER EL FORK");
				}
			}
		}
	}
}

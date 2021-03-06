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

void crearScript(char* bufferScript, int etapa, int pid) {
	log_trace(logger, "Iniciando creacion de script");
	char mode[] = "0777";
	FILE* script;
	int aux = string_length(bufferScript);
	int auxChmod = strtol(mode, 0, 8);
	char* nombreArchivo;

	if (etapa == mensajeProcesarTransformacion)
		nombreArchivo = string_from_format("transformador%d", pid);
	else if (etapa == mensajeProcesarRedLocal)
		nombreArchivo = string_from_format("reductorLocal%d", pid);
	else if (etapa == mensajeProcesarRedGlobal)
		nombreArchivo = string_from_format("reductorGlobal%d", pid);

	char* ruta = string_from_format("./%s", nombreArchivo);
	script = fopen(ruta, "w+");
	fwrite(bufferScript, sizeof(char), aux, script);
	auxChmod = strtol(mode, 0, 8);
	if (chmod(ruta, auxChmod) < 0) {
		log_error(logger, "NO SE PUDO DAR PERMISOS DE EJECUCION AL ARCHIVO");
	}
	log_trace(logger, "Script creado con permisos de ejecucion");
	fclose(script);
}

int conectarseConFS() {
	int socket = crearSocket();
	struct sockaddr_in direccion = cargarDireccion(config.IP_FILESYSTEM,config.PUERTO_FILESYSTEM);
	conectarCon(direccion, socket, idWorker);
	return socket;
}

void handlerMaster(int clientSocket, int pid) {
	respuesta paquete, confirmacionFS, conexion;
	parametrosTransformacion* transformacion;
	parametrosReduccionLocal* reduccionLocal;
	parametrosReduccionGlobal* reduccionGlobal;
	parametrosAlmacenamiento* almacenamiento;

	char* destino, *contenidoScript, *command, *rutaArchivoFinal;
	t_list* listaArchivosTemporales, *listAux, *listaWorkers;
	char* path = obtenerPathActual();
	char* script = string_new();

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
		crearScript(contenidoScript, mensajeProcesarTransformacion, pid);
		log_trace(logger, "Aplicar transformacion en %i bytes del bloque %i",
				bytesRestantes, numeroBloqueTransformado);
		string_append(&path, "/tmp");
		command =
				string_from_format(
						"head -c %d < %s | tail -c %d | ./transformador%d | sort > %s/%s",
						offset, config.RUTA_DATABIN, bytesRestantes, pid, path , destino);
		ejecutarComando(command, clientSocket);
		log_trace(logger, "Transformacion realizada correctamente");
		empaquetar(clientSocket, mensajeFinTransformacion, 0, &numeroBloqueTransformado);
		free(transformacion);
		script = string_from_format("%s/transformador%d",obtenerPathActual(),pid);
		remove(script);
		free(script);
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
		crearScript(contenidoScript, mensajeProcesarRedLocal, pid);
		char* aux = string_from_format("%s/tmp/preReduccion%d", path, pid);
		apareo(listaArchivosTemporales, aux);
		command = string_from_format("cat %s | ./reductorLocal%d > %s", aux, pid, string_from_format("%s/tmp/%s", path, destino));
		ejecutarComando(command, clientSocket);
		log_trace(logger, "Reduccion local realizada correctamente");
		empaquetar(clientSocket, mensajeRedLocalCompleta, 0, &numeroNodo);
		free(reduccionLocal);
		script = string_from_format("%s/reductorLocal%d", path, pid);
		remove(aux);
		remove(script);
		free(script);
		exit(0);
		break;
	case mensajeProcesarRedGlobal:
		reduccionGlobal = (parametrosReduccionGlobal*)paquete.envio;
		log_trace(logger, "Soy el Worker Encargado");
		destino = reduccionGlobal->archivoTemporal.cadena;
		listaWorkers = list_create();
		list_add_all(listaWorkers, reduccionGlobal->infoWorkers);
		contenidoScript = strdup(reduccionGlobal->contenidoScript.cadena);
		crearScript(contenidoScript, mensajeProcesarRedGlobal, pid);
		rutaArchivoFinal = crearRutaArchivoAReducir(listaWorkers);
		command = string_from_format("cat %s |./reductorGlobal%d > %s", rutaArchivoFinal, pid, string_from_format("%s/tmp/%s", path, destino));
		ejecutarComando(command, clientSocket);
		log_trace(logger, "Reduccion global realizada correctamente");
		empaquetar(clientSocket, mensajeRedGlobalCompleta, 0, 0);
		free(reduccionGlobal);
		script = string_from_format("%s/reductorGlobal%d",obtenerPathActual(),pid);
		remove(script);
		free(script);
		exit(0);
		break;
	case mensajeProcesarAlmacenamiento:
		log_trace(logger, "Soy el Worker Encargado de almacenar");
		almacenamiento = (parametrosAlmacenamiento*)paquete.envio;
		int socketFS = conectarseConFS();
		conexion = desempaquetar(socketFS);
		if(conexion.idMensaje != mensajeOk){
			log_trace(logger, "Fallo en conexion con FS");
			empaquetar(clientSocket,mensajeFalloAlmacenamiento, 0, 0);
			exit(0);
			break;
		}
		log_trace(logger, "Conexion con File System establecida");
		almacenamientoFinal* almacenar = malloc(sizeof(almacenamientoFinal));
		almacenar->nombre.longitud = almacenamiento->rutaAlmacenamiento.longitud;
		almacenar->nombre.cadena = strdup(almacenamiento->rutaAlmacenamiento.cadena);
		char* rutaArchivo = string_new();
		rutaArchivo = string_from_format("%s/tmp/%s", path, almacenamiento->archivoTemporal.cadena);
		struct stat fileStat;
		if(stat(rutaArchivo,&fileStat) < 0){
			printf("No se pudo abrir el archivo\n");
			exit(0);
		}
		int fd = open(rutaArchivo,O_RDWR);
		int size = fileStat.st_size;
		almacenar->contenido.cadena = mmap(NULL,size,PROT_READ,MAP_SHARED,fd,0);
		almacenar->contenido.longitud = size;
		empaquetar(socketFS, mensajeAlmacenar, 0, almacenar);
		if (munmap(almacenar->contenido.cadena, almacenar->contenido.longitud) == -1){
			perror("Error un-mmapping the file");
			exit(EXIT_FAILURE);
		}
		close(fd);
		confirmacionFS = desempaquetar(socketFS);
		if(confirmacionFS.idMensaje == mensajeAlmacenamientoCompleto){
			empaquetar(clientSocket,mensajeAlmacenamientoCompleto, 0, 0);
		}else {
			empaquetar(clientSocket,mensajeFalloAlmacenamiento, 0, 0);
		}
		close(socketFS);
		free(almacenamiento);
		exit(0);
		break;
	default:
		break;
	}
}


void apareo(t_list* lista, char* archivoFinal){

	log_trace(logger, "Comenzando apareo de archivos");

	char* nombreArchivo = list_remove(lista, 0);
	FILE* arch1 = fopen(nombreArchivo, "r");
	FILE* arch2 = fopen(archivoFinal, "w");

	char* str1 = malloc(1024);
	memset(str1,0,1024);

	fgets(str1, 1024, arch1);
	while(!feof(arch1)){
		fputs(str1, arch2);
		memset(str1,0,1024);
		fgets(str1, 1024, arch1);
	}

	fclose(arch1);
	fclose(arch2);

	void aparear(char* file1) {
		FILE* arch1 = fopen(file1, "r");
		FILE* arch2 = fopen(archivoFinal, "r");
		char* s = string_from_format("tmp/temporal%d", process_getpid());
		char* s2 = string_from_format("tmp/temp%d", process_getpid());
		FILE* resul = fopen(s,"w");
		char* str1 = malloc(1024);
		char* str2 = malloc(1024);
		memset(str1,0,1024);
		memset(str2,0,1024);
		fgets(str1, 1024, arch1);
		fgets(str2, 1024, arch2);

		while ((!feof(arch1)) && (!feof(arch2))) {
			if (strcmp(str1, str2) < 0) {
				fputs(str1, resul);
				memset(str1,0,1024);
				fgets(str1, 1024, arch1);
			} else {
				fputs(str2, resul);
				memset(str2,0,1024);
				fgets(str2, 1024, arch2);
			}
		}
		if ((feof(arch1)) && (feof(arch2))) {
			fclose(arch1);
			fclose(arch2);
			fclose(resul);
			free(str1);
			free(str2);
			rename(archivoFinal,s2);
			system(string_from_format("rm %s", s2));
			rename(s,archivoFinal);
			return;
		} else {
			if (feof(arch1)) {
				fputs(str2, resul);
				while (!feof(arch2)) {
					memset(str2,0,1024);
					fgets(str2, 1024, arch2);
					fputs(str2, resul);
				}
			} else {
				fputs(str1, resul);
				while (!feof(arch1)) {
					memset(str1,0,1024);
					fgets(str1, 1024, arch1);
					fputs(str1, resul);
				}
			}
			fclose(arch1);
			fclose(arch2);
			fclose(resul);
			free(str1);
			free(str2);
			rename(archivoFinal,s2);
			system(string_from_format("rm %s", s2));
			rename(s,archivoFinal);
		}
		return;
	}
	list_iterate(lista,(void*)aparear);
	log_trace(logger, "Apareo finalizado");
}

char* obtenerPathActual(){
	char *path = string_new();
	char cwd[1024];
	string_append(&path, getcwd(cwd, sizeof(cwd)));
	return path;
}

char* crearRutaArchivoAReducir(t_list* listaWorkers) {
	log_trace(logger, "Creando archivo final a reducir");
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
			log_trace(logger, "Conexion con Worker homonimo establecida");

			string* rutaArchivo = malloc(sizeof(string));
			rutaArchivo->cadena = worker->nombreArchivoReducido.cadena;
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
	apareo(archivosAReducir, rutaArchivoAReducir);
	return rutaArchivoAReducir;
}

void handlerWorker(int clientSocket) {
	respuesta solicitudWorker;
	string* nombreArchivoSolicitado = malloc(sizeof(string));
	string* contenidoArchivo = malloc(sizeof(string));
	char* path = obtenerPathActual();

	while (1) {

		solicitudWorker = desempaquetar(clientSocket);
		switch (solicitudWorker.idMensaje) {
		case mensajeSolicitudArchivo:

			nombreArchivoSolicitado = (string*)solicitudWorker.envio;
			log_trace(logger, "Archivo solicitado: %s", nombreArchivoSolicitado->cadena);

			char* rutaArchivoSolicitado = string_from_format("%s/tmp/%s", path, nombreArchivoSolicitado->cadena);

			struct stat fileStat;
			if(stat(rutaArchivoSolicitado,&fileStat) < 0){
				printf("No se pudo abrir el archivo\n");
			}

			int fd = open(rutaArchivoSolicitado,O_RDWR);
			int size = fileStat.st_size;

			contenidoArchivo->cadena = mmap(NULL,size,PROT_READ,MAP_SHARED,fd,0);
			contenidoArchivo->longitud = size;

			empaquetar(clientSocket, mensajeRespuestaSolicitudArchivo, 0, contenidoArchivo);

			log_trace(logger, "Envio contenido de archivo al Worker encargado");

			if (munmap(contenidoArchivo->cadena, contenidoArchivo->longitud) == -1){
				perror("Error un-mmapping the file");
				exit(EXIT_FAILURE);
			}

			log_trace(logger, "Fin conexion con Worker encargado");

			close(clientSocket);
			exit(0);
			break;
		default:
			break;
		}
	}
}

void levantarServidorWorker(char* ip, int port) {
	int sock;
	sock = crearServidorAsociado(ip, port);
	respuesta conexionNueva;

	struct sockaddr_in their_addr;
	socklen_t size = sizeof(struct sockaddr_in);
	int clientSocket;
	int pid;
	while (1) {
		clientSocket = accept(sock, (struct sockaddr*) &their_addr, &size);

		if (clientSocket == -1) {
			close(clientSocket);
			perror("accept");
		}

		conexionNueva = desempaquetar(clientSocket);

		if (conexionNueva.idMensaje == 1) {
			if (*(int*) conexionNueva.envio == 2) {
				log_trace(logger, "Conexion con Master establecida");
				if ((pid = fork()) == 0) {
					log_trace(logger, "Esperando instruccion de Master");
					prctl(PR_SET_PDEATHSIG, SIGHUP);
					handlerMaster(clientSocket, process_getpid());
				} else if (pid > 0) {
					log_trace(logger, "Proceso Padre sigue escuchando conexiones");
					continue;
				} else if (pid < 0) {
					log_error(logger, "NO SE PUDO HACER EL FORK");
				}
			} else if (*(int*) conexionNueva.envio == idWorker) {
				empaquetar(clientSocket,mensajeOk,0,0);
				log_trace(logger, "Conexion con Worker encargado establecida");
				if ((pid = fork()) == 0) {
					log_trace(logger, "Esperando instruccion de Worker encargado");
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

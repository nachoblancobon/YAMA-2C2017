/*
 * Planificador.c
 *
 *  Created on: 3/10/2017
 *      Author: utnso
 */

#include "Planificador.h"

void iniciarListasPlanificacion(){
	jobsAPlanificar = list_create();
}
infoNodo* inicializarWorker(){
	infoNodo* worker = malloc(sizeof(infoNodo));

	worker->bloques  = list_create();
	worker->carga = 0;
	worker->disponibilidad = 0;
	worker->cantTareasHistoricas = 0;
	wlMax = 0;
	return worker;
}
void planificar(job* job){
	infoNodo* worker = inicializarWorker();
	t_list* listaNodos = list_create();
	int nodos,bloques;

	informacionArchivoFsYama* infoArchivo = recibirInfoArchivo(job);//RECIBE BLOQUES Y TAMAÑO DE FS SOBRE EL ARCHIVO DEL JOB

	llenarListaNodos(listaNodos,infoArchivo);

	if(list_is_empty(listaNodos)){
		finalizarJob(job,TRANSFORMACION);
	}

	calcularDisponibilidadWorkers(listaNodos);

	worker = posicionarClock(listaNodos);//POSICIONA EL CLOCK EN EL WORKER DE MAYOR DISPONIBILIDAD

	calcularNodosYBloques(infoArchivo,&nodos,&bloques);

	bool** matrix = llenarMatrizNodosBloques(infoArchivo,nodos,bloques);

	respuestaSolicitudTransformacion* respuestaMaster = moverClock(worker, listaNodos, matrix, infoArchivo->informacionBloques,job->id);

	empaquetar(job->socketFd,mensajeRespuestaTransformacion,0,respuestaMaster);

	actualizarCargasNodos(job->id,TRANSFORMACION);

	planificarReduccionesLocales(job,matrix,respuestaMaster,nodos);

	enviarReduccionGlobalAMaster(job);
	actualizarCargasNodos(job->id,RED_GLOBAL);
	esperarRespuestaReduccionDeMaster(job);

	realizarAlmacenamientoFinal();
}


respuestaSolicitudTransformacion* moverClock(infoNodo* workerDesignado, t_list* listaNodos, bool** nodosPorBloque, t_list* infoBloques,int job){
	int i;
	int cantidadBloques = list_size(infoBloques);
	infoBloque* bloque = malloc(sizeof(infoBloque));
	respuestaSolicitudTransformacion* respuestaAMaster = malloc(sizeof(respuestaSolicitudTransformacion));
	respuestaAMaster->workers = list_create();

	for(i=0;i<cantidadBloques;i++){
		bloque = (infoBloque*)list_get(infoBloques, i);

		if(bloqueEstaEn(workerDesignado,nodosPorBloque,i)){
			if(workerDesignado->disponibilidad > 0){
				modificarCargayDisponibilidad(workerDesignado);
				agregarBloqueANodoParaEnviar(bloque,workerDesignado,respuestaAMaster,job);
				log_trace(logger, "Bloque %i asignado al worker %i | Disp %i",bloque->numeroBloque, workerDesignado->numero, workerDesignado->disponibilidad);

				workerDesignado = avanzarClock(workerDesignado, listaNodos);
				if(workerDesignado->disponibilidad <= 0){
					workerDesignado->disponibilidad += getDisponibilidadBase();
					workerDesignado = avanzarClock(workerDesignado, listaNodos);
				}

			}
			else{
				restaurarDisponibilidad(workerDesignado);
				workerDesignado = avanzarClock(workerDesignado, listaNodos);
				if(workerDesignado->disponibilidad <= 0){
					workerDesignado->disponibilidad += getDisponibilidadBase();
					workerDesignado = avanzarClock(workerDesignado, listaNodos);
				}
				modificarCargayDisponibilidad(workerDesignado);

				agregarBloqueANodoParaEnviar(bloque,workerDesignado,respuestaAMaster,job);
				log_trace(logger, "Bloque %i asignado al worker %i | Disp %i",bloque->numeroBloque, workerDesignado->numero, workerDesignado->disponibilidad);
				workerDesignado = avanzarClock(workerDesignado, listaNodos);

				if(workerDesignado->disponibilidad <= 0){
					workerDesignado->disponibilidad += getDisponibilidadBase();
					workerDesignado = avanzarClock(workerDesignado, listaNodos);
				}
			}
		}
		else {
			printf("El worker %d no tiene el bloque %d \n",workerDesignado->numero,bloque->numeroBloque);
			infoNodo* proximoWorker = obtenerProximoWorkerConBloque(listaNodos,i,workerDesignado->numero);
			modificarCargayDisponibilidad(proximoWorker);

			agregarBloqueANodoParaEnviar(bloque,proximoWorker,respuestaAMaster,job);
			log_trace(logger, "Bloque %i asignado al worker %i | Disp %i",bloque->numeroBloque, proximoWorker->numero, proximoWorker->disponibilidad);
		}

	}

	return respuestaAMaster;

}
void verificarValorDisponibilidad(infoNodo* nodo){
	if(nodo->disponibilidad == 0){
		restaurarDisponibilidad(nodo);
	}
}
void restaurarDisponibilidad(infoNodo* worker){
	worker->disponibilidad += getDisponibilidadBase();
}

infoNodo* avanzarClock(infoNodo* worker, t_list* listaNodos){
	bool nodoConNumero(infoNodo* nodo){
		return nodo->numero == worker->numero;
	}

	infoNodo* siguienteWorker = malloc(sizeof(infoNodo));
	list_remove_by_condition(listaNodos, (void*)nodoConNumero);
	list_add_in_index(listaNodos, list_size(listaNodos), worker);

	siguienteWorker = list_get(listaNodos, 0);

	return siguienteWorker;
}

bool bloqueEstaEn(infoNodo* nodo,bool** nodoXbloque, int bloque){
	int posNodo = nodo->numero;
	return nodoXbloque[posNodo][bloque];
}

informacionArchivoFsYama* recibirInfoArchivo(job* job) {
	solicitudInfoNodos* solTransf = malloc(sizeof(solicitudInfoNodos));
	informacionArchivoFsYama* infoArchivo = malloc(sizeof(informacionArchivoFsYama));

	solTransf->rutaDatos.cadena = strdup(job->rutaDatos.cadena);
	solTransf->rutaDatos.longitud = job->rutaDatos.longitud;
	solTransf->rutaResultado.cadena = strdup(job->rutaResultado.cadena);
	solTransf->rutaResultado.longitud = job->rutaResultado.longitud;

	infoArchivo = solicitarInformacionAFS(solTransf);
	return infoArchivo;
}

infoNodo* posicionarClock(t_list* listaWorkers){
	infoNodo* workerDesignado = malloc(sizeof(infoNodo));
	list_sort(listaWorkers, (void*)mayorDisponibilidad);

	workerDesignado = list_get(listaWorkers, 0);//Ya desempata por cantidad de tareas historicas (PROBAR)
	return workerDesignado;
}

bool mayorDisponibilidad(infoNodo* workerMasDisp, infoNodo* worker){

	int maxDisponibilidad = obtenerDisponibilidadWorker(workerMasDisp);
	int disponibilidad =  obtenerDisponibilidadWorker(worker);
	//if(maxDisponibilidad == disponibilidad){
	//	return workerMasDisp->cantTareasHistoricas < worker->cantTareasHistoricas;
		//FIXME FALLA VALGRIND ACA PORQUE NUNCA SE INICIALIZA
	//}

	return maxDisponibilidad > disponibilidad;

}

infoNodo* buscarNodo(t_list* nodos, int numNodo){
	bool nodoConNumero(infoNodo* nodo){
		return nodo->numero == numNodo;
	}

	return list_find(nodos, (void*) nodoConNumero);
}

bool estaActivo(infoNodo* worker){
	return worker->conectado == true;
}

void calcularDisponibilidadWorkers(t_list* nodos){
	calcularWorkLoadMaxima(nodos);
	list_iterate(nodos, (void*)calcularDisponibilidadWorker);
}

void calcularDisponibilidadWorker(infoNodo* worker){
	worker->disponibilidad = getDisponibilidadBase() + calcularPWL(worker);
}

int obtenerDisponibilidadWorker(infoNodo* worker){//getter
	return worker->disponibilidad;
}

uint32_t calcularPWL(infoNodo* worker){
	if(esClock() != 0){
		return workLoadMaxima() - worker->carga;
	}
	else{
		return 0;
	}
}

void calcularWorkLoadMaxima(t_list* nodos){
	infoNodo* worker = malloc(sizeof(infoNodo));
	bool mayorCarga(infoNodo* nodoMasCarga, infoNodo* nodo){
		return nodoMasCarga->carga > nodo->carga;
	}
	list_sort(nodos,(void*)mayorCarga);

	worker = list_get(nodos, 0);
	wlMax = worker->carga;
}

uint32_t workLoadMaxima(){//getter
	return wlMax;
}
void agregarJobAPlanificar(job* jobAPlanificar){
	list_add(jobsAPlanificar,jobAPlanificar);
}

void agregarNodo(t_list* lista, infoNodo* nodo){
	list_add(lista,nodo);
}

void modificarCargayDisponibilidad(infoNodo* worker){
	worker->disponibilidad--;
	worker->carga++;
}

infoNodo* obtenerProximoWorkerConBloque(t_list* listaNodos,int bloque,int numWorkerActual){
	bool nodoBloqueConNumero(infoBloque* bloquee){
		return bloquee->numeroBloque == bloque;
	}
	bool nodoConNumero(infoNodo* nodo){
		return nodo->numero != numWorkerActual && list_any_satisfy(nodo->bloques, (void*) nodoBloqueConNumero) && (nodo->disponibilidad>0);
	}
	void restaurarDisp(infoNodo* nodo){
		if(nodo->numero != numWorkerActual){
			nodo->disponibilidad += getDisponibilidadBase();
		}
	}
	infoNodo* nodoEncontrado = list_find(listaNodos, (void*) nodoConNumero);
	if(nodoEncontrado == NULL){
		list_iterate(listaNodos,(void*) restaurarDisp);
		return list_find(listaNodos, (void*) nodoConNumero);
	}
	return nodoEncontrado;

}

void agregarBloqueANodoParaEnviar(infoBloque* bloque,infoNodo* nodo,respuestaSolicitudTransformacion* respuestaMaster,int job){
	workerDesdeYama* worker;
	bloquesConSusArchivosTransformacion* bloquesArchivos = malloc(sizeof(bloquesConSusArchivosTransformacion));

	bool nodoConNumero(workerDesdeYama* worker){
		return worker->numeroWorker == nodo->numero;
	}

	if( list_find(respuestaMaster->workers, (void*) nodoConNumero)){
		worker = list_find(respuestaMaster->workers, (void*) nodoConNumero);
	}
	else{
		worker = malloc(sizeof(workerDesdeYama));
		worker->numeroWorker = nodo->numero;
		worker->puerto = nodo->puerto;
		worker->ip.longitud = nodo->ip.longitud;
		worker->ip.cadena = strdup(nodo->ip.cadena);
		worker->bloquesConSusArchivos = list_create();
		list_add(respuestaMaster->workers,worker);
	}

	char* rutaTemporal = dameUnNombreArchivoTemporal(job,bloque->numeroBloque,TRANSFORMACION,worker->numeroWorker);

	bloquesArchivos->numBloque = bloque->numeroBloque;
	bloquesArchivos->bytesOcupados = bloque->bytesOcupados;
	bloquesArchivos->archivoTemporal.cadena = strdup(rutaTemporal);
	bloquesArchivos->archivoTemporal.longitud = string_length(rutaTemporal);

	if(bloque->ubicacionCopia0.numeroNodo == nodo->numero){
		bloquesArchivos->numBloqueEnNodo = bloque->ubicacionCopia0.numeroBloqueEnNodo;
	}
	else{
		bloquesArchivos->numBloqueEnNodo = bloque->ubicacionCopia1.numeroBloqueEnNodo;
	}

	list_add(worker->bloquesConSusArchivos,bloquesArchivos);

	registroTablaEstados* registro = malloc(sizeof(registroTablaEstados));
	registro->bloque= bloquesArchivos->numBloque;
	registro->estado=EN_EJECUCION;
	registro->etapa= TRANSFORMACION;
	registro->job = job;
	registro->nodo= nodo->numero;
	registro->rutaArchivoTemp = strdup(bloquesArchivos->archivoTemporal.cadena);

	pthread_mutex_lock(&mutexTablaEstados);
	list_add(tablaDeEstados,registro);
	pthread_mutex_unlock(&mutexTablaEstados);

}

void replanificar(bloqueYNodo* paraReplanificar,job* jobi,respuestaSolicitudTransformacion* respuestaArchivo, bool** matrix,int bloques){
	workerDesdeYama* respuestaAMaster = malloc(sizeof(workerDesdeYama));
	respuestaAMaster->bloquesConSusArchivos= list_create();

	int nodoNuevo = nodoConOtraCopia(paraReplanificar,matrix,bloques);

	if(nodoNuevo == -1){
		finalizarJob(jobi,TRANSFORMACION);
	}

	bool nodoConNumero(workerDesdeYama* worker){
		return worker->numeroWorker == paraReplanificar->workerId ;
	}

	workerDesdeYama* worker = list_find(respuestaArchivo->workers, (void*) nodoConNumero);

	bool bloqueConNumero(bloquesConSusArchivosTransformacion* bloque){
		return bloque->numBloque== paraReplanificar->bloque ;
	}

	bloquesConSusArchivosTransformacion* bloque= list_find(worker->bloquesConSusArchivos, (void*) bloqueConNumero);

	bloque->archivoTemporal.cadena = dameUnNombreArchivoTemporal(jobi->id,bloque->numBloque,TRANSFORMACION,nodoNuevo);
	bloque->archivoTemporal.longitud = string_length(bloque->archivoTemporal.cadena);

	list_add(respuestaAMaster->bloquesConSusArchivos,bloque);

	respuestaAMaster->numeroWorker= nodoNuevo;

	pthread_mutex_lock(&mutex_NodosConectados);
	infoNodo* infoNod = obtenerNodo(nodoNuevo);

	respuestaAMaster->puerto= infoNod->puerto;
	respuestaAMaster->ip.cadena = strdup(infoNod->ip.cadena);
	respuestaAMaster->ip.longitud = infoNod->ip.longitud;

	pthread_mutex_unlock(&mutex_NodosConectados);

	empaquetar(jobi->socketFd,mensajeReplanificacion,0,respuestaAMaster);

	actualizarCargasNodosReplanificacion(jobi->id,TRANSFORMACION,bloque->numBloque);

	log_trace(logger, "Replanifico bloque %i asignado al worker %i ",bloque->numBloque, respuestaAMaster->numeroWorker);

}

void enviarReduccionLocalAMaster(job* job,int nodo){
	respuestaReduccionLocal* respuestaTodos = malloc(sizeof(respuestaReduccionLocal));
	respuestaTodos->workers = list_create();

	bool encontrarEnTablaEstados(registroTablaEstados* reg) {
		return reg->job == job->id && reg->etapa == TRANSFORMACION && reg->nodo==nodo;
	}

	pthread_mutex_lock(&mutexTablaEstados);
	t_list* registrosRedLocal = list_filter(tablaDeEstados,(void*)encontrarEnTablaEstados);
	pthread_mutex_unlock(&mutexTablaEstados);

	void meterEnRespuestaRedLocal(registroTablaEstados* reg){
		bloquesConSusArchivosRedLocal* bloquesArchivos = malloc(sizeof(bloquesConSusArchivosRedLocal));
		workerDesdeYama* worker;

		//pthread_mutex_lock(&mutex_NodosConectados);
		infoNodo* infoNod = obtenerNodo(reg->nodo);

		bool nodoConNumero(workerDesdeYama* worker){
			return worker->numeroWorker == infoNod->numero;
		}

		if( list_find(respuestaTodos->workers, (void*) nodoConNumero)){
			worker = list_find(respuestaTodos->workers, (void*) nodoConNumero);
		}
		else{
			worker = malloc(sizeof(workerDesdeYama));
			worker->numeroWorker = reg->nodo;
			worker->puerto = infoNod->puerto;
			worker->ip.longitud = infoNod->ip.longitud;
			worker->ip.cadena = strdup(infoNod->ip.cadena);
			worker->bloquesConSusArchivos = list_create();
			list_add(respuestaTodos->workers,worker);
		}
		//pthread_mutex_unlock(&mutex_NodosConectados);

		char* archivoReduccion = dameUnNombreArchivoTemporal(job->id,reg->bloque,RED_LOCAL,worker->numeroWorker);

		bloquesArchivos->numBloque = reg->bloque;

		bloquesArchivos->archivoTransformacion.cadena = strdup(reg->rutaArchivoTemp);
		bloquesArchivos->archivoTransformacion.longitud = string_length(reg->rutaArchivoTemp);

		bloquesArchivos->archivoReduccion.longitud = string_length(archivoReduccion);
		bloquesArchivos->archivoReduccion.cadena = strdup(archivoReduccion);

		list_add(worker->bloquesConSusArchivos,bloquesArchivos);

		registroTablaEstados* registro = malloc(sizeof(registroTablaEstados));
		registro->bloque=reg->bloque;
		registro->estado=EN_EJECUCION;
		registro->etapa=RED_LOCAL;
		registro->job= job->id;
		registro->nodo= reg->nodo;
		registro->rutaArchivoTemp = strdup(archivoReduccion);

		//pthread_mutex_lock(&mutexTablaEstados);
		list_add(tablaDeEstados,registro);
		//pthread_mutex_unlock(&mutexTablaEstados);
	}
	pthread_mutex_lock(&mutexTablaEstados);
	list_iterate(registrosRedLocal,(void*)meterEnRespuestaRedLocal);
	pthread_mutex_unlock(&mutexTablaEstados);
	empaquetar(job->socketFd,mensajeRespuestaRedLocal,0,respuestaTodos);
}

void enviarReduccionGlobalAMaster(job* job){
	respuestaReduccionGlobal* respuesta = malloc(sizeof(respuestaReduccionGlobal));
	respuesta->parametros = malloc(sizeof(parametrosReduccionGlobal));
	respuesta->parametros->infoWorkers = list_create();

	bool encontrarEnTablaEstados(void *registro) {
		registroTablaEstados* reg =(registroTablaEstados*)registro;
		return reg->job == job->id && reg->etapa== RED_LOCAL;
	}
	void meterEnRespuestaRedGlobal(registroTablaEstados* reg){
		infoWorker* info = malloc(sizeof(infoWorker));
		infoNodo* infoNod = obtenerNodo(reg->nodo);

		bool nodoConNumero(workerDesdeYama* worker){
			return worker->numeroWorker == infoNod->numero;
		}

		info->puerto = infoNod->puerto;;
		info->ip.longitud = infoNod->ip.longitud;
		info->ip.cadena = strdup(infoNod->ip.cadena);
		info->nombreArchivoReducido.longitud = string_length(reg->rutaArchivoTemp);
		info->nombreArchivoReducido.cadena = strdup(reg->rutaArchivoTemp);
		list_add(respuesta->parametros->infoWorkers,info);

	}

	pthread_mutex_lock(&mutexTablaEstados);
	t_list* registrosRedGlobal = list_filter(tablaDeEstados,(void*)encontrarEnTablaEstados);
	pthread_mutex_unlock(&mutexTablaEstados);

	//pthread_mutex_lock(&mutex_NodosConectados);
	infoNodo* nodoEncargado = obtenerNodo(calcularNodoEncargado(registrosRedGlobal));

	char* archivoReduccionGlobal = dameUnNombreArchivoTemporal(job->id,0,RED_GLOBAL,0);

	respuesta->archivoTemporal.longitud = string_length(archivoReduccionGlobal);
	respuesta->archivoTemporal.cadena = strdup(archivoReduccionGlobal);

	respuesta->ip.cadena = strdup(nodoEncargado->ip.cadena);
	respuesta->ip.longitud = nodoEncargado->ip.longitud;

	respuesta->puerto=nodoEncargado->puerto;

	respuesta->numero = nodoEncargado->numero;
	respuesta->job = job->id;

	list_iterate(registrosRedGlobal,(void*)meterEnRespuestaRedGlobal);
	empaquetar(job->socketFd,mensajeRespuestaRedGlobal,0,respuesta);

	registroTablaEstados* registro = malloc(sizeof(registroTablaEstados));
	registro->bloque=0;
	registro->estado=EN_EJECUCION;
	registro->etapa=RED_GLOBAL;
	registro->job= job->id;
	registro->nodo= nodoEncargado->numero;
	registro->rutaArchivoTemp = strdup(respuesta->archivoTemporal.cadena);
	//pthread_mutex_unlock(&mutex_NodosConectados);

	pthread_mutex_lock(&mutexTablaEstados);
	list_add(tablaDeEstados,registro);
	pthread_mutex_unlock(&mutexTablaEstados);

}

int calcularNodoEncargado(t_list* registrosRedGlobal){
	int menorCarga,numeroNodo;

	registroTablaEstados* reg = list_get(registrosRedGlobal,0);

	pthread_mutex_lock(&mutex_NodosConectados);
	infoNodo* nodo = obtenerNodo(reg->nodo);
	numeroNodo = reg->nodo;
	menorCarga=nodo->carga;

	int i;
	for(i=1;i<list_size(registrosRedGlobal);i++){
		reg = list_get(registrosRedGlobal,i);
		nodo = obtenerNodo(reg->nodo);

		if(nodo->carga < menorCarga ){
			numeroNodo = reg->nodo;
			menorCarga=nodo->carga;
		}
	}
	pthread_mutex_unlock(&mutex_NodosConectados);

	return numeroNodo;

}

t_list* buscarCopiasBloques(t_list* listaBloques,t_list* listaNodos,informacionArchivoFsYama* infoArchivo){
	t_list* listaNodosActivos = list_create();

	bool nodosConectadosConBloques(infoNodo* worker){
		return worker->conectado == true;
	}

	listaNodosActivos = list_filter(listaNodos, (void*)nodosConectadosConBloques);

	return listaNodosActivos;
}

void realizarAlmacenamientoFinal(){

}

void planificarReduccionesLocales(job* job,bool** matrix,respuestaSolicitudTransformacion* respuestaMaster,int nodos){
	bool redLocalIncompleta= true;
	bloqueYNodo* bloqueNodo;
	int numNodo;

	while(redLocalIncompleta){
		respuesta respuestaPlanificacionMaster=  desempaquetar(job->socketFd);

		if(respuestaPlanificacionMaster.idMensaje == mensajeTransformacionCompleta){
			bloqueNodo = (bloqueYNodo*) respuestaPlanificacionMaster.envio;
			log_trace(logger,"Finalizada tarea transformacion en bloque %d", bloqueNodo->bloque);

			agregarBloqueTerminadoATablaEstados(bloqueNodo->bloque,job->id,TRANSFORMACION);

			if(!faltanBloquesTransformacionParaNodo(job->id,bloqueNodo->workerId)){
				log_trace(logger,"Envio reduccion local de nodo %d", bloqueNodo->workerId);
				enviarReduccionLocalAMaster(job,bloqueNodo->workerId);
				actualizarCargasNodosRedLocal(job->id,bloqueNodo->workerId);
			}

		}
		else if(respuestaPlanificacionMaster.idMensaje == mensajeFalloTransformacion){
			bloqueNodo = (bloqueYNodo*) respuestaPlanificacionMaster.envio;
			log_trace(logger,"Entro a replanificar se desconecto un worker %d", bloqueNodo->workerId);
			replanificar(bloqueNodo,job,respuestaMaster,matrix,nodos);
		}
		else if(respuestaPlanificacionMaster.idMensaje == mensajeRedLocalCompleta){
			numNodo = *(int*) respuestaPlanificacionMaster.envio;
			log_trace(logger,"Finalizada tarea reduccion nodo %d", numNodo);
			agregarBloqueTerminadoATablaEstadosRedLocal(numNodo,job->id,RED_LOCAL);
			redLocalIncompleta= faltanMasTareas(job->id,RED_LOCAL);
		}
		else if(respuestaPlanificacionMaster.idMensaje == mensajeFalloReduccion){
			log_trace(logger,"Fallo en reduccion local del job %d", job->id);
			finalizarJob(job,RED_LOCAL);
		}
	}
}

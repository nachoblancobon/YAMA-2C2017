/*
 * Serializacion.c
 *
 *  Created on: 7/9/2017
 *      Author: utnso
 */
#include "Serializacion.h"

void empaquetar(int socket, int idMensaje,int tamanioS, void* paquete){
	header cabecera;
	cabecera.idMensaje = idMensaje;
	int tamanio;
	void* bloque;

	switch(idMensaje){
		case mensajeHandshake:
			tamanio = sizeof(int);
			bloque = malloc(sizeof(int));
			memcpy(bloque,paquete,sizeof(int));
			break;

		case mensajeInfoArchivo:
		case mensajeProcesarTransformacion:
		case mensajeDesignarWorker:
		case mensajeOk:
			tamanio =1;
			bloque = malloc(1);
			char a = 'a';
			memcpy(bloque,&a,1);
			break;

		case mensajeArchivo:
			bloque = serializarString(paquete,&tamanio);
			break;

		case mensajeSolicitudTransformacion:
			bloque = serializarSolicitudTransformacion(paquete,&tamanio);
			break;


		case mensajeInformacionNodo:
			tamanio = sizeof(informacionNodo);
			bloque = malloc(tamanio);
			memcpy(bloque,paquete,tamanio);
			break;

		case mensajeEnvioBloqueANodo:
			tamanio = tamanioS;
			bloque = malloc(tamanio);
			printf("size envio %d\n", tamanio);
			memcpy(bloque,paquete,tamanio);
			break;

		case mensajeRespuestaEnvioArchivoANodo:
		case mensajeRespuestaEnvioBloqueANodo:
			tamanio = sizeof(int);
			bloque = malloc(tamanio);
			memcpy(bloque,paquete,tamanio);
			break;

		case mensajeEnvioArchivoANodo:
			tamanio = tamanioS;
			bloque = malloc(tamanio);
			memcpy(bloque,paquete,tamanio);
			break;
	}

	cabecera.tamanio = tamanio;
	int desplazamiento =0;
	int tamanioTotal = 2* sizeof(int) + tamanio;
    void* buffer = malloc(tamanioTotal);

	memcpy(buffer , &cabecera, sizeof(header));
	desplazamiento += sizeof(header);
	memcpy(buffer + desplazamiento, bloque, tamanio);
	send(socket,buffer,tamanioTotal,0);
	free(bloque);
	free(buffer);
}

respuesta desempaquetar(int socket){
	void* bufferOk;
	respuesta miRespuesta;
	header* cabecera = malloc(sizeof(header));
	int bytesRecibidos;

	if ((bytesRecibidos = recv(socket, cabecera, sizeof(header), 0)) == 0) {
		miRespuesta.idMensaje = -1;
	}
	else {
		miRespuesta.size = cabecera->tamanio;
		miRespuesta.idMensaje = cabecera->idMensaje;
		switch (miRespuesta.idMensaje) {

			case mensajeEnvioBloqueANodo:
			case mensajeHandshake:
				bufferOk = malloc(sizeof(int));
				recv(socket, bufferOk, sizeof(int), 0);
				miRespuesta.envio = malloc(sizeof(int));
				memcpy(miRespuesta.envio, bufferOk, sizeof(int));
				free(bufferOk);
				break;

			case mensajeArchivo:
				miRespuesta.envio = deserializarString(socket,cabecera->tamanio);
				break;

			case mensajeInfoArchivo://todo
			case mensajeProcesarTransformacion://todo
			case mensajeDesignarWorker://todo
			case mensajeOk:
				bufferOk = malloc(sizeof(char));
				recv(socket,bufferOk,sizeof(char),0);
				free(bufferOk);
				break;

			case mensajeSolicitudTransformacion:
				miRespuesta.envio = deserializarSolicitudTransformacion(socket,cabecera->tamanio);
				break;

			case mensajeInformacionNodo:
				bufferOk = malloc(sizeof(informacionNodo));
				recv(socket,bufferOk,sizeof(informacionNodo),0);
				miRespuesta.envio = malloc(sizeof(informacionNodo));
				memcpy(miRespuesta.envio, bufferOk, sizeof(informacionNodo));
				free(bufferOk);
				break;

			case mensajeRespuestaEnvioArchivoANodo:
			case mensajeRespuestaEnvioBloqueANodo:
				bufferOk = malloc(sizeof(int));
				recv(socket,bufferOk,sizeof(int),0);
				miRespuesta.envio = malloc(sizeof(int));
				memcpy(miRespuesta.envio, bufferOk, sizeof(int));
				free(bufferOk);
				break;

			case mensajeEnvioArchivoANodo:
				bufferOk = malloc(cabecera->tamanio);
				recv(socket,bufferOk,cabecera->tamanio,0);
				miRespuesta.envio = malloc(cabecera->tamanio);
				memcpy(miRespuesta.envio, bufferOk, cabecera->tamanio);
				free(bufferOk);
				break;


		}
	}
	return miRespuesta;
}
//------SERIALIZACIONES PARTICULARES------//
void* serializarString(void* paquete,int *tamanio){
 	string* cadena = (string*)paquete;
 	int longitudInt = sizeof(int);
 	*tamanio = sizeof(int)+strlen(cadena->cadena)+1;
 	void* bloque = malloc(*tamanio);
 	int desplazamiento =0;

 	memcpy(bloque, &cadena->longitud, longitudInt);
 	desplazamiento += longitudInt;

 	memcpy(bloque+desplazamiento, cadena->cadena, cadena->longitud+1);

 	return bloque;
 }

string* deserializarString(int socket,int tamanio){
 	void* paquete = malloc(tamanio+1);
 	recv(socket,paquete,tamanio+1,0);
 	int longitudInt = sizeof(int);
 	string* cadena = malloc(sizeof(string));
 	int desplazamiento =0;

 	memcpy(&cadena->longitud,paquete, longitudInt);
 	desplazamiento += longitudInt;

 	cadena->cadena = malloc(cadena->longitud+1);

 	return cadena;
 }

void* serializarSolicitudTransformacion(void* paquete,int* tamanio){
	solicitudTransformacion* unaSolicitud = (solicitudTransformacion*)paquete;
	int desplazamiento = 0;

	*tamanio = sizeof(solicitudTransformacion) - (2* sizeof(int)) + unaSolicitud->rutaDatos.longitud + unaSolicitud->rutaResultado.longitud;
	void * buffer = malloc(*tamanio);

	memcpy(buffer + desplazamiento, &(unaSolicitud->rutaDatos.longitud), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(buffer + desplazamiento, unaSolicitud->rutaDatos.cadena, unaSolicitud->rutaDatos.longitud);
	desplazamiento += unaSolicitud->rutaDatos.longitud;

	memcpy(buffer + desplazamiento, &(unaSolicitud->rutaResultado.longitud), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(buffer + desplazamiento, unaSolicitud->rutaResultado.cadena, unaSolicitud->rutaResultado.longitud);
	desplazamiento += unaSolicitud->rutaResultado.longitud;

	return buffer;
}

solicitudTransformacion* deserializarSolicitudTransformacion(int socket,int tamanio){
	int desplazamiento = 0;
	solicitudTransformacion* unaSolicitud = malloc(sizeof(solicitudTransformacion));

	void* buffer = malloc(tamanio);
	recv(socket,buffer,tamanio,0);

	memcpy(&unaSolicitud->rutaDatos.longitud, buffer + desplazamiento, sizeof(int) );
	desplazamiento += sizeof(int);

	unaSolicitud->rutaDatos.cadena = malloc(unaSolicitud->rutaDatos.longitud+1);
	memcpy(unaSolicitud->rutaDatos.cadena, buffer + desplazamiento, unaSolicitud->rutaDatos.longitud);
	desplazamiento += unaSolicitud->rutaDatos.longitud;

	memcpy(&unaSolicitud->rutaResultado.longitud, buffer + desplazamiento, sizeof(int) );
	desplazamiento += sizeof(int);

	unaSolicitud->rutaResultado.cadena = malloc(unaSolicitud->rutaResultado.longitud+1);
	memcpy(unaSolicitud->rutaResultado.cadena, buffer + desplazamiento, unaSolicitud->rutaResultado.longitud);
	desplazamiento += unaSolicitud->rutaResultado.longitud;

	return unaSolicitud;
}

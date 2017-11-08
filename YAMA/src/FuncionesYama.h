/*
 * FuncionesYama.h
 *
 *  Created on: 24/9/2017
 *      Author: utnso
 */

#ifndef FUNCIONESYAMA_H_
#define FUNCIONESYAMA_H_

#include "Sockets.h"
#include "Configuracion.h"
#include <stdio.h>
#include <stdlib.h>
#include <Sockets.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <commons/log.h>
#include "Serializacion.h"
#include <Configuracion.h>
#include "Globales.h"
#include <commons/collections/list.h>
#include "Planificador.h"
#include <pthread.h>

#define idMaster 2
#define idDataNodes 3

pthread_mutex_t mutex_NodosConectados;
pthread_mutex_t mutexLog;
pthread_mutex_t mutexConfiguracion;
pthread_mutex_t mutexTablaEstados;
t_list* tablaDeEstados;
t_list* nodosConectados;

pthread_mutex_t cantJobs_mutex;

int servidor,socketFs;

struct sockaddr_in direccionCliente;

t_log* logger;
struct configuracionYama config;

uint32_t cantJobs;

int conectarseConFs();

void *manejarConexionMaster(void *cliente);

void manejarConfig();

void levantarServidorYama(char* ip, int port);

void verCambiosConfig();

void validarCambiosConfig();

void recibirContenidoMaster();

informacionArchivoFsYama* solicitarInformacionAFS(solicitudInfoNodos* solicitud);

int getDisponibilidadBase();

char* obtenerNombreArchivoResultadoTemporal();

int esClock();

void inicializarEstructuras();

bool** llenarMatrizNodosBloques(informacionArchivoFsYama* infoArchivo,int nodos,int bloques);

void calcularNodosYBloques(informacionArchivoFsYama* info,int* nodos,int*bloques);

void llenarListaNodos(t_list* listaNodos,informacionArchivoFsYama* infoArchivo);

void agregarBloqueANodo(t_list* listaNodos,ubicacionBloque ubicacion,int bloque);

char* dameUnNombreArchivoTemporal(int jobId,int numBloque,int etapa,int nodo);

void agregarBloqueTerminadoATablaEstados(int bloque,int jobId,int etapa);

bool faltanMasTareas(int jobid,int etapa);

void actualizarCargasNodos(int jobid,int etapa);

void finalizarJob(job* job, int etapa);

void borrarEntradasDeJob(int jobid);

void esperarRespuestaReduccionDeMaster(job* job,int etapa);

infoNodo* obtenerNodo(int numero);

void eliminarWorker(int id, t_list* listaNodos);

#endif /* FUNCIONESYAMA_H_ */

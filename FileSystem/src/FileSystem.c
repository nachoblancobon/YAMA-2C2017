/*
 ============================================================================
 Name        : FileSystem.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <Sockets.h>
#include <commons/log.h>
#include <commons/string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "Comandos.h"
#include <Configuracion.h>
#include "Sockets.c"
#include <commons/bitarray.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "FuncionesFS.h"

#define cantDataNodes 10

int cantBloques = 10;
int sizeBloque = 1048576; // 1mb
int mostrarLoggerPorPantalla = 1;
t_directory tablaDeDirectorios[100];
char* rutaArchivos = "metadata/Archivos/";
t_bitarray* bitmap[cantDataNodes];

int main(void) {

	int sizeComando = 256;
	int clienteYama = 0;
	int servidorFS = crearSocket();

	inicializarBitmaps();

	/*tablaDeDirectorios[0].index = 0;
	tablaDeDirectorios[1].index = 1;
	tablaDeDirectorios[2].index = 2;		//para probar cosas
	tablaDeDirectorios[3].index = 3;
	tablaDeDirectorios[0].padre = -1;
	tablaDeDirectorios[1].padre = 0;
	tablaDeDirectorios[2].padre = 1;
	tablaDeDirectorios[3].padre = 0;
	memcpy(tablaDeDirectorios[0].nombre,"hola",5);
	memcpy(tablaDeDirectorios[1].nombre,"chau",5);
	memcpy(tablaDeDirectorios[2].nombre,"bla",4);
	memcpy(tablaDeDirectorios[3].nombre,"bla",4);

	printf("\n\n %s", buscarRutaArchivo("hola/bla"));*/

	t_log* logger = log_create("logFileSystem", "FileSystem.c", mostrarLoggerPorPantalla, LOG_LEVEL_TRACE);

	levantarServidorFS(servidorFS, clienteYama);

	while (1) {
		printf("Introduzca comando: ");
		char* comando = malloc(sizeof(char) * sizeComando);
		bzero(comando, sizeComando);
		comando = readline(">");
		if (comando)
			add_history(comando);

		log_trace(logger, "El usuario ingreso: %s", comando);

		if (string_starts_with(comando, "format")) {
			log_trace(logger, "File system formateado");
		}
		else if (string_starts_with(comando, "rm -d")) {
			if (eliminarDirectorio(comando, 6) != -1)
				log_trace(logger, "Directorio eliminado");
			else
				log_trace(logger, "No se pudo eliminar el directorio");
		}
		else if (string_starts_with(comando, "rm -b")) {
			log_trace(logger, "Bloque eliminado");
		}
		else if (string_starts_with(comando, "rm")) {
			if (eliminarArchivo(comando, 3) != -1)
				log_trace(logger, "archivo eliminado");
			else
				log_trace(logger, "No se pudo eliminar el archivo");
		}
		else if (string_starts_with(comando, "rename")) {
			if (cambiarNombre(comando, 7) == 1)
				log_trace(logger, "Renombrado");
			else
				log_trace(logger, "No se pudo renombrar");

		}
		else if (string_starts_with(comando, "mv")) {
			if (mover(comando,3) == 1)
				log_trace(logger, "Archivo movido");
			else
				log_trace(logger, "No se pudo mover el archivo");
		}
		else if (string_starts_with(comando, "cat")) {
			if (mostrarArchivo(comando, 4) == 1){
			log_trace(logger, "Archivo mostrado");
			}else{
				log_trace(logger, "No se pudo mostrar el archivo");
			}
		}
		else if (string_starts_with(comando, "mkdir")) {
			if (crearDirectorio(comando,6) == 1){

			log_trace(logger, "Directorio creado");// avisar si ya existe
			}else{
				if (crearDirectorio(comando,6) == 2){
				log_trace(logger, "El directorio ya existe");
				}else{
					log_trace(logger, "No se pudo crear directorio");
				}
			}
		}
		else if (string_starts_with(comando, "cpfrom")) {
			log_trace(logger, "Archivo copiado a yamafs");
		}
		else if (string_starts_with(comando, "cpto")) {
			log_trace(logger, "Archivo copiado desde yamafs");
		}
		else if (string_starts_with(comando, "cpblock")) {
			log_trace(logger, "Bloque copiado en el nodo");
		}
		else if (string_starts_with(comando, "md5")) {
			log_trace(logger, "MD5 del archivo");
		}
		else if (string_starts_with(comando, "ls")) {
			listarArchivos(comando, 3);
			log_trace(logger, "Archivos listados");
		}
		else if (string_starts_with(comando, "info")) {
			if (informacion(comando,5) == 1)
				log_trace(logger, "Mostrando informacion del archivo");
			else
				log_trace(logger, "No se pudo mostrar informacion del archivo");
		}
		else {
			printf("Comando invalido\n");
			log_trace(logger, "Comando invalido");
		}
		free(comando);
	}
}


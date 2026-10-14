 /*
    INSTITUTO TECNOLÓGICO DE COSTA RICA
    Campus Tecnológico Central Cartago
    Escuela de Ingeniería en Computación

    Curso: IC-6600 Principios de Sistemas Operativos
    Periodo: II Semestre de 2026
    Profesor: Ing. Esteban Arias Méndez, M.Sc.

    Estudiante: José Julián Monge Brenes
    Carné: 2024247024

    Asignación: Tarea #3 - Mr. Meeseeks
    Fecha de entrega: 15 de octubre del 2026
 */ 

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_TEXTO 256

typedef enum {
    TIPO_TEXTO = 1,
    TIPO_CALCULO = 2,
    TIPO_PROGRAMA = 3
} TipoSolicitud;

typedef struct 
{
    TipoSolicitud tipo;
    char contenido[MAX_TEXTO];
} Solicitud;

static const char *nombre_tipo(TipoSolicitud tipo)
{
    switch (tipo)
    {
        case TIPO_TEXTO:
            return "Texto (simulacion)";
        case TIPO_CALCULO:
            return "Calculo aritmetico o logico";
        case TIPO_PROGRAMA:
            return "Programa externo";
        default:
            return "Desconocido";
    }
}

static void limpiar_espacios(char *texto) 
{
    char *inicio = texto;

    while (isspace((unsigned char)*inicio)) 
    {
        inicio++;
    }
    memmove(texto, inicio, strlen(inicio) + 1);

    size_t longitud = strlen(texto);
    while (longitud > 0 && isspace((unsigned char)texto[longitud - 1])) 
    {
        texto[--longitud] = '\0';
    }
}

static bool leer_solicitud(char **texto, size_t *capacidad) 
{
    printf("\nSolicitud: ");
    fflush(stdout);

    ssize_t leidos = getline(texto, capacidad, stdin);

    if (leidos == -1)
        return false;

    limpiar_espacios(*texto);

    if ((*texto)[0] == '\0' || strlen(*texto) >= MAX_TEXTO) 
        return false;

    return true;
}

static int leer_opcion(void)
{
    char *entrada = NULL;
    size_t capacidad = 0;

    printf("Opcion: ");
    fflush(stdout);

    ssize_t leidos = getline(&entrada, &capacidad, stdin);
    if (leidos == -1)
    {
        free(entrada);
        return -1;
    }

    limpiar_espacios(entrada);

    int opcion = -1;
    if (strlen(entrada) == 1 && entrada[0] >= '0' && entrada[0] <= '3')
    {
        opcion = entrada[0] - '0';
    }

    free(entrada);
    return opcion;
}

static int box(void)
{
    //char solicitud[256];
    //int total = 0;
    //int fallidas = 0;

    char *solicitud = NULL;
    size_t capacidad = 0;
    unsigned int total_solicitudes = 0;

    while(1)
    {
        puts("\n--- MENU PRINCIPAL ---");
        puts("1. Solicitud textual (simulacion)");
        puts("2. Calculo aritmetico o logico");
        puts("3. Ejecucion de programa externo");
        puts("0. Salir");

        int opcion = leer_opcion(); 

        if (opcion == -1)
        {
            puts("Opcion invalida. Seleccione un numero entre 0 y 3.");
            continue;
        } 
        else if (opcion == 0)
            break;

        TipoSolicitud tipo = (TipoSolicitud)opcion;

        bool estado = leer_solicitud(&solicitud, &capacidad);
        
        if (estado == false) 
        {
            puts("Error al ingresar la solicitud");
            continue;
        }

        total_solicitudes++;
        printf("Box: solicitud #%u recibida: \"%s\"\n", total_solicitudes, solicitud);
        printf("Box: tipo de solicitud: %s\n", nombre_tipo(tipo));
        puts("Box: pendiente de procesar.");
    }

    printf("\nTotal de solicitudes recibidas: %u\n", total_solicitudes);
    puts("Box: ¡Hasta luego!");
    free(solicitud);
    return 0;

}

int main(void)
{
    printf("=== Box de Mr. Meeseeks === \n");

    return box();
}

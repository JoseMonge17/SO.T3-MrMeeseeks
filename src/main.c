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
#include <math.h>
#include <time.h>

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
    double facilidad;
    bool facilidad_aleatoria;
    bool tiempo_aleatorio;
    double tiempo_segundos;
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

static bool leer_numero(const char *mensaje, double *numero)
{
    char *entrada = NULL;
    size_t capacidad = 0;

    while (1)
    {
        printf("%s", mensaje);
        fflush(stdout);

        if (getline(&entrada, &capacidad, stdin) == -1)
        {
            free(entrada);
            return false;
        }

        limpiar_espacios(entrada);
        char *fin = NULL;
        errno = 0;
        double valor = strtod(entrada, &fin);

        if (fin == entrada || *fin != '\0' ||
            errno == ERANGE || !isfinite(valor))
        {
            puts("Ingrese un numero valido (use punto para los decimales).");
            continue;
        }

        *numero = valor;
        free(entrada);
        return true;
    }
}

static bool capturar_datos_textuales(Solicitud *solicitud)
{
    puts("\n--- CONFIGURACION DE TAREA TEXTUAL ---");
    puts("Facilidad: 0 = imposible, 100 = muy facil.");
    puts("1. Ingresar facilidad manualmente");
    puts("2. Generar facilidad aleatoriamente");
    puts("0. Cancelar solicitud");

    while (1)
    {
        int opcion = leer_opcion();

        if (opcion == 0 || opcion == -2)
            return false;

        if (opcion == 1)
        {
            double facilidad;

            while (1)
            {
                if (!leer_numero("Facilidad (0 a 100): ", &facilidad))
                    return false;

                if (facilidad >= 0.0 && facilidad <= 100.0)
                    break;

                puts("La facilidad debe estar entre 0 y 100.");
            }

            solicitud->facilidad = facilidad;
            solicitud->facilidad_aleatoria = false;
            break;
        }

        if (opcion == 2)
        {
            /* Distribucion uniforme discreta provisional: 0, 1, ..., 100. */
            solicitud->facilidad = (double)(rand() % 101);
            solicitud->facilidad_aleatoria = true;
            break;
        }

        puts("Opcion invalida. Seleccione 0, 1 o 2.");
    }

    puts("\nDuracion de la simulacion:");
    puts("1. Aleatoria (entre 0.5 y 5 segundos)");
    puts("2. Definida por el usuario");
    puts("0. Cancelar solicitud");

    while (1)
    {
        int opcion = leer_opcion();

        if (opcion == 0 || opcion == -2)
            return false;

        if (opcion == 1)
        {
            /* Cada Meeseeks generara su propia duracion al ejecutarse. */
            solicitud->tiempo_aleatorio = true;
            solicitud->tiempo_segundos = 0.0;
            return true;
        }

        if (opcion == 2)
        {
            double segundos;

            while (1)
            {
                if (!leer_numero("Duracion en segundos (> 0): ", &segundos))
                    return false;

                if (segundos > 0.0)
                    break;

                puts("La duracion debe ser mayor que cero.");
            }

            solicitud->tiempo_aleatorio = false;
            solicitud->tiempo_segundos = segundos;
            return true;
        }

        puts("Opcion invalida. Seleccione 0, 1 o 2.");
    }
}


static void mostrar_solicitud(const Solicitud *solicitud, unsigned int numero)
{
    printf("Box: solicitud #%u recibida: \"%s\"\n",
           numero, solicitud->contenido);
    printf("Box: tipo de solicitud: %s\n", nombre_tipo(solicitud->tipo));

    if (solicitud->tipo == TIPO_TEXTO)
    {
        printf("Box: facilidad: %.2f/100 (%s)\n",
               solicitud->facilidad,
               solicitud->facilidad_aleatoria ? "aleatoria" : "manual");

        if (solicitud->tiempo_aleatorio)
        {
            puts("Box: duracion: aleatoria, de 0.5 a 5 segundos (por definir al ejecutar).");
        }
        else
        {
            printf("Box: duracion: %.2f segundos (manual)\n",
                   solicitud->tiempo_segundos);
        }
    }

    puts("Box: pendiente de procesar.");
}

static int box(void)
{
    //char solicitud[256];
    //int total = 0;
    //int fallidas = 0;

    //char *solicitud = NULL;
    //size_t capacidad = 0;
    //unsigned int total_solicitudes = 0;

    char *texto = NULL;
    size_t capacidad = 0;
    unsigned int total_solicitudes = 0;
    int resultado = EXIT_SUCCESS;

    /* La semilla se configura una sola vez durante la ejecucion de Box. */
    srand((unsigned int)time(NULL));

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

        if (!leer_solicitud(&texto, &capacidad))
        {
            if (feof(stdin) || ferror(stdin))
            {
                if (ferror(stdin))
                {
                    perror("Error al leer la solicitud");
                    resultado = EXIT_FAILURE;
                }
                break;
            }

            puts("Solicitud invalida: no puede estar vacia ni superar 255 caracteres.");
            continue;
        }

        Solicitud solicitud = {0};
        solicitud.tipo = (TipoSolicitud)opcion;
        strcpy(solicitud.contenido, texto);

        if (solicitud.tipo == TIPO_TEXTO &&
            !capturar_datos_textuales(&solicitud))
        {
            if (ferror(stdin))
            {
                perror("Error al configurar la solicitud");
                resultado = EXIT_FAILURE;
                break;
            }

            if (feof(stdin))
                break;

            puts("Box: solicitud cancelada.");
            continue;
        }

        total_solicitudes++;
        mostrar_solicitud(&solicitud, total_solicitudes);
    }

    printf("\nTotal de solicitudes recibidas: %u\n", total_solicitudes);
    puts("Box: ¡Hasta luego!");
    free(texto);
    return resultado;

}

int main(void)
{
    printf("=== Box de Mr. Meeseeks === \n");

    return box();
}

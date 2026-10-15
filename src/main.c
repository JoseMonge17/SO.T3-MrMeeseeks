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
#include <fenv.h>
#include <limits.h>
#include <signal.h>
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

#include "../vendor/tinyexpr/tinyexpr.h"

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

typedef enum
{
    RESULTADO_PENDIENTE,
    RESULTADO_COMPLETADO,
    RESULTADO_NO_COMPLETADO
} EstadoResultado;

/* Respuesta privada del Meeseeks; no contiene punteros a memoria del hijo. */
typedef struct
{
    EstadoResultado estado;
    pid_t pid;
    pid_t ppid;
    unsigned int nivel;
    unsigned int instancia;
    char detalle[MAX_TEXTO];
} Resultado;

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

/* El menu valida las opciones permitidas; aqui solo se exige un entero. */
static int leer_opcion(void)
{
    double numero;

    if (!leer_numero("Opcion: ", &numero))
        return -2;

    /* Evita truncar decimales o convertir valores fuera del rango de int. */
    if (numero < 0.0 || numero > INT_MAX || numero != (int)numero)
        return -1;

    return (int)numero;
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
    puts("1. Definida por el usuario");
    puts("2. Aleatoria (entre 0.5 y 5 segundos)");
    puts("0. Cancelar solicitud");

    while (1)
    {
        int opcion = leer_opcion();

        if (opcion == 0 || opcion == -2)
            return false;

        if (opcion == 1)
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

        if (opcion == 2)
        {
            /* Cada Meeseeks generara su propia duracion al ejecutarse. */
            solicitud->tiempo_aleatorio = true;
            solicitud->tiempo_segundos = 0.0;
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

static bool escribir_completo(int descriptor, const void *datos, size_t cantidad)
{
    const char *bytes = datos;
    size_t enviados = 0;

    while (enviados < cantidad)
    {
        ssize_t escritos = write(descriptor, bytes + enviados, cantidad - enviados);

        if (escritos == -1)
        {
            if (errno == EINTR)
                continue;
            return false;
        }

        if (escritos == 0)
        {
            errno = EIO;
            return false;
        }

        enviados += (size_t)escritos;
    }

    return true;
}

static bool leer_completo(int descriptor, void *datos, size_t cantidad)
{
    char *bytes = datos;
    size_t recibidos = 0;

    while (recibidos < cantidad)
    {
        ssize_t leidos = read(descriptor, bytes + recibidos, cantidad - recibidos);

        if (leidos == -1)
        {
            if (errno == EINTR)
                continue;
            return false;
        }

        if (leidos == 0)
        {
            /* El emisor cerro el pipe antes de completar el mensaje. */
            errno = EPIPE;
            return false;
        }

        recibidos += (size_t)leidos;
    }

    return true;
}

static void procesar_calculo(const char *expresion, Resultado *resultado)
{
    resultado->estado = RESULTADO_NO_COMPLETADO;

    const char *actual = expresion;
    while (*actual != '\0')
    {
        if (isdigit((unsigned char)*actual) || *actual == '.')
        {
            char *fin;
            (void)strtod(actual, &fin);
            if (fin != actual)
            {
                actual = fin;
                continue;
            }
        }

        if (isalpha((unsigned char)*actual) || *actual == '_')
        {
            snprintf(resultado->detalle, sizeof(resultado->detalle),
                     "Esta etapa admite numeros y operadores, sin funciones ni variables.");
            return;
        }

        actual++;
    }

    /* TinyExpr tambien evalua operaciones mientras compila la expresion. */
    if (feclearexcept(FE_ALL_EXCEPT) != 0)
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "No se pudo preparar la deteccion de errores numericos.");
        return;
    }

    int error = 0;
    errno = 0;
    double valor = te_interp(expresion, &error);
    int error_numerico = errno;
    int excepciones = fetestexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);

    if (error < 0)
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Memoria insuficiente para interpretar la expresion.");
    }
    else if (error > 0)
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Expresion invalida cerca de la posicion %d (contada desde 1).", error);
    }
    else if (excepciones & FE_DIVBYZERO)
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle), "Division por cero.");
    }
    else if ((excepciones & FE_INVALID) || error_numerico == EDOM)
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Operacion indefinida: por ejemplo 0/0, resto entre cero o potencia fuera de dominio.");
    }
    else if ((excepciones & FE_OVERFLOW) || error_numerico == ERANGE || !isfinite(valor))
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Resultado fuera del rango numerico admitido.");
    }
    else
    {
        resultado->estado = RESULTADO_COMPLETADO;
        snprintf(resultado->detalle, sizeof(resultado->detalle), "Resultado: %.15g", valor);
    }
}

/* Recibe su solicitud privada y devuelve el resultado a su padre. */
static int meeseeks(int entrada, int salida, unsigned int nivel, unsigned int instancia)
{
    Resultado resultado = {0};
    resultado.estado = RESULTADO_PENDIENTE;
    resultado.pid = getpid();
    resultado.ppid = getppid();
    resultado.nivel = nivel;
    resultado.instancia = instancia;

    printf("Hi, I'm Mr. Meeseeks! Look at Meeeee. (%ld, %ld, %u, %u)\n",
           (long)resultado.pid, (long)resultado.ppid, nivel, instancia);

    Solicitud solicitud = {0};
    bool recibida = leer_completo(entrada, &solicitud, sizeof(solicitud));
    int error_lectura = errno;
    close(entrada);

    if (!recibida)
    {
        fprintf(stderr, "Meeseeks (PID %ld, PPID %ld): error al recibir solicitud: %s.\n",
                (long)resultado.pid, (long)resultado.ppid, strerror(error_lectura));
        close(salida);
        return EXIT_FAILURE;
    }

    printf("Meeseeks (PID %ld, PPID %ld): recibi por pipe la solicitud \"%s\" (%s).\n",
           (long)resultado.pid, (long)resultado.ppid,
           solicitud.contenido, nombre_tipo(solicitud.tipo));

    if (solicitud.tipo == TIPO_CALCULO)
        procesar_calculo(solicitud.contenido, &resultado);

    bool enviado = escribir_completo(salida, &resultado, sizeof(resultado));
    int error_escritura = errno;
    close(salida);

    if (!enviado)
    {
        fprintf(stderr, "Meeseeks (PID %ld, PPID %ld): error al enviar resultado: %s.\n",
                (long)resultado.pid, (long)resultado.ppid, strerror(error_escritura));
        return EXIT_FAILURE;
    }

    printf("Meeseeks (PID %ld, PPID %ld): Hasta luego. Resultado enviado a la Box.\n",
           (long)resultado.pid, (long)resultado.ppid);
    return EXIT_SUCCESS;
}

static bool esperar_meeseeks(pid_t pid)
{
    int estado;
    pid_t esperado;

    do
    {
        esperado = waitpid(pid, &estado, 0);
    } while (esperado == -1 && errno == EINTR);

    if (esperado == -1)
    {
        perror("Error al esperar al Meeseeks");
        return false;
    }

    if (WIFSIGNALED(estado))
    {
        fprintf(stderr, "Box: Meeseeks PID %ld termino por la senal %d.\n",
                (long)pid, WTERMSIG(estado));
        return false;
    }

    if (!WIFEXITED(estado) || WEXITSTATUS(estado) != EXIT_SUCCESS)
    {
        fprintf(stderr, "Box: Meeseeks PID %ld termino con error.\n", (long)pid);
        return false;
    }

    return true;
}

/* Cada hijo recibe sus propios canales Box -> hijo e hijo -> Box. */
static bool crear_meeseeks(const Solicitud *solicitud, Resultado *resultado)
{
    /* Evita que el hijo vuelva a imprimir la salida pendiente de la Box. */
    if (fflush(stdout) == EOF)
    {
        perror("Error al mostrar la salida de la Box");
        return false;
    }

    int peticion[2];
    int respuesta[2];

    if (pipe(peticion) == -1)
    {
        perror("Error al crear el pipe de solicitud");
        return false;
    }

    if (pipe(respuesta) == -1)
    {
        perror("Error al crear el pipe de resultado");
        close(peticion[0]);
        close(peticion[1]);
        return false;
    }

    pid_t pid = fork();

    if (pid == -1)
    {
        perror("Error al crear el Meeseeks");
        close(peticion[0]);
        close(peticion[1]);
        close(respuesta[0]);
        close(respuesta[1]);
        return false;
    }

    if (pid == 0)
    {
        close(peticion[1]);
        close(respuesta[0]);
        int estado = meeseeks(peticion[0], respuesta[1], 1, 1);

        /* El hijo termina aqui: no debe continuar con el menu de la Box. */
        if (fflush(stdout) == EOF)
            estado = EXIT_FAILURE;
        _exit(estado);
    }

    close(peticion[0]);
    close(respuesta[1]);

    bool comunicado = escribir_completo(peticion[1], solicitud, sizeof(*solicitud));
    if (!comunicado)
        perror("Error al enviar la solicitud");
    close(peticion[1]);

    /* Recibe antes de waitpid: el hijo puede estar esperando espacio en el pipe. */
    if (comunicado)
    {
        comunicado = leer_completo(respuesta[0], resultado, sizeof(*resultado));
        if (!comunicado)
            perror("Error al recibir el resultado");
    }
    close(respuesta[0]);

    /* Recoge al hijo incluso cuando falla la comunicacion. */
    bool terminado = esperar_meeseeks(pid);
    return comunicado && terminado;
}

static void mostrar_resultado(const Resultado *resultado)
{
    printf("Box: resultado recibido por pipe de Meeseeks (PID %ld, PPID %ld, N %u, i %u).\n",
           (long)resultado->pid, (long)resultado->ppid,
           resultado->nivel, resultado->instancia);

    switch (resultado->estado)
    {
        case RESULTADO_PENDIENTE:
            puts("Box: solicitud recibida por el Meeseeks; pendiente de procesar.");
            break;
        case RESULTADO_COMPLETADO:
            puts("Box: solicitud completada.");
            break;
        case RESULTADO_NO_COMPLETADO:
            puts("Box: solicitud no completada.");
            break;
    }

    if (resultado->detalle[0] != '\0')
        printf("Box: %s\n", resultado->detalle);
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

    /* Un pipe sin lector debe producir EPIPE, sin terminar abruptamente la Box. */
    struct sigaction accion = {0};
    accion.sa_handler = SIG_IGN;
    sigemptyset(&accion.sa_mask);
    if (sigaction(SIGPIPE, &accion, NULL) == -1)
    {
        perror("Error al configurar SIGPIPE");
        return EXIT_FAILURE;
    }

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

        if (opcion == -2)
        {
            if (ferror(stdin))
            {
                perror("Error al leer la opcion");
                resultado = EXIT_FAILURE;
            }
            break;
        }

        if (opcion < 0 || opcion > 3)
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

        Resultado respuesta = {0};
        if (!crear_meeseeks(&solicitud, &respuesta))
        {
            resultado = EXIT_FAILURE;
            break;
        }

        mostrar_resultado(&respuesta);
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

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
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include "../vendor/tinyexpr/tinyexpr.h"

#define MAX_TEXTO 256
#define MAX_ARGUMENTOS 32
#define MAX_PROCESOS 256
#define PROCESOS_PREDETERMINADOS 128
#define PLAZO_PREDETERMINADO 300.0
#define PLAZO_MAXIMO 86400.0
#define INTERVALO_REVISION 0.01
#define MAX_HIJOS 5

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
    uint32_t semilla;
} Solicitud;

typedef enum
{
    RESULTADO_PENDIENTE,
    RESULTADO_COMPLETADO,
    RESULTADO_NO_COMPLETADO
} EstadoResultado;

typedef struct
{
    pid_t pid;
    pid_t ppid;
    unsigned int nivel;
    unsigned int instancia;
} IdentidadMeeseeks;

typedef enum
{
    AVISO_NINGUNO,
    AVISO_RESUELTO,
    AVISO_PLAZO,
    AVISO_ERROR,
    AVISO_INTERRUPCION
} MotivoAviso;

typedef struct
{
    MotivoAviso motivo;
    IdentidadMeeseeks ganador;
} AvisoCancelacion;

typedef struct
{
    pid_t pid;
    int envio;
    int recepcion;
} HijoMeeseeks;

/* Respuesta privada del Meeseeks; no contiene punteros a memoria del hijo. */
typedef struct
{
    EstadoResultado estado;
    pid_t pid;
    pid_t ppid;
    unsigned int nivel;
    unsigned int instancia;
    unsigned int meeseeks_creados;
    unsigned int profundidad_maxima;
    bool caos_planetario;
    bool presupuesto_agotado;
    bool interrumpida;
    unsigned int meeseeks_recogidos;
    unsigned int activos_al_final;
    IdentidadMeeseeks ganador;
    double tiempo_programa_segundos;
    double tiempo_espera_simulada_segundos;
    char detalle[MAX_TEXTO];
} Resultado;

typedef struct
{
    unsigned int max_procesos;
    double plazo_segundos;
    bool modo_experimental;
    double limite_extraordinario;
    uint32_t semilla;
} Configuracion;

/* Solo los contadores se comparten; las solicitudes siguen viajando por pipes. */
typedef struct
{
    sem_t mutex;
    unsigned int creados;
    unsigned int activos;
    unsigned int recogidos;
    unsigned int profundidad_maxima;
    Configuracion limites;
    struct timespec inicio;
} ControlCompartido;

typedef struct
{
    Solicitud solicitud;
    Resultado resultado;
    Configuracion configuracion;
    double tiempo_transcurrido_segundos;
} RegistroSolicitud;

static bool escribir_completo(int descriptor, const void *datos, size_t cantidad);
static bool leer_completo(int descriptor, void *datos, size_t cantidad);
static int meeseeks(int entrada, int salida, unsigned int nivel, unsigned int instancia,
                    ControlCompartido *control);

static void imprimir_linea(unsigned int nivel, const char *texto)
{
    char linea[1024];
    size_t sangria = nivel > 0 ? nivel - 1 : 0;
    if (sangria > MAX_PROCESOS)
        sangria = MAX_PROCESOS;
    memset(linea, '\t', sangria);
    int longitud = snprintf(linea + sangria, sizeof(linea) - sangria, "%s\n", texto);
    if (longitud >= 0 && (size_t)longitud < sizeof(linea) - sangria)
        (void)escribir_completo(STDOUT_FILENO, linea, sangria + (size_t)longitud);
}

static void mensaje_meeseeks(const Resultado *resultado, const char *formato, ...)
{
    char mensaje[640];
    va_list argumentos;
    va_start(argumentos, formato);
    vsnprintf(mensaje, sizeof(mensaje), formato, argumentos);
    va_end(argumentos);

    char texto[768];
    snprintf(texto, sizeof(texto), "Meeseeks (PID %ld, PPID %ld): %s",
             (long)resultado->pid, (long)resultado->ppid, mensaje);
    imprimir_linea(resultado->nivel, texto);
}

static void aplicar_cancelacion(Resultado *resultado, const AvisoCancelacion *aviso)
{
    resultado->estado = RESULTADO_NO_COMPLETADO;
    resultado->caos_planetario = aviso->motivo == AVISO_PLAZO;
    resultado->interrumpida = aviso->motivo == AVISO_INTERRUPCION;
    resultado->presupuesto_agotado = false;
    resultado->ganador = aviso->ganador;
    if (aviso->motivo == AVISO_RESUELTO)
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Trabajo cancelado: gano el Meeseeks PID %ld.", (long)aviso->ganador.pid);
    else if (aviso->motivo == AVISO_PLAZO)
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Caos planetario: se alcanzo el plazo de ejecucion.");
    else if (aviso->motivo == AVISO_INTERRUPCION)
        snprintf(resultado->detalle, sizeof(resultado->detalle), "Solicitud interrumpida por el usuario.");
    else
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Cancelacion por error o cierre del pipe del padre.");
}

static bool recibir_aviso(int entrada, AvisoCancelacion *aviso)
{
    if (!leer_completo(entrada, aviso, sizeof(*aviso)))
    {
        *aviso = (AvisoCancelacion){.motivo = AVISO_ERROR};
        return false;
    }
    if (aviso->motivo < AVISO_RESUELTO || aviso->motivo > AVISO_INTERRUPCION)
    {
        *aviso = (AvisoCancelacion){.motivo = AVISO_ERROR};
        errno = EPROTO;
        return false;
    }
    return true;
}

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

static const char *nombre_estado(EstadoResultado estado)
{
    switch (estado)
    {
        case RESULTADO_PENDIENTE:
            return "pendiente";
        case RESULTADO_COMPLETADO:
            return "completada";
        case RESULTADO_NO_COMPLETADO:
            return "no completada";
        default:
            return "desconocida";
    }
}

static double diferencia_segundos(const struct timespec *inicio, const struct timespec *fin)
{
    return (double)(fin->tv_sec - inicio->tv_sec) +
           (double)(fin->tv_nsec - inicio->tv_nsec) / 1000000000.0;
}

static double plazo_efectivo(const Configuracion *configuracion)
{
    return configuracion->modo_experimental ? configuracion->limite_extraordinario :
                                             configuracion->plazo_segundos;
}

static bool leer_variable_positiva(const char *nombre, double maximo, double *valor)
{
    const char *texto = getenv(nombre);
    if (texto == NULL)
        return true;
    char *fin;
    errno = 0;
    double numero = strtod(texto, &fin);
    if (fin == texto || *fin != '\0' || errno == ERANGE || !isfinite(numero) ||
        numero <= 0.0 || numero > maximo)
    {
        fprintf(stderr, "Configuracion invalida en %s.\n", nombre);
        return false;
    }
    *valor = numero;
    return true;
}

static bool cargar_configuracion(Configuracion *configuracion)
{
    *configuracion = (Configuracion){.max_procesos = PROCESOS_PREDETERMINADOS,
                                    .plazo_segundos = PLAZO_PREDETERMINADO};
    double procesos = configuracion->max_procesos;
    if (!leer_variable_positiva("MEESEEKS_MAX_PROCESOS", MAX_PROCESOS, &procesos) ||
        !leer_variable_positiva("MEESEEKS_PLAZO_SEGUNDOS", PLAZO_MAXIMO, &configuracion->plazo_segundos))
        return false;
    if (floor(procesos) != procesos)
    {
        fputs("MEESEEKS_MAX_PROCESOS debe ser entero.\n", stderr);
        return false;
    }
    configuracion->max_procesos = (unsigned int)procesos;

    const char *modo = getenv("MEESEEKS_EXPERIMENTAL");
    if (modo != NULL && strcmp(modo, "0") != 0 && strcmp(modo, "1") != 0)
    {
        fputs("MEESEEKS_EXPERIMENTAL debe ser 0 o 1.\n", stderr);
        return false;
    }
    configuracion->modo_experimental = modo != NULL && strcmp(modo, "1") == 0;
    const char *extra = getenv("MEESEEKS_LIMITE_EXTRAORDINARIO");
    if ((configuracion->modo_experimental && extra == NULL) ||
        (!configuracion->modo_experimental && extra != NULL))
    {
        fputs("El modo experimental requiere un limite extraordinario explicito y viceversa.\n", stderr);
        return false;
    }
    if (!leer_variable_positiva("MEESEEKS_LIMITE_EXTRAORDINARIO", PLAZO_MAXIMO,
                                &configuracion->limite_extraordinario))
        return false;
    if (configuracion->modo_experimental && configuracion->limite_extraordinario <= configuracion->plazo_segundos)
    {
        fputs("El limite extraordinario debe superar el plazo normal.\n", stderr);
        return false;
    }

    const char *semilla = getenv("MEESEEKS_SEMILLA");
    if (semilla != NULL)
    {
        if (*semilla == '\0' || strspn(semilla, "0123456789") != strlen(semilla))
        {
            fputs("MEESEEKS_SEMILLA debe ser un entero entre 0 y 4294967295.\n", stderr);
            return false;
        }
        char *fin;
        errno = 0;
        unsigned long long valor = strtoull(semilla, &fin, 10);
        if (*fin != '\0' || errno == ERANGE || valor > UINT32_MAX)
        {
            fputs("MEESEEKS_SEMILLA fuera de rango.\n", stderr);
            return false;
        }
        configuracion->semilla = (uint32_t)valor;
    }
    else
    {
        struct timespec ahora;
        if (clock_gettime(CLOCK_MONOTONIC, &ahora) == -1)
            return false;
        configuracion->semilla = (uint32_t)ahora.tv_nsec ^ (uint32_t)ahora.tv_sec ^
                                ((uint32_t)getpid() * 2654435761u);
    }
    return true;
}

static bool bloquear_control(ControlCompartido *control)
{
    int estado;
    do
    {
        estado = sem_wait(&control->mutex);
    } while (estado == -1 && errno == EINTR);
    return estado == 0;
}

static ControlCompartido *crear_control(const Configuracion *configuracion,
                                       const struct timespec *inicio)
{
    /* /dev/zero permite mmap compartido sin depender de MAP_ANONYMOUS. */
    int descriptor = open("/dev/zero", O_RDWR);
    if (descriptor == -1)
        return NULL;
    ControlCompartido *control = mmap(NULL, sizeof(*control), PROT_READ | PROT_WRITE,
                                     MAP_SHARED, descriptor, 0);
    int error = errno;
    close(descriptor);
    if (control == MAP_FAILED)
    {
        errno = error;
        return NULL;
    }

    *control = (ControlCompartido){0};
    control->limites = *configuracion;
    control->inicio = *inicio;
    if (sem_init(&control->mutex, 1, 1) == -1)
    {
        error = errno;
        munmap(control, sizeof(*control));
        errno = error;
        return NULL;
    }
    return control;
}

static double tiempo_disponible(const ControlCompartido *control)
{
    struct timespec ahora;
    if (clock_gettime(CLOCK_MONOTONIC, &ahora) == -1)
        return -1.0;
    double restante = plazo_efectivo(&control->limites) -
                      diferencia_segundos(&control->inicio, &ahora);
    return fmax(0.0, restante);
}

static bool reservar_meeseeks(ControlCompartido *control, unsigned int cantidad, bool *reservado)
{
    double restante = tiempo_disponible(control);
    if (restante < 0.0 || !bloquear_control(control))
        return false;

    /* Se reserva antes de fork; nunca se bloquea el semaforo durante fork o IPC. */
    *reservado = restante > 0.0 && cantidad <= control->limites.max_procesos - control->creados;
    if (*reservado)
    {
        control->creados += cantidad;
        control->activos += cantidad;
    }
    return sem_post(&control->mutex) == 0;
}

static bool liberar_meeseeks(ControlCompartido *control, unsigned int cantidad, bool fallo_fork)
{
    if (!bloquear_control(control))
        return false;
    control->activos -= cantidad;
    if (fallo_fork)
        control->creados -= cantidad;
    else
        control->recogidos += cantidad;
    return sem_post(&control->mutex) == 0;
}

static double facilidad_por_generacion(double facilidad, unsigned int nivel)
{
    if (facilidad == 0.0 || nivel == 1)
        return facilidad;
    return 100.0 - (100.0 - facilidad) * pow(0.8, (double)(nivel - 1));
}

static double probabilidad_resolver(double facilidad, unsigned int nivel)
{
    double efectiva = facilidad_por_generacion(facilidad, nivel);
    return efectiva > 85.0 ? 1.0 : pow(efectiva / 100.0, 2.0);
}

static unsigned int ayuda_necesaria(double facilidad)
{
    return facilidad > 85.0 ? 0 : (facilidad <= 45.0 ? 5 : 2);
}

static uint32_t derivar_semilla(uint32_t base, uint32_t instancia)
{
    uint32_t valor = base ^ (instancia * 0x9e3779b9u);
    valor ^= valor >> 16;
    valor *= 0x85ebca6bu;
    valor ^= valor >> 13;
    valor *= 0xc2b2ae35u;
    valor ^= valor >> 16;
    return valor;
}

static double aleatorio_uniforme(uint32_t *semilla)
{
    /* Xorshift32: secuencia definida en C, independiente del PID y de libc. */
    uint32_t valor = *semilla == 0 ? 0x6d2b79f5u : *semilla;
    valor ^= valor << 13;
    valor ^= valor >> 17;
    valor ^= valor << 5;
    *semilla = valor;
    return (double)valor / 4294967296.0;
}

static bool esperar_simulacion(double duracion, const ControlCompartido *control,
                              int entrada, double *espera_medida, AvisoCancelacion *aviso)
{
    struct timespec inicio;
    if (clock_gettime(CLOCK_MONOTONIC, &inicio) == -1)
        return false;

    bool correcto = true;
    struct timespec ahora = inicio;
    while (1)
    {
        double restante = tiempo_disponible(control);
        if (restante < 0.0)
        {
            correcto = false;
            break;
        }
        if (restante == 0.0)
        {
            aviso->motivo = AVISO_PLAZO;
            break;
        }
        double pendiente = duracion - diferencia_segundos(&inicio, &ahora);
        /* poll permite despertar por un aviso, sin esperar a que termine la pausa. */
        int milisegundos = pendiente <= 0.0 ? 0 : (int)ceil(fmin(pendiente, restante) * 1000.0);
        struct pollfd canal = {.fd = entrada, .events = POLLIN};
        int eventos = poll(&canal, 1, milisegundos);
        if (eventos == -1 && errno == EINTR)
        {
            if (clock_gettime(CLOCK_MONOTONIC, &ahora) == -1)
            {
                correcto = false;
                break;
            }
            continue;
        }
        if (eventos == -1 || (canal.revents & (POLLERR | POLLNVAL)))
        {
            correcto = false;
            break;
        }
        if (canal.revents & (POLLIN | POLLHUP))
        {
            correcto = recibir_aviso(entrada, aviso);
            break;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &ahora) == -1)
        {
            correcto = false;
            break;
        }
        if (diferencia_segundos(&inicio, &ahora) >= duracion)
        {
            restante = tiempo_disponible(control);
            if (restante < 0.0)
                correcto = false;
            else if (restante == 0.0)
                aviso->motivo = AVISO_PLAZO;
            break;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &ahora) == -1)
        return false;
    *espera_medida = diferencia_segundos(&inicio, &ahora);
    return correcto;
}

static bool procesar_texto(const Solicitud *solicitud, int entrada,
                          ControlCompartido *control, Resultado *resultado)
{
    uint32_t semilla = solicitud->semilla;
    double duracion = solicitud->tiempo_aleatorio ?
                      0.5 + 4.5 * aleatorio_uniforme(&semilla) : solicitud->tiempo_segundos;
    double probabilidad = probabilidad_resolver(solicitud->facilidad, resultado->nivel);
    mensaje_meeseeks(resultado, "facilidad efectiva %.9g, probabilidad %.9g%%, simulacion %.4f segundos.",
                     facilidad_por_generacion(solicitud->facilidad, resultado->nivel),
                     100.0 * probabilidad, duracion);

    AvisoCancelacion aviso = {0};
    if (!esperar_simulacion(duracion, control, entrada,
                           &resultado->tiempo_espera_simulada_segundos, &aviso))
        return false;
    if (aviso.motivo != AVISO_NINGUNO)
    {
        aplicar_cancelacion(resultado, &aviso);
        mensaje_meeseeks(resultado, "%s", resultado->detalle);
        return aviso.motivo != AVISO_ERROR;
    }

    if (solicitud->facilidad > 0.0 && aleatorio_uniforme(&semilla) < probabilidad)
    {
        resultado->estado = RESULTADO_COMPLETADO;
        resultado->ganador = (IdentidadMeeseeks){resultado->pid, resultado->ppid,
                                               resultado->nivel, resultado->instancia};
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Solicitud resuelta por el Meeseeks (PID %ld, PPID %ld, N %u, i %u).",
                 (long)resultado->pid, (long)resultado->ppid, resultado->nivel, resultado->instancia);
        mensaje_meeseeks(resultado, "Resolvi la solicitud; informare a mi padre por pipe.");
    }
    else
    {
        resultado->estado = RESULTADO_PENDIENTE;
        mensaje_meeseeks(resultado, "Necesito ayuda de %u hijos.", ayuda_necesaria(solicitud->facilidad));
    }
    return true;
}

static double tiempo_sin_simulacion(const RegistroSolicitud *registro)
{
    if (registro->tiempo_transcurrido_segundos < 0.0)
        return -1.0;

    double tiempo = registro->tiempo_transcurrido_segundos -
                    registro->resultado.tiempo_espera_simulada_segundos;
    return tiempo < 0.0 ? 0.0 : tiempo;
}

static bool registrar_solicitud(RegistroSolicitud **historial, size_t *cantidad,
                               const Solicitud *solicitud)
{
    if (*cantidad >= SIZE_MAX / sizeof(**historial))
    {
        errno = ENOMEM;
        return false;
    }

    RegistroSolicitud *nuevo = realloc(*historial, (*cantidad + 1) * sizeof(*nuevo));
    if (nuevo == NULL)
        return false;

    *historial = nuevo;
    RegistroSolicitud *registro = &nuevo[*cantidad];
    *registro = (RegistroSolicitud){0};
    registro->solicitud = *solicitud;
    registro->resultado.estado = RESULTADO_NO_COMPLETADO;
    registro->resultado.tiempo_programa_segundos = -1.0;
    registro->tiempo_transcurrido_segundos = -1.0;
    snprintf(registro->resultado.detalle, sizeof(registro->resultado.detalle),
             "Fallo al crear, comunicar o recoger el Meeseeks.");
    (*cantidad)++;
    return true;
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

static bool capturar_datos_textuales(Solicitud *solicitud, uint32_t *semilla)
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
            /* Distribucion uniforme sobre la escala, incluyendo decimales. */
            solicitud->facilidad = 100.0 * aleatorio_uniforme(semilla);
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


static void mostrar_solicitud(const Solicitud *solicitud, size_t numero)
{
    printf("Box: solicitud #%zu recibida: \"%s\"\n",
           numero, solicitud->contenido);
    printf("Box: tipo de solicitud: %s\n", nombre_tipo(solicitud->tipo));

    if (solicitud->tipo == TIPO_TEXTO)
    {
        printf("Box: facilidad: %.9g/100 (%s)\n",
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

static bool esperar_proceso(pid_t pid, int *estado)
{
    pid_t esperado;

    do
    {
        esperado = waitpid(pid, estado, 0);
    } while (esperado == -1 && errno == EINTR);

    return esperado != -1;
}

static bool terminar_programa(pid_t pid, int *estado)
{
    /* El grupo incluye los hijos normales del programa; el Meeseeks queda fuera. */
    (void)kill(-pid, SIGTERM);
    (void)kill(pid, SIGTERM);
    struct timespec gracia = {0, 50000000};
    while (nanosleep(&gracia, &gracia) == -1 && errno == EINTR)
    {
    }
    (void)kill(-pid, SIGKILL);
    (void)kill(pid, SIGKILL);
    return esperar_proceso(pid, estado);
}

static bool esperar_programa_limitado(pid_t pid, int *estado, int entrada,
                                     const ControlCompartido *control, AvisoCancelacion *aviso)
{
    while (1)
    {
        pid_t esperado = waitpid(pid, estado, WNOHANG);
        if (esperado == pid)
            return true;
        if (esperado == -1)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        double restante = tiempo_disponible(control);
        if (restante <= 0.0)
        {
            int error = errno;
            bool recogido = terminar_programa(pid, estado);
            aviso->motivo = restante == 0.0 ? AVISO_PLAZO : AVISO_ERROR;
            if (restante < 0.0)
                errno = error;
            return restante == 0.0 && recogido;
        }
        struct pollfd canal = {.fd = entrada, .events = POLLIN};
        int eventos = poll(&canal, 1, (int)ceil(fmin(INTERVALO_REVISION, restante) * 1000.0));
        if (eventos == -1 && errno == EINTR)
            continue;
        if (eventos == -1 || (canal.revents & (POLLERR | POLLNVAL)))
        {
            int error = errno;
            (void)terminar_programa(pid, estado);
            errno = error;
            return false;
        }
        if (canal.revents & (POLLIN | POLLHUP))
        {
            bool recibido = recibir_aviso(entrada, aviso);
            bool recogido = terminar_programa(pid, estado);
            return recibido && recogido && aviso->motivo != AVISO_ERROR;
        }
    }
}

static bool separar_argumentos(char *comando, char *argumentos[], size_t capacidad,
                              Resultado *resultado)
{
    char *lectura = comando;
    char *escritura = comando;
    size_t cantidad = 0;

    while (*lectura != '\0')
    {
        while (isspace((unsigned char)*lectura))
            lectura++;
        if (*lectura == '\0')
            break;

        if (cantidad + 1 >= capacidad)
        {
            snprintf(resultado->detalle, sizeof(resultado->detalle),
                     "Maximo %zu argumentos, incluido el programa.", capacidad - 1);
            return false;
        }

        argumentos[cantidad++] = escritura;
        char comilla = '\0';

        while (*lectura != '\0' && (comilla != '\0' || !isspace((unsigned char)*lectura)))
        {
            char actual = *lectura++;

            if (actual == '\\' && comilla != '\'')
            {
                if (*lectura == '\0')
                {
                    snprintf(resultado->detalle, sizeof(resultado->detalle),
                             "Escape incompleto al final del comando.");
                    return false;
                }
                *escritura++ = *lectura++;
            }
            else if (comilla != '\0' && actual == comilla)
            {
                comilla = '\0';
            }
            else if (comilla == '\0' && (actual == '\'' || actual == '"'))
            {
                comilla = actual;
            }
            else if (comilla == '\0' && strchr("|&;<>", actual) != NULL)
            {
                snprintf(resultado->detalle, sizeof(resultado->detalle),
                         "No se admiten operadores de shell sin comillas.");
                return false;
            }
            else
            {
                *escritura++ = actual;
            }
        }

        if (comilla != '\0')
        {
            snprintf(resultado->detalle, sizeof(resultado->detalle), "Comillas sin cerrar.");
            return false;
        }

        /* Avanza antes de escribir el terminador para no borrar un separador pendiente. */
        while (isspace((unsigned char)*lectura))
            lectura++;
        *escritura++ = '\0';
    }

    argumentos[cantidad] = NULL;
    if (cantidad == 0 || argumentos[0][0] == '\0')
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle), "Falta el nombre del programa.");
        return false;
    }

    return true;
}

static bool leer_error_ejecucion(int descriptor, int *error_ejecucion)
{
    ssize_t leidos;

    do
    {
        leidos = read(descriptor, error_ejecucion, sizeof(*error_ejecucion));
    } while (leidos == -1 && errno == EINTR);

    if (leidos == -1)
        return false;
    if (leidos == 0)
    {
        *error_ejecucion = 0;
        return true;
    }

    return leer_completo(descriptor, (char *)error_ejecucion + leidos,
                         sizeof(*error_ejecucion) - (size_t)leidos);
}

static void ejecutar_programa_hijo(char *const argumentos[], int errores, int respuesta, int cancelaciones)
{
    close(respuesta);
    close(cancelaciones);
    int error_ejecucion = setpgid(0, 0) == -1 ? errno : 0;

    /* El programa no debe consumir las siguientes opciones de la Box. */
    int entrada = open("/dev/null", O_RDONLY);
    if (entrada == -1)
        error_ejecucion = errno;
    else if (entrada != STDIN_FILENO)
    {
        if (dup2(entrada, STDIN_FILENO) == -1)
            error_ejecucion = errno;
        close(entrada);
    }

    sigset_t desbloquear;
    sigemptyset(&desbloquear);
    sigaddset(&desbloquear, SIGINT);
    sigaddset(&desbloquear, SIGTERM);
    if (sigprocmask(SIG_UNBLOCK, &desbloquear, NULL) == -1)
        error_ejecucion = errno;

    struct sigaction accion = {0};
    accion.sa_handler = SIG_DFL;
    sigemptyset(&accion.sa_mask);
    if (error_ejecucion == 0 && sigaction(SIGPIPE, &accion, NULL) == -1)
        error_ejecucion = errno;

    if (error_ejecucion == 0)
    {
        execvp(argumentos[0], argumentos);
        error_ejecucion = errno;
    }

    (void)escribir_completo(errores, &error_ejecucion, sizeof(error_ejecucion));
    close(errores);
    _exit(127);
}

static void procesar_programa(const char *contenido, int respuesta, int entrada,
                              ControlCompartido *control, Resultado *resultado)
{
    resultado->estado = RESULTADO_NO_COMPLETADO;
    char comando[MAX_TEXTO];
    strcpy(comando, contenido);
    char *argumentos[MAX_ARGUMENTOS + 1];

    if (!separar_argumentos(comando, argumentos, MAX_ARGUMENTOS + 1, resultado))
        return;

    if (fflush(stdout) == EOF)
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Error al mostrar la salida antes de ejecutar: %s.", strerror(errno));
        return;
    }

    int errores[2];
    if (pipe(errores) == -1)
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Error al crear el pipe de ejecucion: %s.", strerror(errno));
        return;
    }

    /* Un exec correcto cierra el escritor: EOF indica que no fallo el lanzamiento. */
    struct timespec inicio;
    if (fcntl(errores[1], F_SETFD, FD_CLOEXEC) == -1 ||
        clock_gettime(CLOCK_MONOTONIC, &inicio) == -1)
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Error al preparar la ejecucion: %s.", strerror(errno));
        close(errores[0]);
        close(errores[1]);
        return;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Error al crear el proceso del programa: %s.", strerror(errno));
        close(errores[0]);
        close(errores[1]);
        return;
    }

    if (pid == 0)
    {
        close(errores[0]);
        ejecutar_programa_hijo(argumentos, errores[1], respuesta, entrada);
    }

    close(errores[1]);
    /* Ambos procesos establecen el grupo para cubrir cualquiera de los dos ordenes. */
    if (setpgid(pid, pid) == -1 && errno != EACCES && errno != ESRCH)
    {
        int error = errno;
        int estado;
        (void)terminar_programa(pid, &estado);
        close(errores[0]);
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Error al crear el grupo del programa: %s.", strerror(error));
        return;
    }

    int estado;
    AvisoCancelacion aviso = {0};
    if (!esperar_programa_limitado(pid, &estado, entrada, control, &aviso))
    {
        int error = errno;
        close(errores[0]);
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Error al esperar el programa: %s.", strerror(error));
        return;
    }

    /* Solo se envia un errno: cabe en el pipe aun si esperamos antes de leer. */
    int error_ejecucion = 0;
    bool comunicado = leer_error_ejecucion(errores[0], &error_ejecucion);
    int error_comunicacion = errno;
    close(errores[0]);

    struct timespec fin;
    if (clock_gettime(CLOCK_MONOTONIC, &fin) == -1)
    {
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Error al medir el tiempo del programa: %s.", strerror(errno));
        return;
    }

    resultado->tiempo_programa_segundos = diferencia_segundos(&inicio, &fin);

    if (aviso.motivo != AVISO_NINGUNO)
        aplicar_cancelacion(resultado, &aviso);
    else if (!comunicado)
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Error al recibir el estado de ejecucion: %s.", strerror(error_comunicacion));
    else if (error_ejecucion != 0)
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "No se pudo lanzar el programa (errno %d): %s.",
                 error_ejecucion, strerror(error_ejecucion));
    else if (WIFSIGNALED(estado))
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "El programa termino por la senal %d.", WTERMSIG(estado));
    else if (WIFEXITED(estado))
    {
        int codigo = WEXITSTATUS(estado);
        resultado->estado = codigo == 0 ? RESULTADO_COMPLETADO : RESULTADO_NO_COMPLETADO;
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "El programa termino con codigo de salida %d.", codigo);
    }
    else
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "El programa termino con un estado inesperado.");
}

static bool iniciar_hijo(const Solicitud *solicitud, unsigned int nivel, unsigned int instancia,
                         int entrada_padre, int salida_padre, HijoMeeseeks hermanos[],
                         unsigned int cantidad_hermanos, HijoMeeseeks *hijo,
                         ControlCompartido *control)
{
    int peticion[2];
    int respuesta[2];
    *hijo = (HijoMeeseeks){.envio = -1, .recepcion = -1};
    if (pipe(peticion) == -1)
        return false;
    if (pipe(respuesta) == -1)
    {
        int error = errno;
        close(peticion[0]);
        close(peticion[1]);
        errno = error;
        return false;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        int error = errno;
        close(peticion[0]);
        close(peticion[1]);
        close(respuesta[0]);
        close(respuesta[1]);
        errno = error;
        return false;
    }
    if (pid == 0)
    {
        close(peticion[1]);
        close(respuesta[0]);
        /* El nieto no debe conservar canales de sus tios ni de sus abuelos. */
        if (entrada_padre >= 0)
            close(entrada_padre);
        if (salida_padre >= 0)
            close(salida_padre);
        for (unsigned int i = 0; i < cantidad_hermanos; i++)
        {
            if (hermanos[i].envio >= 0)
                close(hermanos[i].envio);
            if (hermanos[i].recepcion >= 0)
                close(hermanos[i].recepcion);
        }
        int estado = meeseeks(peticion[0], respuesta[1], nivel, instancia, control);
        if (fflush(stdout) == EOF)
            estado = EXIT_FAILURE;
        _exit(estado);
    }

    hijo->pid = pid;
    hijo->envio = peticion[1];
    hijo->recepcion = respuesta[0];
    close(peticion[0]);
    close(respuesta[1]);
    if (!escribir_completo(hijo->envio, solicitud, sizeof(*solicitud)))
    {
        int error = errno;
        close(hijo->envio);
        hijo->envio = -1;
        errno = error;
        return false;
    }
    return true;
}

static bool avisar_hijos(HijoMeeseeks hijos[], unsigned int cantidad,
                        const AvisoCancelacion *aviso, const Resultado *padre)
{
    bool correcto = true;
    for (unsigned int i = 0; i < cantidad; i++)
    {
        if (hijos[i].pid == 0 || hijos[i].envio < 0)
            continue;
        if (!escribir_completo(hijos[i].envio, aviso, sizeof(*aviso)))
        {
            /* Un hijo que ya envio su resultado puede haber cerrado el lector. */
            if (errno != EPIPE)
                correcto = false;
            close(hijos[i].envio);
            hijos[i].envio = -1;
        }
        else if (aviso->motivo == AVISO_RESUELTO)
            mensaje_meeseeks(padre, "Aviso por pipe a hijo PID %ld: gano PID %ld; deten tu subarbol.",
                             (long)hijos[i].pid, (long)aviso->ganador.pid);
        else
            mensaje_meeseeks(padre, "Aviso por pipe a hijo PID %ld: cancelar por %s.",
                             (long)hijos[i].pid, aviso->motivo == AVISO_PLAZO ? "plazo" :
                             (aviso->motivo == AVISO_INTERRUPCION ? "interrupcion" : "error"));
    }
    return correcto;
}

static bool recoger_hijo(HijoMeeseeks *hijo, ControlCompartido *control,
                         Resultado *respuesta, const Resultado *padre)
{
    Resultado mensaje = {0};
    bool recibido = leer_completo(hijo->recepcion, &mensaje, sizeof(mensaje));
    int error = errno;
    if (recibido)
        *respuesta = mensaje;
    close(hijo->recepcion);
    hijo->recepcion = -1;
    int estado = 0;
    bool recogido = esperar_proceso(hijo->pid, &estado);
    if (hijo->envio >= 0)
        close(hijo->envio);
    hijo->envio = -1;
    bool liberado = recogido && liberar_meeseeks(control, 1, false);
    mensaje_meeseeks(padre, "Recogi con waitpid al hijo PID %ld.", (long)hijo->pid);
    hijo->pid = 0;
    if (!recibido)
        mensaje_meeseeks(padre, "Error al recibir resultado del hijo: %s.", strerror(error));
    return recibido && recogido && liberado && WIFEXITED(estado) && WEXITSTATUS(estado) == EXIT_SUCCESS;
}

/* Cada padre supervisa solo sus hijos directos; cada hijo hace lo mismo con su subarbol. */
static bool esperar_hijos(int entrada, HijoMeeseeks hijos[], unsigned int cantidad,
                          ControlCompartido *control, Resultado *resultado,
                          AvisoCancelacion aviso)
{
    bool correcto = aviso.motivo != AVISO_ERROR;
    bool vigilar_padre = true;
    unsigned int pendientes = cantidad;
    double mayor_espera = 0.0;
    if (aviso.motivo != AVISO_NINGUNO)
    {
        aplicar_cancelacion(resultado, &aviso);
        if (!avisar_hijos(hijos, cantidad, &aviso, resultado))
            correcto = false;
    }

    while (pendientes > 0)
    {
        double restante = tiempo_disponible(control);
        if (aviso.motivo == AVISO_NINGUNO && restante <= 0.0)
        {
            aviso.motivo = restante == 0.0 ? AVISO_PLAZO : AVISO_ERROR;
            correcto = correcto && restante == 0.0;
            aplicar_cancelacion(resultado, &aviso);
            if (!avisar_hijos(hijos, cantidad, &aviso, resultado))
                correcto = false;
        }

        struct pollfd canales[MAX_HIJOS + 1] = {0};
        canales[0] = (struct pollfd){.fd = vigilar_padre ? entrada : -1, .events = POLLIN};
        for (unsigned int i = 0; i < cantidad; i++)
            canales[i + 1] = (struct pollfd){.fd = hijos[i].recepcion, .events = POLLIN};
        int plazo = aviso.motivo == AVISO_NINGUNO ? (int)ceil(restante * 1000.0) : -1;
        int eventos = poll(canales, cantidad + 1, plazo);
        if (eventos == -1 && errno == EINTR)
            continue;
        if (eventos == -1)
        {
            /* Aun si falla poll, se ordena el cierre y se recoge cada hijo. */
            correcto = false;
            aviso.motivo = AVISO_ERROR;
            aplicar_cancelacion(resultado, &aviso);
            (void)avisar_hijos(hijos, cantidad, &aviso, resultado);
            for (unsigned int i = 0; i < cantidad; i++)
            {
                if (hijos[i].pid == 0)
                    continue;
                Resultado respuesta = {0};
                (void)recoger_hijo(&hijos[i], control, &respuesta, resultado);
                mayor_espera = fmax(mayor_espera, respuesta.tiempo_espera_simulada_segundos);
            }
            break;
        }

        if (canales[0].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))
        {
            if (!recibir_aviso(entrada, &aviso))
                correcto = false;
            vigilar_padre = false;
            aplicar_cancelacion(resultado, &aviso);
            mensaje_meeseeks(resultado, "Recibi cancelacion de mi padre; aviso a mis descendientes.");
            if (aviso.motivo == AVISO_ERROR)
                correcto = false;
            if (!avisar_hijos(hijos, cantidad, &aviso, resultado))
                correcto = false;
        }

        for (unsigned int i = 0; i < cantidad; i++)
        {
            if (hijos[i].pid == 0 || canales[i + 1].revents == 0)
                continue;
            if (canales[i + 1].revents & (POLLERR | POLLNVAL))
            {
                correcto = false;
                aviso.motivo = AVISO_ERROR;
                aplicar_cancelacion(resultado, &aviso);
                (void)avisar_hijos(hijos, cantidad, &aviso, resultado);
            }
            Resultado respuesta = {0};
            bool recogido = recoger_hijo(&hijos[i], control, &respuesta, resultado);
            pendientes--;
            mayor_espera = fmax(mayor_espera, respuesta.tiempo_espera_simulada_segundos);
            if (!recogido)
            {
                correcto = false;
                aviso.motivo = AVISO_ERROR;
                aplicar_cancelacion(resultado, &aviso);
                (void)avisar_hijos(hijos, cantidad, &aviso, resultado);
            }
            else if (aviso.motivo == AVISO_NINGUNO && respuesta.estado == RESULTADO_COMPLETADO)
            {
                resultado->estado = RESULTADO_COMPLETADO;
                resultado->presupuesto_agotado = false;
                resultado->ganador = respuesta.ganador;
                snprintf(resultado->detalle, sizeof(resultado->detalle), "%s", respuesta.detalle);
                aviso = (AvisoCancelacion){AVISO_RESUELTO, respuesta.ganador};
                mensaje_meeseeks(resultado, "Hijo informo por pipe: gano PID %ld. Aviso a sus hermanos.",
                                 (long)respuesta.ganador.pid);
                if (!avisar_hijos(hijos, cantidad, &aviso, resultado))
                    correcto = false;
            }
            else if (aviso.motivo == AVISO_NINGUNO && respuesta.caos_planetario)
            {
                aviso.motivo = AVISO_PLAZO;
                aplicar_cancelacion(resultado, &aviso);
                if (!avisar_hijos(hijos, cantidad, &aviso, resultado))
                    correcto = false;
            }
        }
    }

    /* Las pausas de hermanos se superponen: se conserva el mayor camino de pausas. */
    resultado->tiempo_espera_simulada_segundos += mayor_espera;
    if (aviso.motivo == AVISO_NINGUNO)
    {
        resultado->estado = RESULTADO_NO_COMPLETADO;
        resultado->presupuesto_agotado = true;
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 "Ningun descendiente resolvio la solicitud dentro del presupuesto.");
    }
    return correcto;
}

static bool solicitar_ayuda(const Solicitud *solicitud, int entrada, int salida,
                           ControlCompartido *control, Resultado *resultado)
{
    unsigned int cantidad = ayuda_necesaria(solicitud->facilidad);
    bool reservado = false;
    if (!reservar_meeseeks(control, cantidad, &reservado))
        return false;
    if (!reservado)
    {
        double restante = tiempo_disponible(control);
        if (restante < 0.0)
            return false;
        AvisoCancelacion aviso = {.motivo = AVISO_PLAZO};
        if (restante == 0.0)
            aplicar_cancelacion(resultado, &aviso);
        else
        {
            resultado->estado = RESULTADO_NO_COMPLETADO;
            resultado->presupuesto_agotado = true;
            snprintf(resultado->detalle, sizeof(resultado->detalle),
                     "Presupuesto insuficiente para crear el grupo de %u hijos.", cantidad);
        }
        mensaje_meeseeks(resultado, "%s", resultado->detalle);
        return true;
    }

    HijoMeeseeks hijos[MAX_HIJOS];
    unsigned int creados = 0;
    bool correcto = true;
    for (unsigned int i = 0; i < cantidad; i++)
    {
        Solicitud solicitud_hijo = *solicitud;
        solicitud_hijo.semilla = derivar_semilla(solicitud->semilla, i + 1);
        bool iniciado = iniciar_hijo(&solicitud_hijo, resultado->nivel + 1, i + 1,
                                     entrada, salida, hijos, creados, &hijos[i], control);
        if (hijos[i].pid > 0)
        {
            creados++;
            mensaje_meeseeks(resultado, "Cree hijo PID %ld (N %u, i %u).",
                             (long)hijos[i].pid, resultado->nivel + 1, i + 1);
        }
        if (!iniciado)
        {
            mensaje_meeseeks(resultado, "Error al crear o enviar solicitud al hijo: %s.", strerror(errno));
            correcto = false;
            break;
        }
    }
    if (creados < cantidad && !liberar_meeseeks(control, cantidad - creados, true))
        correcto = false;
    AvisoCancelacion aviso = {.motivo = correcto ? AVISO_NINGUNO : AVISO_ERROR};
    return esperar_hijos(entrada, hijos, creados, control, resultado, aviso) && correcto;
}

/* Recibe su solicitud privada y devuelve el resultado a su padre. */
static int meeseeks(int entrada, int salida, unsigned int nivel, unsigned int instancia,
                    ControlCompartido *control)
{
    Resultado resultado = {0};
    resultado.estado = RESULTADO_PENDIENTE;
    resultado.pid = getpid();
    resultado.ppid = getppid();
    resultado.nivel = nivel;
    resultado.instancia = instancia;
    resultado.tiempo_programa_segundos = -1.0;
    if (!bloquear_control(control))
    {
        close(entrada);
        close(salida);
        return EXIT_FAILURE;
    }
    if (nivel > control->profundidad_maxima)
        control->profundidad_maxima = nivel;
    if (sem_post(&control->mutex) == -1)
    {
        close(entrada);
        close(salida);
        return EXIT_FAILURE;
    }

    char saludo[160];
    snprintf(saludo, sizeof(saludo), "Hi, I'm Mr. Meeseeks! Look at Meeeee. (%ld, %ld, %u, %u)",
             (long)resultado.pid, (long)resultado.ppid, nivel, instancia);
    imprimir_linea(nivel, saludo);

    Solicitud solicitud = {0};
    bool correcto = leer_completo(entrada, &solicitud, sizeof(solicitud));
    if (correcto)
    {
        mensaje_meeseeks(&resultado, "Recibi por pipe la solicitud \"%s\" (%s).",
                         solicitud.contenido, nombre_tipo(solicitud.tipo));
        if (solicitud.tipo == TIPO_TEXTO)
        {
            correcto = procesar_texto(&solicitud, entrada, control, &resultado);
            if (correcto && resultado.estado == RESULTADO_PENDIENTE)
                correcto = solicitar_ayuda(&solicitud, entrada, salida, control, &resultado);
        }
        else if (solicitud.tipo == TIPO_CALCULO)
            procesar_calculo(solicitud.contenido, &resultado);
        else if (solicitud.tipo == TIPO_PROGRAMA)
            procesar_programa(solicitud.contenido, salida, entrada, control, &resultado);

        if (resultado.estado == RESULTADO_COMPLETADO && resultado.ganador.pid == 0)
            resultado.ganador = (IdentidadMeeseeks){resultado.pid, resultado.ppid, nivel, instancia};
    }
    if (!correcto)
    {
        mensaje_meeseeks(&resultado, "Error en la solicitud o en sus descendientes; termino la atencion.");
        resultado.estado = RESULTADO_NO_COMPLETADO;
        snprintf(resultado.detalle, sizeof(resultado.detalle),
                 "Error interno; los hijos creados fueron recogidos.");
    }
    close(entrada);

    bool enviado = escribir_completo(salida, &resultado, sizeof(resultado));
    if (!enviado)
        mensaje_meeseeks(&resultado, "Error al enviar resultado: %s.", strerror(errno));
    close(salida);
    mensaje_meeseeks(&resultado, "Hasta luego. Descendientes recogidos; resultado enviado al padre.");
    return correcto && enviado ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool esperar_meeseeks(pid_t pid, bool *recogido)
{
    int estado;

    *recogido = esperar_proceso(pid, &estado);
    if (!*recogido)
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

static int consultar_interrupcion(void)
{
    sigset_t senales;
    sigemptyset(&senales);
    sigaddset(&senales, SIGINT);
    sigaddset(&senales, SIGTERM);
    struct timespec sin_espera = {0};
    int signo = sigtimedwait(&senales, NULL, &sin_espera);
    if (signo == -1 && (errno == EAGAIN || errno == EINTR))
        return 0;
    return signo;
}

static bool recibir_resultado_raiz(HijoMeeseeks *hijo, Resultado *resultado,
                                  const ControlCompartido *control)
{
    bool aviso_enviado = false;
    bool plazo_normal_notificado = false;
    while (1)
    {
        double restante = tiempo_disponible(control);
        if (restante < 0.0)
            return false;
        int signo = consultar_interrupcion();
        if (signo == -1)
            return false;
        double transcurrido = plazo_efectivo(&control->limites) - restante;
        if (control->limites.modo_experimental && !plazo_normal_notificado &&
            transcurrido >= control->limites.plazo_segundos)
        {
            printf("Box: plazo normal superado; modo experimental activo hasta %.3f segundos.\n",
                   control->limites.limite_extraordinario);
            fflush(stdout);
            plazo_normal_notificado = true;
        }
        if (!aviso_enviado && (signo != 0 || restante == 0.0))
        {
            AvisoCancelacion aviso = {.motivo = signo != 0 ? AVISO_INTERRUPCION : AVISO_PLAZO};
            if (!escribir_completo(hijo->envio, &aviso, sizeof(aviso)) && errno != EPIPE)
                return false;
            aviso_enviado = true;
            puts(signo != 0 ? "Box: interrupcion recibida; cancelando y recogiendo el arbol." :
                             "Box: limite efectivo alcanzado; cancelando y recogiendo el arbol.");
            fflush(stdout);
        }
        double pausa = aviso_enviado ? 0.1 : fmin(0.1, restante);
        if (control->limites.modo_experimental && !plazo_normal_notificado)
            pausa = fmin(pausa, fmax(0.0, control->limites.plazo_segundos - transcurrido));
        struct pollfd canal = {.fd = hijo->recepcion, .events = POLLIN};
        int eventos = poll(&canal, 1, (int)ceil(pausa * 1000.0));
        if (eventos == -1 && errno == EINTR)
            continue;
        if (eventos == -1 || (canal.revents & (POLLERR | POLLNVAL)))
            return false;
        if (canal.revents & (POLLIN | POLLHUP))
            return leer_completo(hijo->recepcion, resultado, sizeof(*resultado));
    }
}

/* La Box usa los mismos canales privados que los padres del arbol. */
static bool crear_meeseeks(const Solicitud *solicitud, Resultado *resultado,
                           ControlCompartido *control)
{
    if (fflush(stdout) == EOF)
    {
        perror("Error al mostrar la salida de la Box");
        return false;
    }
    bool reservado = false;
    if (!reservar_meeseeks(control, 1, &reservado))
        return false;
    if (!reservado)
    {
        resultado->estado = RESULTADO_NO_COMPLETADO;
        double restante = tiempo_disponible(control);
        if (restante < 0.0)
            return false;
        resultado->caos_planetario = restante == 0.0;
        resultado->presupuesto_agotado = restante > 0.0;
        snprintf(resultado->detalle, sizeof(resultado->detalle),
                 restante == 0.0 ? "Caos planetario: plazo agotado antes de fork." :
                 "Presupuesto de procesos agotado.");
        return true;
    }

    HijoMeeseeks hijo;
    bool comunicado = iniciar_hijo(solicitud, 1, 1, -1, -1, NULL, 0, &hijo, control);
    if (hijo.pid == 0)
    {
        perror("Error al crear el Meeseeks");
        (void)liberar_meeseeks(control, 1, true);
        return false;
    }
    resultado->pid = hijo.pid;
    resultado->ppid = getpid();
    resultado->nivel = 1;
    resultado->instancia = 1;
    if (!comunicado)
    {
        perror("Error al enviar la solicitud");
        /* EOF tambien cancela un subarbol si se pierde su padre. */
        if (hijo.envio >= 0)
            close(hijo.envio);
        hijo.envio = -1;
    }

    Resultado respuesta = {0};
    bool recibido = comunicado && recibir_resultado_raiz(&hijo, &respuesta, control);
    if (!recibido)
    {
        perror("Error al recibir el resultado");
        if (hijo.envio >= 0)
            close(hijo.envio);
        hijo.envio = -1;
    }
    close(hijo.recepcion);
    bool recogido = false;
    bool terminado = esperar_meeseeks(hijo.pid, &recogido);
    if (hijo.envio >= 0)
        close(hijo.envio);
    bool liberado = recogido && liberar_meeseeks(control, 1, false);
    if (recibido)
        *resultado = respuesta;
    return comunicado && recibido && terminado && liberado;
}

static bool procesar_solicitud(RegistroSolicitud *registro, const Configuracion *configuracion)
{
    registro->configuracion = *configuracion;
    struct timespec inicio;
    if (clock_gettime(CLOCK_MONOTONIC, &inicio) == -1)
    {
        perror("Error al iniciar la medicion de la solicitud");
        snprintf(registro->resultado.detalle, sizeof(registro->resultado.detalle),
                 "No se pudo iniciar la medicion de la solicitud.");
        return false;
    }

    ControlCompartido *control = crear_control(configuracion, &inicio);
    if (control == NULL)
    {
        perror("Error al crear memoria compartida o semaforo");
        return false;
    }
    sigset_t bloqueo, anteriores;
    sigemptyset(&bloqueo);
    sigaddset(&bloqueo, SIGINT);
    sigaddset(&bloqueo, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &bloqueo, &anteriores) == -1)
    {
        perror("Error al preparar cancelacion por senales");
        sem_destroy(&control->mutex);
        munmap(control, sizeof(*control));
        return false;
    }
    bool procesada = crear_meeseeks(&registro->solicitud, &registro->resultado, control);
    /* La Box consume las senales pendientes antes de restaurar su mascara. */
    while (consultar_interrupcion() > 0)
    {
    }
    if (sigprocmask(SIG_SETMASK, &anteriores, NULL) == -1)
    {
        perror("Error al restaurar la mascara de senales");
        procesada = false;
    }
    /* El hijo ya fue recogido: solo la Box destruye los recursos compartidos. */
    registro->resultado.meeseeks_creados = control->creados;
    registro->resultado.profundidad_maxima = control->profundidad_maxima;
    registro->resultado.meeseeks_recogidos = control->recogidos;
    registro->resultado.activos_al_final = control->activos;
    if (control->activos != 0 || control->recogidos != control->creados)
    {
        fputs("Box: contador de procesos activos inconsistente.\n", stderr);
        procesada = false;
    }
    if (sem_destroy(&control->mutex) == -1)
    {
        perror("Error al destruir el semaforo");
        procesada = false;
    }
    if (munmap(control, sizeof(*control)) == -1)
    {
        perror("Error al liberar memoria compartida");
        procesada = false;
    }

    struct timespec fin;
    if (clock_gettime(CLOCK_MONOTONIC, &fin) == -1)
    {
        perror("Error al terminar la medicion de la solicitud");
        return false;
    }

    registro->tiempo_transcurrido_segundos = diferencia_segundos(&inicio, &fin);
    return procesada;
}

static void mostrar_configuracion_registro(const RegistroSolicitud *registro)
{
    const Configuracion *configuracion = &registro->configuracion;
    printf("Modo: %s. Plazo normal: %.3f s. Limite efectivo: %.3f s. Presupuesto: %u.\n",
           configuracion->modo_experimental ? "experimental" : "normal",
           configuracion->plazo_segundos, plazo_efectivo(configuracion), configuracion->max_procesos);
    printf("Semilla base: %u. Semilla solicitud: %u. Supero duracion normal: %s.\n",
           (unsigned int)configuracion->semilla, (unsigned int)registro->solicitud.semilla,
           registro->tiempo_transcurrido_segundos > configuracion->plazo_segundos ? "si" : "no");
}

static void mostrar_resultado(const Resultado *resultado)
{
    if (resultado->pid > 0)
        printf("Box: Meeseeks asociado (PID %ld, PPID %ld, N %u, i %u).\n",
               (long)resultado->pid, (long)resultado->ppid,
               resultado->nivel, resultado->instancia);
    else
        puts("Box: no se creo un Meeseeks para esta solicitud.");

    if (resultado->estado == RESULTADO_COMPLETADO)
        printf("Box: ganador (PID %ld, PPID %ld, N %u, i %u).\n",
               (long)resultado->ganador.pid, (long)resultado->ganador.ppid,
               resultado->ganador.nivel, resultado->ganador.instancia);
    printf("Box: solicitud %s.\n", nombre_estado(resultado->estado));
    printf("Box: Meeseeks creados: %u. Recogidos: %u. Activos al cerrar: %u. Profundidad maxima: %u.\n",
           resultado->meeseeks_creados, resultado->meeseeks_recogidos,
           resultado->activos_al_final, resultado->profundidad_maxima);

    if (resultado->detalle[0] != '\0')
        printf("Box: %s\n", resultado->detalle);
}

static void mostrar_tiempos(const RegistroSolicitud *registro)
{
    if (registro->tiempo_transcurrido_segundos >= 0.0)
    {
        printf("Box: tiempo transcurrido de la solicitud: %.6f segundos.\n",
               registro->tiempo_transcurrido_segundos);
        printf("Box: espera artificial en la rama mas larga: %.6f segundos.\n",
               registro->resultado.tiempo_espera_simulada_segundos);
        printf("Box: tiempo ajustado por pausas de la rama: %.6f segundos.\n",
               tiempo_sin_simulacion(registro));
    }
    else
    {
        puts("Box: medicion de la solicitud no disponible.");
    }

    if (registro->resultado.tiempo_programa_segundos >= 0.0)
        printf("Box: tiempo del programa externo: %.6f segundos.\n",
               registro->resultado.tiempo_programa_segundos);
}

static void mostrar_informe(const RegistroSolicitud historial[], size_t cantidad)
{
    size_t completadas = 0;
    size_t no_completadas = 0;
    size_t pendientes = 0;
    size_t caos = 0;
    size_t recursos = 0;
    size_t interrumpidas = 0;
    size_t extendidas = 0;

    for (size_t i = 0; i < cantidad; i++)
    {
        caos += historial[i].resultado.caos_planetario;
        recursos += historial[i].resultado.presupuesto_agotado;
        interrumpidas += historial[i].resultado.interrumpida;
        extendidas += historial[i].configuracion.modo_experimental &&
                      historial[i].tiempo_transcurrido_segundos > historial[i].configuracion.plazo_segundos;
        switch (historial[i].resultado.estado)
        {
            case RESULTADO_COMPLETADO:
                completadas++;
                break;
            case RESULTADO_NO_COMPLETADO:
                no_completadas++;
                break;
            default:
                pendientes++;
                break;
        }
    }

    puts("\n=== INFORME DE SOLICITUDES ===");
    printf("Total de solicitudes recibidas: %zu\n", cantidad);
    printf("Completadas: %zu\nNo completadas: %zu\nPendientes: %zu\n",
           completadas, no_completadas, pendientes);
    printf("Caos planetario: %zu. Fin por presupuesto: %zu. Interrumpidas: %zu.\n", caos, recursos, interrumpidas);
    printf("Experimentales que superaron duracion normal: %zu.\n", extendidas);

    for (size_t i = 0; i < cantidad; i++)
    {
        const RegistroSolicitud *registro = &historial[i];
        printf("\nSolicitud #%zu (%s): \"%s\"\n", i + 1,
               nombre_tipo(registro->solicitud.tipo), registro->solicitud.contenido);
        mostrar_configuracion_registro(registro);
        printf("Estado: %s. Meeseeks creados: %u. Recogidos: %u. Activos al cerrar: %u.\n",
               nombre_estado(registro->resultado.estado), registro->resultado.meeseeks_creados,
               registro->resultado.meeseeks_recogidos, registro->resultado.activos_al_final);
        printf("Profundidad maxima: %u. Caos planetario: %s.\n",
               registro->resultado.profundidad_maxima,
               registro->resultado.caos_planetario ? "si" : "no");
        if (registro->resultado.pid > 0)
            printf("Meeseeks inicial: (PID %ld, PPID %ld, N %u, i %u).\n",
                   (long)registro->resultado.pid, (long)registro->resultado.ppid,
                   registro->resultado.nivel, registro->resultado.instancia);
        if (registro->resultado.estado == RESULTADO_COMPLETADO)
            printf("Ganador: (PID %ld, PPID %ld, N %u, i %u).\n",
                   (long)registro->resultado.ganador.pid, (long)registro->resultado.ganador.ppid,
                   registro->resultado.ganador.nivel, registro->resultado.ganador.instancia);
        if (registro->resultado.detalle[0] != '\0')
            printf("Detalle: %s\n", registro->resultado.detalle);
        mostrar_tiempos(registro);
    }
}

static int box(void)
{
    char *texto = NULL;
    size_t capacidad = 0;
    RegistroSolicitud *historial = NULL;
    size_t total_solicitudes = 0;
    int resultado = EXIT_SUCCESS;
    Configuracion configuracion;
    if (!cargar_configuracion(&configuracion))
        return EXIT_FAILURE;
    printf("Box: limites por solicitud: %u Meeseeks, %.3f segundos.\n",
           configuracion.max_procesos, configuracion.plazo_segundos);

    /* Un pipe sin lector debe producir EPIPE, sin terminar abruptamente la Box. */
    struct sigaction accion = {0};
    accion.sa_handler = SIG_IGN;
    sigemptyset(&accion.sa_mask);
    if (sigaction(SIGPIPE, &accion, NULL) == -1)
    {
        perror("Error al configurar SIGPIPE");
        return EXIT_FAILURE;
    }

    uint32_t semilla_captura = configuracion.semilla;
    printf("Box: modo %s; limite efectivo %.3f segundos; semilla base %u.\n",
           configuracion.modo_experimental ? "experimental" : "normal",
           plazo_efectivo(&configuracion), (unsigned int)configuracion.semilla);

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
            !capturar_datos_textuales(&solicitud, &semilla_captura))
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

        solicitud.semilla = derivar_semilla(configuracion.semilla, (uint32_t)(total_solicitudes + 1));
        if (!registrar_solicitud(&historial, &total_solicitudes, &solicitud))
        {
            perror("Error al reservar el registro de la solicitud");
            resultado = EXIT_FAILURE;
            break;
        }

        mostrar_solicitud(&solicitud, total_solicitudes);

        RegistroSolicitud *registro = &historial[total_solicitudes - 1];
        bool procesada = procesar_solicitud(registro, &configuracion);
        mostrar_resultado(&registro->resultado);
        mostrar_tiempos(registro);

        if (!procesada)
        {
            resultado = EXIT_FAILURE;
            break;
        }
    }

    mostrar_informe(historial, total_solicitudes);
    puts("Box: ¡Hasta luego!");
    free(texto);
    free(historial);
    return resultado;

}

int main(void)
{
    printf("=== Box de Mr. Meeseeks === \n");

    return box();
}

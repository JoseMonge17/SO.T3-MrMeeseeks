# Tarea #3 — Mr. Meeseeks

**José Julián Monge Brenes · 2024247024**

IC-6600 Principios de Sistemas Operativos · II semestre de 2026

Este proyecto representa una Box que atiende solicitudes usando procesos reales
de Linux. El desarrollo se organiza por etapas, con el código propio en
`src/main.c` y un `main()` pequeño que delega en `box()`.

## Compilar y ejecutar en Ubuntu

Se necesita GCC, disponible con `sudo apt install build-essential`. TinyExpr ya
viene incluido en `vendor/tinyexpr` y no requiere otra instalación. Desde la
carpeta del repositorio, crear la carpeta de salida una vez con `mkdir -p build`
y compilar:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DTE_POW_FROM_RIGHT src/main.c vendor/tinyexpr/tinyexpr.c -pthread -lm -o build/meeseeks
```

- `-std=c11`: selecciona el estándar de C.
- `-Wall -Wextra -Wpedantic`: activan advertencias.
- `-DTE_POW_FROM_RIGHT`: agrupa potencias de derecha a izquierda.
- `-pthread`: habilita el soporte de sincronización POSIX, incluidos los semáforos.
- `-lm`: enlaza matemáticas y el entorno de punto flotante.
- `-o build/meeseeks`: establece el ejecutable de salida.

TinyExpr genera algunas advertencias; los avisos de `src/main.c` deben revisarse
por separado. No se debe usar `-ffast-math` porque afecta la detección de errores
numéricos. Si compila correctamente, ejecutar `./build/meeseeks`.

Es necesario recompilar cada vez que cambia el código C. Para editar el README
o repetir una corrida, no hace falta. Copiar el comando en una sola línea evita
problemas con espacios al pegarlo. Este programa necesita Linux; MinGW no incluye estas
interfaces de procesos y memoria compartida.

## Avance paso a paso

1. Menú y captura validada de las tres clases de solicitudes.
2. Creación del primer Meeseeks con `fork()`, identidad y espera con `waitpid()`.
3. Pipes privados para enviar solicitudes y recibir resultados completos.
4. Integración de TinyExpr para evaluar cálculos y comunicar sus errores.
5. Ejecución de programas externos con argumentos, estados de salida y tiempo real.
6. Medición por solicitud e historial con informe al salir.
7. Simulación textual individual y definición de la probabilidad y el aging.
8. Memoria compartida, semáforo, presupuesto y plazo de ejecución.

## Organización del código

`TipoSolicitud` identifica el caso y `Solicitud` guarda contenido y configuración.
`Resultado` guarda estado, identidad, tiempos y datos para el informe.
`RegistroSolicitud` conserva la petición, el resultado y su tiempo total.
`Configuracion` contiene los límites; `ControlCompartido` contiene el semáforo,
contadores y el inicio del plazo. Las solicitudes siguen viajando por pipes.

| Parte | Funciones principales |
| --- | --- |
| Control y lectura | `main()`, `box()`, `limpiar_espacios()`, `leer_solicitud()`, `leer_numero()`, `leer_opcion()`. |
| Configuración | `capturar_datos_textuales()` pide facilidad y duración: 1 manual, 2 aleatorio. `cargar_configuracion()` lee los límites. |
| Presentación | `nombre_tipo()`, `nombre_estado()`, `mostrar_solicitud()`, `mostrar_resultado()`. |
| Procesos y pipes | `crear_meeseeks()`, `meeseeks()`, `leer_completo()`, `escribir_completo()`, `esperar_proceso()`, `esperar_meeseeks()`. Las transferencias completan datos parciales y reintentan ante `EINTR`. |
| Cálculos | `procesar_calculo()` valida la expresión y usa TinyExpr. |
| Programas | `separar_argumentos()`, `procesar_programa()`, `ejecutar_programa_hijo()`, `leer_error_ejecucion()`, `esperar_programa_limitado()`, `terminar_programa()`. |
| Simulación | `facilidad_por_generacion()`, `probabilidad_resolver()`, `ayuda_necesaria()`, `aleatorio_uniforme()`, `esperar_simulacion()`, `procesar_texto()`. |
| Sincronización | `crear_control()`, `bloquear_control()`, `reservar_meeseeks()`, `liberar_meeseeks()`, `tiempo_disponible()`. |
| Tiempos e historial | `procesar_solicitud()`, `diferencia_segundos()`, `tiempo_sin_simulacion()`, `mostrar_tiempos()`, `registrar_solicitud()`, `mostrar_informe()`. |

## Bibliotecas y solicitudes

El programa usa C estándar e interfaces POSIX para procesos, pipes, señales,
memoria y sincronización. `math.h` y `fenv.h` ayudan a detectar errores numéricos;
`clock_gettime(CLOCK_MONOTONIC)` mide tiempos sin depender de cambios en el reloj.

La única biblioteca externa es [TinyExpr](https://github.com/codeplea/tinyexpr).
Su código y licencia zlib se conservan en `vendor/tinyexpr`, revisión
`7ab30db449a359e8af77fbb062ab2ec7ee686d62`. Todo el código propio está en `src/main.c`.

- **Texto:** facilidad decimal entre 0 y 100, manual o uniforme aleatoria. Duración
  positiva manual o uniforme entre 0.5 y 5 segundos, generada por el Meeseeks.
  Cancelar no registra una solicitud.
- **Cálculos:** números, paréntesis, `+ - * / % ^`, comparaciones
  `< <= > >= == !=` y lógica `&& || !`. La evaluación usa `double`, punto decimal
  y resultados lógicos 0 o 1. No admite funciones ni variables. `2^3^2` da 512 y
  `-2^2` da -4. No hay cortocircuito: `0 && (1/0)` también falla. La comprobación
  de errores comienza antes de interpretar porque TinyExpr puede evaluar
  mientras compila la expresión.
- **Programas:** hasta 32 argumentos contando el ejecutable, comillas y escapes.
  No se expanden variables ni comodines y se rechazan operadores de shell sin comillas.
  Un Meeseeks crea el proceso del programa con `execvp()` y lo recoge. Su entrada
  es `/dev/null`. Un pipe con `FD_CLOEXEC` distingue un fallo de lanzamiento de
  una salida real con código 127. El Meeseeks comprueba si hubo salida correcta,
  código de error o terminación por señal.

## Probabilidad y evolución

`F` representa la facilidad original y `N` el nivel, empezando en 1:

```text
F = 0: facilidad_efectiva = 0 en todos los niveles.
F > 0: facilidad_efectiva = 100 - (100 - F) * 0.8^(N - 1).
P = 1 si facilidad_efectiva > 85; en otro caso, P = (facilidad_efectiva / 100)^2.
```

La idea es que cada generación reduzca un 20% de la dificultad que queda. El
cuadrado hace menos probable resolver algo difícil al primer intento: con F=25,
P=6.25%; con F=45, P=20.25%; con F=85, P=72.25%. Desde 85.01 el único Meeseeks
resuelve siempre, siempre que termine la simulación antes del plazo. F=0 nunca
se resuelve, ni por aging ni por un sorteo favorable.

Después de esperar, el Meeseeks compara un sorteo uniforme en [0,1) con P. Usa
un estado local de `rand_r()`, iniciado con reloj, PID y nivel para evitar heredar
la misma secuencia después de `fork()`. No es un generador criptográfico.

Si falla, la ayuda se calcula con F original: 5 hijos para F<=45 y 2 para
45<F<=85. **La creación de esos hijos todavía está pendiente:** la solicitud
queda pendiente si hay presupuesto suficiente o no completada si no alcanza
para el grupo mínimo. Las funciones de aging ya reciben el nivel, pero las
generaciones reales se agregarán con el árbol. Cada proceso realiza un solo
intento; las generaciones corresponden a niveles del árbol.

## Sincronización y límites

La Box reserva `ControlCompartido` con `mmap(MAP_SHARED)` sobre `/dev/zero`, antes
del primer `fork()`. El semáforo vive dentro de esa memoria y se inicia con
`sem_init(..., 1, 1)`: es compartido entre procesos, no una copia privada.

El semáforo protege los contadores de creados, activos y profundidad. Antes de
`fork()` se reserva un cupo, que se devuelve si falla. Tras recoger al hijo, se
reduce el contador de activos; creados conserva el total acumulado, por lo que
terminar un proceso no renueva el presupuesto. El programa externo no cuenta
como Meeseeks. El semáforo se libera antes de `fork()`, esperas o comunicación.
La Box destruye el semáforo y libera el mapa cuando el hijo ya terminó.
Se usan semáforos POSIX en vez de System V para mantener juntos el contador y su
sincronización, sin identificadores IPC persistentes.

Los límites se configuran mediante variables de entorno:

| Variable | Predeterminado | Valores admitidos |
| --- | --- | --- |
| `MEESEEKS_MAX_PROCESOS` | 128 por solicitud | Entero de 1 a 256. |
| `MEESEEKS_PLAZO_SEGUNDOS` | 300 segundos (5 minutos) | Positivo, hasta 86400. |

El plazo comienza antes de preparar los recursos y crear al primer Meeseeks;
incluye la espera simulada. La simulación revisa el plazo entre pausas de hasta
10 ms; la latencia real también depende del planificador. Al agotarse informa
caos planetario y devuelve un resultado no completado.
Para un programa externo, el Meeseeks consulta `waitpid(..., WNOHANG)`: al vencer
el plazo envía `SIGTERM` a su grupo y, tras 50 ms, `SIGKILL`; recoge al hijo con
`waitpid()`.
Esto controla programas que permanecen en su grupo; no es un supervisor de daemons
que se separen con `setsid()`.

Se pueden usar plazos cortos para comprobar la contingencia o ampliar el límite para
una prueba larga sin quitar la protección de procesos:

```bash
MEESEEKS_PLAZO_SEGUNDOS=900 MEESEEKS_MAX_PROCESOS=64 ./build/meeseeks
```

Esta configuración permite hasta 15 minutos. Las pruebas del árbol difícil que
exceda los 5 minutos quedan para cuando exista el árbol; este comando no
representa un experimento ya realizado.

## Medición y pruebas

El programa mide el tiempo transcurrido con `clock_gettime(CLOCK_MONOTONIC)`.
También mide el intervalo de simulación durante el que espera con `nanosleep()`
y lo resta del total para obtener el tiempo sin espera artificial. No es tiempo
de CPU. Si el plazo interrumpe la simulación, se descuenta solo la espera medida.
El programa externo conserva su propia duración, incluido el lanzamiento y la
recolección. El sondeo puede agregar un intervalo de revisión y demoras del
planificador a esa medición. Un tiempo en -1 significa que no está disponible.
Al agregar hermanos concurrentes habrá que evitar sumar pausas simultáneas.
El informe incluye estados, tiempos, creados y profundidad.

Después de compilar, este comando prueba las tres opciones y una cancelación:

```bash
printf '%s\n' 2 '5+3*2' 2 '1/0' 3 'pwd' 1 'Texto facil' 1 100 1 0.05 1 'Cancelada' 0 0 | ./build/meeseeks
```

Se esperan 4 solicitudes: 3 completadas y 1 no completada. Cada una crea un Meeseeks;
solo la textual tiene espera artificial. Para imposibilidad y presupuesto mínimo:

```bash
printf '%s\n' 1 'Imposible' 1 0 1 0.05 0 | MEESEEKS_MAX_PROCESOS=1 ./build/meeseeks
```

Debe quedar no completada por presupuesto, nunca resuelta. Para verificar el
plazo sin esperar cinco minutos, estos comandos prueban la simulación y un programa:

```bash
printf '%s\n' 1 'Plazo' 1 100 1 10 0 | MEESEEKS_PLAZO_SEGUNDOS=0.05 ./build/meeseeks
printf '%s\n' 3 'sleep 10' 0 | MEESEEKS_PLAZO_SEGUNDOS=0.05 ./build/meeseeks
```

Ambas deben informar caos y regresar al menú. Son resultados esperados para
comprobar en Ubuntu, no resultados experimentales medidos. Otras comprobaciones
incluyen entradas inválidas, `false`, salida 127, terminación por señal y
ejecutable inexistente.

## Pendiente

Faltan el árbol recursivo, la resolución y cancelación en cascada por pipes y el
control conjunto de todos sus descendientes. El presupuesto está preparado,
pero todavía se consume solo el cupo inicial. Luego se ampliarán las estadísticas
y se realizarán los experimentos y el reporte. Este resumen se actualiza al
terminar cada etapa.

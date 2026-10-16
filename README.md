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
9. Árbol recursivo con grupos completos de hijos y tabulaciones por nivel.
10. Resolución y cancelación en cascada por pipes, ganador y recolección del árbol.
11. Modo experimental con límite extraordinario, cancelación por señales e informe completo.
12. Reutilización de captura, mensajes, cierres de canales e impresión de resultados.

## Organización del código

`TipoSolicitud` identifica el caso y `Solicitud` guarda contenido y configuración.
`Resultado` guarda estado, identidad inicial, identidad del ganador y tiempos.
`AvisoCancelacion` comunica resolución, plazo o error; `HijoMeeseeks` guarda el
PID y los descriptores de cada hijo directo.
`RegistroSolicitud` conserva la petición, el resultado y su tiempo total.
`Configuracion` contiene los límites, el modo y la semilla base; `ControlCompartido` contiene el semáforo,
contadores y el inicio del plazo. Las solicitudes siguen viajando por pipes.

| Parte | Funciones principales |
| --- | --- |
| Control y lectura | `main()`, `box()`, `limpiar_espacios()`, `leer_solicitud()`, `leer_numero()`, `leer_opcion()`, `leer_modalidad()` y `leer_numero_en_rango()`. |
| Configuración | `capturar_datos_textuales()` pide facilidad y duración: 1 manual, 2 aleatorio. `cargar_configuracion()` lee los límites. |
| Presentación | `nombre_tipo()`, `nombre_estado()`, `mostrar_solicitud()`, `mostrar_resultado()` y `establecer_detalle()`. |
| Procesos y pipes | `crear_meeseeks()`, `iniciar_hijo()`, `meeseeks()`, `leer_completo()`, `escribir_completo()`, `esperar_proceso()`, `esperar_meeseeks()`. Las transferencias completan datos parciales y reintentan ante `EINTR`; `cerrar_descriptor()` cierra e invalida un canal. |
| Árbol y cascada | `solicitar_ayuda()`, `esperar_hijos()`, `avisar_hijos()`, `recibir_aviso()`, `aplicar_cancelacion()`, `recoger_hijo()` y `cancelar_hijos()` coordinan los hijos directos y propagan los mensajes. |
| Secuencia | `imprimir_linea()` y `mensaje_meeseeks()` muestran mensajes con PID, PPID y una tabulación por nivel adicional. |
| Cálculos | `procesar_calculo()` valida la expresión y usa TinyExpr. |
| Programas | `separar_argumentos()`, `procesar_programa()`, `ejecutar_programa_hijo()`, `leer_error_ejecucion()`, `esperar_programa_limitado()`, `terminar_programa()`. |
| Simulación | `facilidad_por_generacion()`, `probabilidad_resolver()`, `ayuda_necesaria()`, `aleatorio_uniforme()`, `esperar_simulacion()`, `procesar_texto()`. |
| Sincronización | `crear_control()`, `bloquear_control()`, `reservar_meeseeks()`, `liberar_meeseeks()`, `tiempo_disponible()`. |
| Tiempos e historial | `procesar_solicitud()`, `diferencia_segundos()`, `tiempo_sin_simulacion()`, `mostrar_tiempos()`, `registrar_solicitud()`, `mostrar_informe()`. |
| Contingencia | `plazo_efectivo()`, `consultar_interrupcion()` y `recibir_resultado_raiz()` supervisan el plazo y la cancelación. |

La captura trata Ctrl+D como fin de entrada y muestra el informe antes de salir.
Los errores de lectura se notifican en un solo punto de `box()`. Se mantienen los
controles necesarios para validar entradas, comunicar mensajes completos, limitar
los procesos y recoger a los hijos.

## Bibliotecas y solicitudes

El programa usa C estándar e interfaces POSIX para procesos, pipes, señales,
memoria y sincronización. `math.h` y `fenv.h` ayudan a detectar errores numéricos;
`clock_gettime(CLOCK_MONOTONIC)` mide tiempos sin depender de cambios en el reloj.

La única biblioteca externa es [TinyExpr](https://github.com/codeplea/tinyexpr).
Su código y licencia zlib se conservan en `vendor/tinyexpr`, revisión
`7ab30db449a359e8af77fbb062ab2ec7ee686d62`. Todo el código propio está en `src/main.c`.

- **Texto:** facilidad decimal entre 0 y 100, manual o uniforme aleatoria. Duración
  positiva manual o uniforme entre 0.5 y 5 segundos, generada por el Meeseeks.
  Cancelar durante la captura no registra una solicitud; interrumpirla cuando
  ya está ejecutándose sí queda registrado.
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
P=6.25%; con F=45, P=20.25%; con F=85, P=72.25%. Para F>85 el único Meeseeks
resuelve siempre, siempre que termine la simulación antes del plazo. F=0 nunca
se resuelve, ni por aging ni por un sorteo favorable.

Después de esperar, el Meeseeks compara un sorteo uniforme en [0,1) con P.
El generador es `rand()` de C. La Box usa `srand()` al iniciar y cada Meeseeks
lo reinicializa con la semilla recibida por pipe antes de simular. La fórmula
`semilla_hijo = semilla_padre * 6 + instancia` diferencia las rutas sin mezclar
bits ni constantes hexadecimales.

`MEESEEKS_SEMILLA=123 ./build/meeseeks` permite repetir los sorteos con la misma
biblioteca de C y las mismas entradas. Sin esa variable se usa la hora actual.
La planificación todavía puede cambiar cuántos procesos se crean y qué ganador
se recibe primero; una semilla fija no reproduce los tiempos ni el planificador.

Si falla, la ayuda se calcula con F original: 5 hijos para F<=45 y 2 para
45<F<=85. Cada hijo recibe la misma solicitud por su pipe privado, con nivel
N+1 e instancia de 1 a 5 o de 1 a 2 según su padre. Cada proceso realiza un solo
intento; el aging se aplica al nivel real del árbol.

## Árbol y resolución en cascada

Cada padre conserva dos pipes por hijo: uno envía la solicitud inicial y luego
avisos de cancelación; el otro recibe el resultado estructurado. `poll()` permite
atender a cualquiera de los hijos y recibir avisos del propio padre. Durante la
pausa también se usa `poll()`, de modo que un aviso puede interrumpirla.

Cuando un hijo informa una solución, el padre conserva la identidad del ganador
y avisa por pipe a los otros hijos. Estos propagan el aviso a sus descendientes,
interrumpen el trabajo, se despiden y terminan. Cada padre lee los resultados y
recoge **todos** sus hijos con `waitpid()` antes de devolver el resultado hacia
arriba. Finalmente, la Box recoge al Meeseeks inicial e informa quién resolvió.
Si coinciden varias soluciones, cada padre conserva la primera que recibe;
el ganador definitivo es el que llega por la rama elegida hasta la Box.

El plazo y los errores también producen avisos que recorren el árbol hacia
abajo. Si se cierra inesperadamente el pipe del padre, el hijo cancela su
subárbol. Al crear un nieto se cierran los canales heredados de sus tíos y
abuelos, para que no mantengan pipes abiertos por accidente. Los mensajes usan
una tabulación por cada nivel adicional, sin dibujar un árbol en la terminal;
las líneas de ramas concurrentes pueden alternarse.

## Sincronización y límites

La Box reserva `ControlCompartido` con `mmap(MAP_SHARED)` sobre `/dev/zero`, antes
del primer `fork()`. El semáforo vive dentro de esa memoria y se inicia con
`sem_init(..., 1, 1)`: es compartido entre procesos, no una copia privada.

El semáforo protege los contadores de creados, activos y profundidad. Antes de
`fork()` se reserva el grupo completo de 5 o 2 cupos bajo el mismo semáforo
(uno para el Meeseeks inicial). Si no alcanza, esa rama termina sin crear un
grupo incompleto. Si falla un pipe o `fork()`, se devuelven los cupos sin usar y
se cancela y recoge lo que ya se creó. Tras recoger a cada hijo, se reduce el
contador de activos; creados conserva el total acumulado, por lo que
terminar un proceso no renueva el presupuesto. El programa externo no cuenta
como Meeseeks. El semáforo se libera antes de `fork()`, esperas o comunicación.
La Box destruye el semáforo y libera el mapa cuando todo el árbol ya terminó.
Se usan semáforos POSIX en vez de System V para mantener juntos el contador y su
sincronización, sin identificadores IPC persistentes.

Los límites se configuran mediante variables de entorno:

| Variable | Predeterminado | Valores admitidos |
| --- | --- | --- |
| `MEESEEKS_MAX_PROCESOS` | 128 por solicitud | Entero de 1 a 256. |
| `MEESEEKS_PLAZO_SEGUNDOS` | 300 segundos (5 minutos) | Positivo, hasta 86400. |
| `MEESEEKS_EXPERIMENTAL` | 0 | 0 normal, 1 experimental. |
| `MEESEEKS_LIMITE_EXTRAORDINARIO` | Sin definir | Obligatorio solo en modo experimental, mayor que el plazo normal y hasta 86400 s. |
| `MEESEEKS_SEMILLA` | Hora actual | Entero positivo hasta `UINT_MAX`; opcional para repetir sorteos. |

El plazo comienza antes de preparar los recursos y crear al primer Meeseeks;
incluye la espera simulada. En modo normal se aplica el plazo normal. El modo
experimental continúa después de ese plazo y aplica el límite extraordinario
explícito; mantiene el mismo presupuesto de procesos. La Box anuncia cuando se supera el plazo normal. `poll()` espera
como máximo el tiempo restante y puede despertar antes por un mensaje. Al agotarse el plazo, el árbol se cancela
y recoge, se informa caos planetario y la Box termina mostrando el informe. La planificación, la salida en terminal
y la recolección pueden hacer que el tiempo observado supere el plazo; no es
una garantía de tiempo real estricto. Si antes se agota el presupuesto sin
solución, termina como no completada por recursos, sin esperar al caos.
Para un programa externo, el Meeseeks consulta `waitpid(..., WNOHANG)`: al vencer
el plazo envía `SIGTERM` a su grupo y, tras 50 ms, `SIGKILL`; recoge al hijo con
`waitpid()`.
Esto controla programas que permanecen en su grupo; no es un supervisor de daemons
que se separen con `setsid()`.

Se pueden usar plazos cortos para comprobar la contingencia o ampliar el límite para
una prueba larga sin quitar la protección de procesos:

```bash
MEESEEKS_EXPERIMENTAL=1 MEESEEKS_LIMITE_EXTRAORDINARIO=900 MEESEEKS_MAX_PROCESOS=64 ./build/meeseeks
```

Esta configuración permite hasta 15 minutos, con presupuesto 64. No desactiva
los límites. Durante una solicitud, Ctrl+C o SIGTERM a la Box se convierten en
avisos por pipe: el árbol se cancela y recoge, y los programas externos reciben
terminación y se recogen. Las señales se esperan con `sigtimedwait()` mientras
están bloqueadas, sin variables globales para comunicar solicitudes o avisos.
Al volver al menú se restaura la máscara original.

## Medición e informe

El programa mide el tiempo transcurrido con `clock_gettime(CLOCK_MONOTONIC)` y
mide las pausas realmente ejecutadas, incluidas las interrumpidas por cancelación.
En cada nodo se suma su pausa al máximo de las pausas acumuladas de sus hijos.
Así, la Box recibe **la mayor suma de pausas en una rama**, sin sumar como
consecutivas las pausas simultáneas de los hermanos.

El tiempo ajustado es el transcurrido menos esa suma. Es una medida de tiempo
de pared ajustada por una rama; no es tiempo de CPU ni una suma del trabajo de
todos los procesos. El programa externo conserva su propia duración, incluido
el lanzamiento y la recolección. Su sondeo puede agregar un intervalo de revisión
y demoras del planificador. Un tiempo en -1 significa que no está disponible.
El informe incluye el total de solicitudes, completadas y no completadas. Para
cada solicitud muestra contenido, tipo, estado, duración, cantidad de Meeseeks,
profundidad y ganador cuando existe. Los errores y el caos quedan en el detalle.
El contador interno de activos debe quedar en cero después de recoger el árbol.
El informe se muestra en la terminal al salir; las corridas y el análisis se
documentan por separado mediante capturas.

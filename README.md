# Tarea #3 — Mr. Meeseeks

**José Julián Monge Brenes · 2024247024**

IC-6600 Principios de Sistemas Operativos · II semestre de 2026

Este proyecto representa una Box que atiende solicitudes usando procesos reales
de Linux. Lo voy desarrollando por etapas, con mi código en `src/main.c` y un
`main()` pequeño que delega en `box()`.

## Compilar y ejecutar en Ubuntu

Ejecuto estos comandos desde la carpeta del repositorio. Necesito GCC; si no lo
tengo, lo instalo con `sudo apt install build-essential`. TinyExpr ya viene
incluido en `vendor/tinyexpr` y no requiere otra instalación.

Creo la carpeta de salida una vez:

```bash
mkdir -p build
```

Compilo ambos archivos C y los enlazo en un solo comando:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DTE_POW_FROM_RIGHT src/main.c vendor/tinyexpr/tinyexpr.c -lm -o build/meeseeks
```

- `-std=c11`: selecciona el estándar de C.
- `-Wall -Wextra -Wpedantic`: activan advertencias.
- `-DTE_POW_FROM_RIGHT`: agrupa potencias de derecha a izquierda.
- `-lm`: enlaza funciones matemáticas y del entorno de punto flotante.
- `-o build/meeseeks`: establece la salida.

TinyExpr genera algunas advertencias; reviso aparte cualquier aviso de
`src/main.c`. No uso `-ffast-math` porque afecta la detección de errores numéricos.
Si la compilación termina correctamente, ejecuto:

```bash
./build/meeseeks
```

Recompilo cuando cambio código C. Para modificar el README o repetir una
corrida, no hace falta. Copio el comando de compilación en una sola línea para
evitar problemas con espacios al pegarlo.

## Avance paso a paso

1. Agregué el menú y la captura validada de las tres clases de solicitudes.
2. Creé el primer Meeseeks con `fork()`, su identidad y su espera con `waitpid()`.
3. Incorporé pipes privados para enviar solicitudes y recibir resultados completos.
4. Integré TinyExpr para evaluar cálculos y comunicar sus errores.
5. Agregué ejecución de programas, argumentos, estados de salida y tiempo real.
6. Separé los tiempos por solicitud y agregué un historial con el informe al salir.

## Organización del código

`TipoSolicitud` identifica el caso. `Solicitud` guarda contenido y configuración.
`Resultado` guarda estado, identidad, cantidad de Meeseeks, detalle, tiempo del
programa y pausa simulada. `RegistroSolicitud` conserva esos datos junto con la
solicitud y su tiempo total. Un tiempo medido en `-1` indica que no está disponible.

| Parte | Funciones |
| --- | --- |
| Control | `main()` llama a `box()`, que administra el menú, las cancelaciones y el total recibido. |
| Lectura | `limpiar_espacios()`, `leer_solicitud()`, `leer_numero()` y `leer_opcion()` validan entradas. EOF o un error de lectura permiten terminar. |
| Configuración | `capturar_datos_textuales()` pide facilidad y duración: 1 manual, 2 aleatorio. |
| Presentación | `nombre_tipo()`, `nombre_estado()`, `mostrar_solicitud()` y `mostrar_resultado()` muestran la información. |
| Comunicación | `crear_meeseeks()` crea los canales y el hijo. `meeseeks()` recibe y atiende la solicitud. `leer_completo()` y `escribir_completo()` completan transferencias parciales y reintentan ante `EINTR`. |
| Espera | `esperar_proceso()` recoge un hijo específico. `esperar_meeseeks()` también revisa su terminación. |
| Cálculos | `procesar_calculo()` limita las expresiones admitidas, llama a TinyExpr y comprueba errores. |
| Programas | `separar_argumentos()` prepara los argumentos. `procesar_programa()` coordina ejecución y tiempo. `ejecutar_programa_hijo()` prepara la entrada y ejecuta. `leer_error_ejecucion()` distingue fallos de lanzamiento de una salida real con código 127. |
| Medición | `procesar_solicitud()` mide desde antes de crear al Meeseeks hasta recogerlo. `diferencia_segundos()`, `tiempo_sin_simulacion()` y `mostrar_tiempos()` calculan y muestran los tiempos. |
| Historial | `registrar_solicitud()` reserva y guarda cada petición. `mostrar_informe()` presenta estados, tiempos, detalles y Meeseeks creados. |

## Bibliotecas y funcionamiento actual

Uso C estándar para entrada, cadenas, memoria y validaciones, e interfaces POSIX
para procesos, pipes, descriptores, señales y espera. `math.h` y `fenv.h` ayudan
a detectar errores numéricos; `clock_gettime(CLOCK_MONOTONIC)` mide tiempos.

La única biblioteca externa es [TinyExpr](https://github.com/codeplea/tinyexpr):
es pequeña, está escrita en C e interpreta aritmética, comparaciones y lógica.
Conservé su código y licencia zlib en `vendor/tinyexpr`, revisión
`7ab30db449a359e8af77fbb062ab2ec7ee686d62`. Todo mi código sigue en `src/main.c`.

- **Texto:** todavía solo capturo la configuración. La facilidad manual admite
  decimales de 0 a 100 y la aleatoria produce enteros del mismo rango. La duración
  manual debe ser positiva; la aleatoria se definirá al simular. Cancelar no suma
  una solicitud.
- **Cálculos:** admito números, paréntesis, `+ - * / % ^`, comparaciones
  `< <= > >= == !=` y lógica `&& || !`. Uso `double`, punto decimal y resultados
  lógicos 0 o 1. No habilité funciones ni variables simbólicas. `2^3^2` da 512 y
  `-2^2` da -4. No hay cortocircuito: `0 && (1/0)` también falla. Reviso errores
  desde antes de interpretar porque TinyExpr puede evaluar al compilar.
- **Programas:** admito 32 argumentos contando el ejecutable, comillas y escapes.
  No expando variables ni comodines y rechazo operadores de shell sin comillas.
  Hay un Meeseeks que espera y un hijo que usa `execvp()`. El programa recibe
  `/dev/null` como entrada y muestra su salida en la terminal. Un pipe con
  `FD_CLOEXEC` comunica fallos al lanzar. Distingo código 0, código de error y
  señal. El tiempo incluye lanzamiento y espera real.

## Medición e informe

Uso `CLOCK_MONOTONIC` desde antes de crear al Meeseeks hasta recogerlo, después
de capturar los datos. Distingo tiempo transcurrido, pausa artificial y tiempo
sin esa pausa: transcurrido menos espera simulada. No es tiempo de CPU. El
programa externo conserva además su propia duración.

Por ahora la pausa artificial es cero: configurar una duración no significa
haberla ejecutado. Al agregar la simulación mediré esa espera; con el árbol
habrá que evitar sumar pausas simultáneas de hermanos.

El historial se guarda en memoria de la Box y se libera al salir. El informe
incluye estados, detalles, tiempos y Meeseeks creados por solicitud. Una
cancelación no se registra y un programa externo no cuenta como otro Meeseeks.

## Prueba rápida

Después de compilar, puedo probar las tres opciones y una cancelación:

```bash
printf '%s\n' 2 '5+3*2' 2 '1/0' 3 'pwd' 1 'Prueba textual' 1 45.01 1 0.5 1 'Cancelada' 0 0 | ./build/meeseeks
```

Espero 4 solicitudes: 2 completadas, 1 no completada y 1 pendiente. Cada una
muestra tiempos y un Meeseeks creado. Son resultados esperados para comprobar,
no mediciones experimentales. También puedo probar `false`, `sh -c "exit 127"`
y un ejecutable inexistente para revisar diferentes errores de programas.

## Pendiente

Faltan simulación probabilística con aging, árbol de Meeseeks, resolución en
cascada, contadores compartidos con semáforos, límites y caos planetario. El
informe básico ya funciona; falta ampliarlo con las estadísticas del árbol y
hacer los experimentos para el reporte. Voy actualizando este resumen conforme
termino cada etapa.

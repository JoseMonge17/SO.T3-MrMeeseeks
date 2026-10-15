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

## Organización del código

`TipoSolicitud` identifica el caso. `Solicitud` guarda contenido y configuración.
`Resultado` guarda estado, identidad, detalle y tiempo; `-1` indica que no hay
una medición disponible.

| Parte | Funciones |
| --- | --- |
| Control | `main()` llama a `box()`, que administra el menú, las cancelaciones y el total recibido. |
| Lectura | `limpiar_espacios()`, `leer_solicitud()`, `leer_numero()` y `leer_opcion()` validan entradas. EOF o un error de lectura permiten terminar. |
| Configuración | `capturar_datos_textuales()` pide facilidad y duración: 1 manual, 2 aleatorio. |
| Presentación | `nombre_tipo()`, `mostrar_solicitud()` y `mostrar_resultado()` muestran la información. |
| Comunicación | `crear_meeseeks()` crea los canales y el hijo. `meeseeks()` recibe y atiende la solicitud. `leer_completo()` y `escribir_completo()` completan transferencias parciales y reintentan ante `EINTR`. |
| Espera | `esperar_proceso()` recoge un hijo específico. `esperar_meeseeks()` también revisa su terminación. |
| Cálculos | `procesar_calculo()` limita las expresiones admitidas, llama a TinyExpr y comprueba errores. |
| Programas | `separar_argumentos()` prepara los argumentos. `procesar_programa()` coordina ejecución y tiempo. `ejecutar_programa_hijo()` prepara la entrada y ejecuta. `leer_error_ejecucion()` distingue fallos de lanzamiento de una salida real con código 127. |

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

## Pruebas rápidas

Después de compilar, compruebo los cálculos con:

```bash
printf '%s\n' 2 '5+3*2' 2 '(10+5)/3' 2 '5>3 && !(2==3)' 2 '1/0' 0 | ./build/meeseeks
```

Espero 11, 5, 1 y un error de división por cero. Para los programas:

```bash
printf '%s\n' 3 'pwd' 3 'false' 3 'sh -c "exit 127"' 3 'sh -c "kill -TERM $$"' 3 'programa_no_existe_meeseeks' 0 | ./build/meeseeks
```

Espero códigos 0, 1 y 127, señal 15 y un fallo al lanzar. Aquí solicito `sh`
explícitamente para probar sus estados de salida. La Box debe volver al menú
después de cada solicitud. Son resultados esperados, no mediciones experimentales.

## Pendiente

Faltan simulación probabilística con aging, árbol de Meeseeks, resolución en
cascada, contadores compartidos con semáforos, límites y caos planetario. También
faltan tiempos de los casos A y B, estadísticas finales y experimentos para el
reporte. Voy actualizando este resumen conforme termino cada etapa.

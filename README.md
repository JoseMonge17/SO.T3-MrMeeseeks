# Tarea #3 — Mr. Meeseeks

Instituto Tecnológico de Costa Rica

Campus Tecnológico Central Cartago

Escuela de Ingeniería en Computación

| Dato | Información |
| --- | --- |
| Curso | IC-6600 Principios de Sistemas Operativos |
| Período | II semestre de 2026 |
| Profesor | Ing. Esteban Arias Méndez, M.Sc. |
| Estudiante | José Julián Monge Brenes |
| Carné | 2024247024 |
| Fecha de entrega | 15 de octubre de 2026 |

## Descripción

La tarea consiste en implementar una Box de Mr. Meeseeks en C sobre GNU/Linux.
La Box recibe solicitudes y crea procesos reales mediante `fork()` para
atenderlas. Permite realizar varias solicitudes consecutivas hasta seleccionar
la opción de salir y presenta un informe final.

## Tipos de solicitudes

| Caso | Solicitud | Procesamiento |
| --- | --- | --- |
| A | Texto, como «Quiero aprender a jugar golf» | Simulación basada en probabilidades. |
| B | Cálculo aritmético o lógico | Interpretación de la expresión y devolución del resultado o error. |
| C | Programa externo, como `ls -l` o `pwd` | Ejecución, espera y comprobación de su terminación. |

## Requisitos principales

- Cada Meeseeks debe mostrar PID, PPID, nivel e instancia. El primero comienza
  en nivel 1 e instancia 1.
- Las solicitudes y los resultados se transmiten mediante pipes privados.
- Los contadores compartidos se protegen mediante semáforos.
- Cuando un proceso resuelve una tarea, el resultado se propaga hacia sus padres.
  Las otras ramas reciben avisos para detenerse, despedirse y finalizar.
- Cada padre recoge a sus hijos con `waitpid()`.
- El programa limita la creación de procesos y el tiempo de ejecución para
  evitar el agotamiento de recursos.

## Simulación textual

La facilidad puede ingresarse manualmente o generarse aleatoriamente entre
0 y 100, incluyendo decimales. Una facilidad de 0 representa una tarea imposible.
La duración puede ser manual, con un valor positivo, o aleatoria entre 0.5 y
5 segundos.

| Facilidad original | Ayuda cuando el Meeseeks no resuelve |
| --- | --- |
| De 0 a 45 | Al menos 5 hijos. |
| Mayor que 45 y hasta 85 | Al menos 2 hijos. |
| Mayor que 85 y hasta 100 | Resuelve un único Meeseeks. |

Cada nuevo proceso evalúa nuevamente la posibilidad de resolver. Las tareas
realizables se simplifican entre generaciones mediante aging; una tarea
imposible conserva su facilidad en cero.

## Compilar y ejecutar en Ubuntu

Se necesita GCC, disponible con `sudo apt install build-essential`.
TinyExpr está incluido en `vendor/tinyexpr` y no requiere otra instalación.
Todo el código propio permanece en `src/main.c`.

Desde la carpeta del repositorio, crear la carpeta de salida:

```bash
mkdir -p build
```

Compilar el programa junto con TinyExpr en un solo comando:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DTE_POW_FROM_RIGHT src/main.c vendor/tinyexpr/tinyexpr.c -pthread -lm -o build/meeseeks
```

`-pthread` habilita el soporte POSIX de sincronización y `-lm` enlaza las funciones
matemáticas. `TE_POW_FROM_RIGHT` establece la asociación de potencias de derecha
a izquierda. No debe usarse `-ffast-math`, porque afecta la detección de errores
numéricos.

Para ejecutar:

```bash
./build/meeseeks
```

El programa debe recompilarse después de modificar código C.

## Bibliotecas y operaciones

El proyecto utiliza C estándar e interfaces POSIX para procesos, pipes,
semáforos, memoria compartida y medición de tiempos.

[TinyExpr](https://github.com/codeplea/tinyexpr) interpreta los cálculos y conserva
su licencia zlib en `vendor/tinyexpr/LICENSE`. Se admiten números, paréntesis,
`+ - * / % ^`, comparaciones `< <= > >= == !=` y operadores lógicos `&& || !`.
Los resultados lógicos son 0 o 1. Las expresiones admitidas no incluyen funciones
ni variables y se evalúan sin cortocircuito: `0 && (1/0)` también produce un error.

## Límites y caos planetario

El plazo predeterminado es de 300 segundos, equivalente a cinco minutos, y el
presupuesto es de 128 Meeseeks por solicitud.

`MEESEEKS_PLAZO_SEGUNDOS` permite configurar el plazo y `MEESEEKS_MAX_PROCESOS`
el presupuesto, hasta un máximo de 256. Al alcanzar el plazo se declara caos
planetario, se cancela y recoge el árbol y la Box termina mostrando el informe.

Para las pruebas que exceden el plazo normal existe un modo experimental con
un límite extraordinario explícito y el presupuesto de procesos activo:

```bash
MEESEEKS_EXPERIMENTAL=1 MEESEEKS_LIMITE_EXTRAORDINARIO=900 MEESEEKS_MAX_PROCESOS=64 ./build/meeseeks
```

## Informe y entrega

El informe final muestra el total de solicitudes, completadas y no completadas,
la cantidad de Meeseeks por solicitud, la profundidad y la identidad del ganador.
Distingue el tiempo transcurrido, la CPU consumida sin esperas artificiales y la
duración del programa externo.

La entrega incluye el código fuente, el enlace al repositorio privado y un
reporte PDF con portada, introducción, índice, desarrollo y conclusiones.
El reporte debe explicar la implementación con diagramas, mostrar al menos tres
corridas de cada caso y comparar resultados reales, incluyendo tareas difíciles,
imposibles, caos planetario y pruebas que excedan el plazo normal.

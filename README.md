# Tarea #3 — Mr. Meeseeks

Proyecto en C para GNU/Linux. El codigo propio se mantiene en `src/main.c`.

## Compilacion en Ubuntu

Si GCC no esta instalado, se puede obtener con `sudo apt install build-essential`.
TinyExpr ya esta incluido en `vendor/tinyexpr`; no requiere instalar un paquete
adicional ni descargar archivos al compilar.

```bash
mkdir -p build
gcc -std=c11 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -c vendor/tinyexpr/tinyexpr.c -o build/tinyexpr.o
gcc -std=c11 -Wall -Wextra -Wpedantic \
    src/main.c build/tinyexpr.o -lm -o build/meeseeks
./build/meeseeks
```

La biblioteca se compila por separado porque sus conversiones entre punteros
a funciones y a objetos generan advertencias con `-Wpedantic`. El codigo propio
conserva esa verificacion. `-lm` enlaza las funciones matematicas y de manejo
del entorno de punto flotante. No se debe utilizar `-ffast-math`, ya que puede
invalidar la deteccion de errores numericos.

## Caso B: calculos aritmeticos y logicos

Se utiliza [TinyExpr](https://github.com/codeplea/tinyexpr), una biblioteca pequena
escrita en C. Interpreta la expresion dentro del unico proceso Meeseeks creado
para la solicitud. Se adapto la integracion, sin modificar el codigo de la
biblioteca ni escribir un parser propio.

Se incluye la revision `7ab30db449a359e8af77fbb062ab2ec7ee686d62`, conservando su
licencia zlib en `vendor/tinyexpr/LICENSE`. Esta revision incluye operadores
relacionales y logicos. Una version anterior de TinyExpr podria no admitirlos.

### Operaciones admitidas en esta etapa

| Operacion | Sintaxis |
| --- | --- |
| Suma, resta y signos unarios | `+`, `-` |
| Multiplicacion y division real | `*`, `/` |
| Resto de punto flotante | `%` |
| Potencia | `^` |
| Agrupacion | `(`, `)` |
| Comparaciones | `<`, `<=`, `>`, `>=`, `==`, `!=` |
| Logica | `&&`, `||`, `!` |

Se admiten numeros decimales con punto, notacion cientifica como `1.5e-3` y
constantes hexadecimales enteras como `0x10`. Por ahora no se habilitan nombres
de variables, constantes simbolicas ni funciones de la biblioteca. La revision
de identificadores solo delimita ese alcance; TinyExpr realiza el analisis
sintactico y la evaluacion de los operadores.

### Consideraciones

- Los calculos utilizan `double`; la salida muestra hasta 15 cifras significativas.
- Cero representa falso y cualquier otro numero representa verdadero. Las
  comparaciones y los operadores logicos devuelven `0` o `1`.
- La precedencia de comparaciones y operadores logicos sigue a C. Una cadena
  como `1 < 2 < 3` significa `(1 < 2) < 3`.
- `TE_POW_FROM_RIGHT` configura `2^3^2` como `2^(3^2)`, que da `512`, y `-2^2`
  como `-(2^2)`, que da `-4`. `^` es potencia; `**` no esta admitido.
- `&&` y `||` evaluan ambos operandos. Por eso `0 && (1/0)` tambien se informa
  como error. No existe cortocircuito.
- La coma puede encadenar expresiones y devuelve el ultimo valor, segun la
  gramatica de TinyExpr. Los decimales deben usar punto.
- Se comprueba la posicion de los errores de sintaxis y se detectan divisiones
  por cero, operaciones indefinidas y desbordamientos. El entorno de punto
  flotante se limpia antes de interpretar, porque TinyExpr tambien evalua
  subexpresiones al compilar. Esto permite detectar errores intermedios aunque
  el valor final sea finito, como ocurre con `(1/0) > 0`.
- Se conserva el redondeo de `double`: valores extremadamente pequenos pueden
  aproximarse a cero. No se utiliza aritmetica de precision arbitraria.
- Un calculo invalido devuelve `RESULTADO_NO_COMPLETADO` y su explicacion por
  el pipe. El Meeseeks termina normalmente si pudo comunicar ese resultado,
  permitiendo que la Box reciba otra solicitud.

### Comprobacion en Ubuntu

Luego de compilar, se pueden enviar varios calculos consecutivos:

```bash
printf '2\n5 + 3 * 2\n2\n(10 + 5) / 3\n2\n5 > 3 && !(2 == 3)\n2\n10 / (3 - 3)\n2\n5 +\n0\n' | ./build/meeseeks
```

Los resultados esperados son `11`, `5`, `1`, un error de division por cero y un
error de sintaxis, respectivamente. Cada solicitud debe crear un solo Meeseeks
de nivel e instancia `1`. Estos casos son comprobaciones funcionales; no
constituyen resultados experimentales de tiempos de ejecucion.

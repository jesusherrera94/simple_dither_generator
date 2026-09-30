# Implementación: un generador de dither en C++

Hasta aquí vimos la teoría. Ahora vamos a escribirlo. El objetivo de esta sección es que puedas
leer el código y reconocer en él cada concepto del post: el escalón de cuantización, el LSB, las dos
distribuciones de ruido y, sobre todo, el *orden* en que las cosas tienen que pasar.

El resultado es un pedal para la plataforma Hothouse, así que además de ver el código lo vas a poder
escuchar.

## Antes de empezar: por qué el bit depth es una perilla

En el post hablamos de reducir de 24 a 16 bits, que es el caso real. El problema es que si
implementamos exactamente eso en un pedal, **no se escucha nada**. La distorsión de cuantización a
16 bits vive unos 96 dB por debajo de la señal: está ahí, es medible, pero no la vas a oír con una
guitarra enchufada.

Por eso en esta implementación el bit depth es un control y no una constante. Con la perilla en cero
estás en 16 bits, el caso honesto del que habla el post. A medida que la subís, el bit depth baja
hasta 4 bits, y ahí sí escuchás las dos cosas que nos interesan: primero aparece la distorsión de
cuantización, y después, al pisar el dither, esa distorsión se convierte en un piso de ruido parejo.

Escuchar ese cambio *es* la lección. El dither no hace desaparecer el error: lo cambia de forma,
cambia distorsión correlacionada por ruido no correlacionado.

## La cadena de señal

Todo el efecto es esto:

```
entrada → [+ dither] → [cuantizar a N bits] → [volumen] → salida
```

El orden no es negociable y vale la pena detenerse en los dos extremos:

- **El dither entra ANTES del cuantizador.** Si lo sumás después, no estás dithereando nada: estás
  agregando ruido encima de una distorsión que ya ocurrió. El dither funciona porque está presente
  *en el momento* en que se decide a qué escalón redondea cada sample.
- **El volumen va AL FINAL.** Si escalás la señal antes de cuantizar, estás cambiando qué tan grande
  es la señal respecto de 1 LSB, que es justamente la variable que el post analiza. Un control de
  volumen mal ubicado acá no baja el volumen: cambia el efecto.

## El escalón de cuantización

Primero necesitamos saber cuánto mide un escalón. Trabajamos con samples normalizados en el rango
`[-1, 1]`, así que:

```cpp
float dsp::lsbStep(int bitDepth) {
    return 2.0f / static_cast<float>((1 << bitDepth) - 1);
}
```

A 16 bits eso da un escalón de `2/65535 ≈ 0.0000305`. A 4 bits da `2/15 ≈ 0.1333`, unas 4400 veces
más grande. Ese número es nuestro LSB, y es la unidad en la que vamos a medir el dither.

Cuantizar es redondear al escalón más cercano:

```cpp
float dsp::quantize(float x, int bitDepth) {
    const float step = lsbStep(bitDepth);
    const float snapped = std::round(x / step) * step;
    return std::fmin(std::fmax(snapped, -1.0f), 1.0f);
}
```

Fijate en lo determinista que es ese `std::round`. La misma entrada cae siempre en el mismo escalón,
siempre con el mismo error. Eso es exactamente lo que decíamos en el post: el error queda
emparentado con la señal y el oído lo escucha como distorsión armónica, no como ruido.

El `clamp` del final está porque el dither puede empujar un sample que ya venía cerca del máximo un
poco más allá de `±1`.

> **Un detalle si vas a mirar los números de cerca:** con esta fórmula la grilla queda centrada en
> cero, lo cual es lo que querés en audio — el silencio es exactamente representable. El precio es
> que a 4 bits obtenés 15 escalones en lugar de 16, y el máximo queda en 0.933 en vez de 1.0. Es el
> clásico off-by-one de un cuantizador simétrico y no cambia nada de lo que escuchás.

## El ruido

Necesitamos ruido blanco, y lo necesitamos dentro del callback de audio. Eso descarta `std::rand()`:
no es seguro para tiempo real ni para múltiples hilos. Un xorshift de 32 bits alcanza de sobra, son
tres líneas y no asigna memoria:

```cpp
float dsp::NoiseSource::nextUniform() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return static_cast<float>(state_) * 2.3283064e-10f;  // 2^-32 → [0, 1)
}
```

El único cuidado: si el estado llega a ser 0, se queda en 0 para siempre. Por eso el constructor
nunca deja pasar esa semilla.

## Las dos distribuciones

Acá está el corazón del asunto, y es más corto de lo que uno esperaría:

```cpp
case DitherType::Rectangular:
    return (noise_.nextUniform() - 0.5f) * step;

case DitherType::Triangular:
    return (noise_.nextUniform() - noise_.nextUniform()) * step;
```

**RPDF** es un solo ruido uniforme, corrido para que quede centrado: `±0.5 LSB`. Descorrelaciona el
error y la distorsión se va.

**TPDF** es la resta de dos ruidos uniformes independientes, lo que da `±1 LSB` con distribución
triangular. Restar uno de otro es lo mismo que sumar dos ruidos uniformes, y es por qué se llama
triangular: la suma de dos distribuciones rectangulares da una triangular.

Una sola línea de diferencia, y esa línea es la que separa "funciona" de "es el estándar de la
industria".

## ¿Y esto se puede comprobar?

Sí, y es el ejercicio más útil que podés hacer con este código. Dos mediciones, las dos a 4 bits.

**Primera: el dither linealiza la señal por debajo del escalón.** Tomamos una entrada constante y la
movemos de a poquito a lo largo de *un solo* escalón de cuantización, midiendo el promedio de la
salida:

| entrada | promedio sin dither | promedio con TPDF |
|---|---|---|
| 0.00000 | 0.00000 | -0.00003 |
| 0.04000 | 0.00000 | 0.03999 |
| 0.06000 | 0.00000 | 0.06000 |
| 0.06667 | 0.13333 | 0.06655 |
| 0.10000 | 0.13333 | 0.10012 |
| 0.13333 | 0.13333 | 0.13330 |

Sin dither la salida es una escalera: se queda clavada en 0, y de golpe salta a 0.13333. Toda la
información intermedia se perdió — esto es, literalmente, lo que veíamos en los gráficos del post
cuando los samples se apilaban en tres escalones.

Con TPDF el promedio **sigue a la entrada**, aunque cada sample individual sigue cayendo en la misma
grilla gruesa de 15 escalones. El error promedio pasa de 0.03175 a 0.00009: unas 350 veces mejor.
Ninguna muestra es más precisa que antes; lo que cambió es que ahora el error es ruido, y el ruido
promedia a cero.

**Segunda: la modulación de ruido de RPDF es real.** Medimos la varianza del error con la señal
parada en dos puntos distintos dentro del escalón:

| | en 0.0 LSB | en 0.5 LSB |
|---|---|---|
| RPDF | 0.000000 | 0.004444 |
| TPDF | 0.004442 | 0.004444 |

Mirá el cero de RPDF. Con la señal justo sobre un escalón, un ruido de `±0.5 LSB` nunca llega a
cruzar el límite de decisión, así que no pasa nada: cero ruido, pero también cero dither. Medio
escalón más allá, ruido máximo. Ese ir y venir según dónde esté la señal es exactamente el
"respirar" del que hablábamos.

TPDF no se mueve: 0.004442 contra 0.004444. Por eso es el que usan los DAWs.

## Los controles

| Control | Función |
|---|---|
| Perilla 1 | Volumen de salida |
| Perilla 2 | Bit depth objetivo, de 16 a 4 bits |
| Toggle 1 | Tipo de dither — arriba TPDF, medio RPDF, abajo sin dither |
| Footswitch 1 | Dither on/off |
| Footswitch 2 | Bypass |

El footswitch y el toggle se combinan: el toggle elige *cuál* dither, y el footswitch es el A/B que
podés pisar en medio de una frase. Es la forma más directa de escuchar el cambio.

El bit depth se mueve en enteros, no interpolado — no existe algo como 11.4 bits:

```cpp
int knobToBitDepth(float knob, int maxBits, int minBits) {
    const float span = static_cast<float>(maxBits - minBits);
    return maxBits - static_cast<int>(std::lround(knob * span));
}
```

## Dos cosas sobre el hilo de audio

Aunque este efecto es simple, el código respeta dos reglas que cualquier DSP en tiempo real tiene que
respetar.

**Los controles se leen con atómicos.** La perilla la mueve el loop de control; la señal la procesa
el callback de audio. Son dos hilos distintos, y el traspaso tiene que ser sin locks. Además, toda la
matemática de mapeo (posición de perilla → bit depth) se hace en los setters, es decir a control
rate, para que `processSample()` solo tenga que leer valores ya listos.

**Y esos atómicos tienen que ser lock-free de verdad.** Un `std::atomic` que internamente use un
mutex pondría un lock en el camino del audio, con riesgo de inversión de prioridades y un dropout
audible. Es fácil asumirlo y estar equivocado, así que el código lo verifica en tiempo de
compilación:

```cpp
static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "atomic<bool> must be lock-free");
static_assert(ATOMIC_INT_LOCK_FREE  == 2, "atomic<int>/<float> must be lock-free");
```

Nada de esto agrega complejidad al efecto, pero es la diferencia entre código que funciona en tu
máquina y código que funciona en el pedal.

## Lo que quedó afuera

El **noise shaping** no está implementado, a propósito. Redistribuir el ruido con una curva
psicoacústica implica realimentación de error, estado de filtro y cuidar la estabilidad, y eso es
otro post. El dither TPDF plano que acabamos de escribir es la opción transparente y segura, y es la
base sobre la que se construye cualquier noise shaper.

El **dither gaussiano** tampoco: como decíamos, cuesta unos 2.8 dB más de ruido que el triangular
sin ninguna ventaja práctica.

Si querés seguir, el orden natural es agregar un noise shaper de primer orden sobre este mismo
cuantizador y comparar a oído contra el TPDF plano a 8 bits. Con lo que ya está escrito, es
sorprendentemente poco código.

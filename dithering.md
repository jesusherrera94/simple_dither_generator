# Audio 101 - Dithering

Es el proceso en el cual se añade ruido intencionalmente a una señal durante el proceso de cuantización, para así, preservar la información de bajo nivel y prevenir distorsión.

Parece ser contraintuitivo el hecho de agregar ruido a una señal cuando nosotros queremos que el audio sea lo más limpio posible en nuestras grabaciones, pero esto tiene su razón de ser. Como vimos en el post de Bit Depth, cuando en el proceso de reducir el bit depth de un audio de 24 bits a 16, pueden haber errores de cuantización, entonces aplicamos ruido SOLO cuando reducimos el bit depth, al final cuando vamos a masterizar.

<aside>
💡

En la industria en un DAW o plugin trabajaríamos el audio en 32 - bits float, pero una vez necesitamos exportar ese audio, digamos, salvarlo en la computadora, subirlo a un servicio de streaming o compartirlo con tus amigos, el bit depth necesita ser reducido a 16 bit int.

</aside>

Para entender mejor lo que sucede veamos lo siguiente:

Tomaremos una señal cualquiera y la vamos a cuantizar a un sample rate X y un bit depth Y. En este punto no agregaremos dither aún.

![image.png](Audio%20101%20-%20Dithering/image.png)

En la cual la gráfica azul sería nuestra señal de alta resolución y los puntos amarillos lo que recibimos en nuestro software, como siempre podemos observar que en este momento, lo que obtenemos y pretendemos exportar sigue el patrón de nuestra señal, pero veamos lo que sucede cuando reducimos la amplitud de la señal a la mitad:

![image.png](Audio%20101%20-%20Dithering/image%201.png)

Aquí podemos ver que cada vez más puntitos (samples) caen en solo los escalones entre -1 a 1. Lo que sucedería a partir de este punto es que la información de nuestra señal original se comenzaría a perder y por lo tanto cuando hay un “fade out” comenzaríamos a escuchar ruido y potencialmente distorsión en el audio debido a que cada vez más samples caen en el escalón de 0.

Sin dither el error de cuantización es determinista y está correlacionado con la señal, el mismo valor de entrada siempre se redondea igual. Produciendo distorsión armónica, que en el oido detecta fácilmente porque está musicalmente emparentada con la señal.

## ¿Cómo el dither puede evitar esto?

Bueno, el dither se aplica en el orden de 1 LSB(Least Significant Bit o bit menos significativo). Hablamos de bits significativos, el cual puede ser un término muy a bajo nivel porque dependiendo del bit depth original, el valor del sample “dithered” puede ser mayor o menor.

Como regla general, entre mayor el bit depth, menor es la distancia del sample de dither del sample original. Pasemos a verlo gráficamente:

![image.png](Audio%20101%20-%20Dithering/image%202.png)

En la cual tenemos nuestra señal de amplitud reducida (gráfica color azul) y el dither agregado en color rojo. Bien, la siguiente imagen veremos como queda nuestra señal de salida con el dither agregado:

![image.png](Audio%20101%20-%20Dithering/image%203.png)

En esta gráfica podemos ver que los puntos verdes que sería la señal con bit depth reducido, en lugar de estar todos los samples en solo 3 escalones, ahora están entre 5 escalones, si bien, no es perfecta como la señal 1, pero mejoraría bastante la percepción de la calidad de audio y sobre todo, no tendríamos las distorsiones que pueden aparecer al final. Con dither, una señal con amplitud tres veces menor, puede llegar a sobrevivir casi intacta.

## Tipos de Dither

Como regla general podemos clasificar los dither o la forma de ruido basado en lo siguiente:

- Rango: bit menos significativo (LSB)
- Tipo de función: el tipo de fórmula que se utiliza para generar el piso de ruido(PDF).

### Rectangular

Esta es una distribución uniforma de ±0.5 LSB. Elimina la distorsión pero deja modulación de ruido. El nivel de piso de ruido varia según la señal y eso se escucha como un “respirar” en los sonidos suaves.

### Triangular

±1 LSB y se genera sumando dos ruidos uniformes independientes. Elimina la distorsión y la modulación de ruido. Este es el estándar de la industria y lo que usa casi cualquier DAW por defecto. 

## Gaussiano

Este en la práctica casi no se usa. Cuesta unos 2.8 dB más de ruido que el triangular sin ninguna ventaja.

## Noise Shaping

Hasta aquí asumimos que el ruido del dither se reparte parejo por todo el espectro, y así es, es ruido blanco, tiene la misma energía en los graves que en los agudos. Pero nuestro oído no escucha parejo. Somos mucho más sensibles alrededor de los 3 o 4 kHz, justo donde vive la voz humana; y bastante sordos en los extremos, sobre todo arriba de los 15 kHz.

El noise shaping aprovecha eso. En lugar de dejar el ruido distribuido uniformemente, lo **redistribuye,** saca ruido de la zona donde el oído es más sensible y lo empuja hacia arriba, cerca del límite de lo audible. Lo curioso es que la cantidad total de ruido en el archivo **aumenta**, pero la cantidad de ruido que se llega a percibir es baja bastante. Se está pagando con ruido en un lugar donde casi no se escucharía para comprar silencio en el lugar donde sí.

Esto no es algo exótico: si alguna vez exportaste desde un DAW y viste opciones como POW-r 1, POW-r 2, UV22 o los modos de dither de Ozone, todas esas son variantes de noise shaping con curvas distintas. El dither plano TPDF que vimos arriba es la opción segura y transparente; el noise shaping es el paso siguiente cuando quieres exprimir el máximo de los 16 bits.

<aside>
💡

Dos cosas a tener en cuenta: el noise shaping se aplica solo en la exportación final, nunca en material que vaya a seguir procesándose, porque la curva de ruido se acumula y se deforma en cada etapa. Y si tu audio va a terminar en un codec con pérdida (MP3, AAC), las formas agresivas pueden interactuar mal con el codec — en ese caso TPDF plano suele ser mejor decisión.

</aside>
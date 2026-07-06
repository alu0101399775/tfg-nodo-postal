# tfg-nodo-postal
Firmware Arduino y servidor Python del prototipo de nodo postal inteligente. TFG GIEIA, Universidad de La Laguna

Prototipo de maqueta de un nodo postal inteligente, desarrollado como Trabajo de
Fin de Grado en Ingeniería Electrónica Industrial y Automática de la Universidad
de La Laguna.

El sistema demuestra la lógica de gestión de un punto de entrega desatendido:
gestiona nueve taquillas virtuales sobre un único conjunto físico de actuación,
con dos vías de acceso (depósito por teclado y recogida por código QR) y
emulación de telemetría por puerto serie.

## Contenido del repositorio

- `taquilla.ino` — Firmware del microcontrolador (Arduino Mega 2560). Gestiona
  el teclado matricial, la pantalla LCD, el sensor de ultrasonido, la cerradura
  y el microinterruptor. Implementa la máquina de estados de las nueve taquillas
  y la comunicación serie con el servidor.
- `servidor.py` — Servidor local (PC). Genera el código único de cada envío,
  crea la imagen QR, lee los códigos por webcam y registra la telemetría.

## Arquitectura

El control se reparte entre dos elementos:

- El **microcontrolador** gobierna el hardware y la lógica de estado de las
  taquillas.
- El **servidor en el PC** se encarga de la visión (lectura de QR por cámara),
  la generación de los códigos de envío y el registro de la telemetría.

Ambos se comunican por puerto serie mediante un protocolo de texto:

- `EVT:<tipo>:<taquilla>:<dato>` — telemetría del microcontrolador al PC.
- `SET:<taquilla>:<codigo>` — el PC asigna el código generado en un depósito.
- `QR:<codigo>` — el PC transmite un código leído por la cámara en una recogida.

## Flujo de funcionamiento

- **Depósito:** se pulsa la tecla `C`; el sistema busca una taquilla libre,
  solicita al servidor un código único, genera el QR y abre la taquilla. El
  depósito se confirma al detectar el paquete con el ultrasonido y registrar el
  cierre de la puerta.
- **Recogida:** se muestra el QR a la webcam; el servidor lo decodifica y lo
  envía al microcontrolador, que abre la taquilla solo si ese código corresponde
  a un envío previamente depositado.

## Requisitos

Firmware (Arduino IDE):
- Placa Arduino Mega 2560.
- Librerías: `Keypad`, `LiquidCrystal_I2C`.

Servidor (Python 3.12):
- `pip install opencv-python pyzbar pyserial qrcode pillow`

## Uso

1. Cargar `taquilla.ino` en el Arduino Mega.
2. Ajustar el puerto serie en `servidor.py` (variable `PUERTO`).
3. Cerrar el monitor serie del IDE y ejecutar el servidor: `python servidor.py`.
4. Depósito: pulsar `C` en el teclado; el servidor genera el QR.
5. Recogida: mostrar el QR a la webcam.

## Nota

Este código corresponde a una maqueta demostrativa. Las nueve taquillas se
gestionan de forma lógica sobre un único actuador físico. La telemetría se emula
por puerto serie; en la unidad real se canalizaría por el enlace NB-IoT descrito
en la memoria.

## Autor

Jorge Juan Arencibia Martínez — Universidad de La Laguna, 2026.

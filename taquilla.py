"""
Hito 3 - Servidor PC.
- Recibe EVT:DEP_INI del Arduino, genera un codigo unico, crea el QR y lo muestra.
- Envia SET:<taq>:<codigo> al Arduino.
- Lee QR por webcam en la recogida y envia QR:<codigo>.
- Registra la telemetria en telemetria.log.
Requisitos: pip install opencv-python pyzbar pyserial qrcode pillow
"""

import cv2
from pyzbar.pyzbar import decode
import serial, threading, time, random
from datetime import datetime
import qrcode

PUERTO = "COM10"      # ajustar al puerto real del Arduino
BAUDIOS = 115200

arduino = serial.Serial(PUERTO, BAUDIOS, timeout=1)
time.sleep(2)  # espera al reset del Arduino

def registrar(l):
    ts = datetime.now().strftime("%H:%M:%S")
    print(f"[{ts}] {l}")
    with open("telemetria.log", "a", encoding="utf-8") as f:
        f.write(f"{ts};{l}\n")

def generar_codigo():
    return "ENV" + datetime.now().strftime("%H%M%S") + str(random.randint(10, 99))

def generar_qr(codigo):
    img = qrcode.make(codigo)
    ruta = f"qr_{codigo}.png"
    img.save(ruta)
    qr = cv2.imread(ruta)
    cv2.imshow("QR generado - escanealo para recoger", qr)
    cv2.waitKey(1)
    print(f"QR generado para {codigo} -> {ruta}")

def escuchar():
    while True:
        if arduino.in_waiting:
            linea = arduino.readline().decode(errors="ignore").strip()
            if not linea:
                continue
            registrar("<- " + linea)
            if linea.startswith("EVT:DEP_INI:"):
                taq = linea.split(":")[2]
                cod = generar_codigo()
                generar_qr(cod)
                arduino.write(f"SET:{taq}:{cod}\n".encode())
                registrar(f"-> SET:{taq}:{cod}")

threading.Thread(target=escuchar, daemon=True).start()

cap = cv2.VideoCapture(0)
ultimo = None
tenv = 0
print("Servidor activo. Pulsa Q en la ventana de la webcam para salir.")

while True:
    ok, frame = cap.read()
    if not ok:
        continue
    for obj in decode(frame):
        cod = obj.data.decode("utf-8")
        if cod != ultimo or time.time() - tenv > 2:
            arduino.write(f"QR:{cod}\n".encode())
            registrar(f"-> QR:{cod}")
            ultimo = cod
            tenv = time.time()
        pts = obj.polygon
        if len(pts) >= 4:
            for i in range(len(pts)):
                cv2.line(frame, pts[i], pts[(i + 1) % len(pts)], (0, 255, 0), 2)
    cv2.imshow("Lector QR - nodo postal", frame)
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

cap.release()
cv2.destroyAllWindows()
arduino.close()
import socket
import time
import math
import random

# --- CONFIGURACIÓN DE RED ---
UDP_IP = "127.0.0.1"  # Enviamos a nuestro propio PC (Localhost)
UDP_PORT = 4210

# Formato del paquete: "clave=valor" separados por ';'. El monitor tolera que
# falte cualquier canal, así que se pueden comentar líneas del diccionario de
# abajo para simular canales que el firmware todavía no envía.
FORMATO_CLAVE_VALOR = True

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print("--- SIMULADOR G26 INICIADO ---")
print(f"Enviando a {UDP_IP}:{UDP_PORT}")
print("Formato:", "clave=valor" if FORMATO_CLAVE_VALOR else "posicional (antiguo)")

t = 0.0

while True:
    # El acelerador manda: marca el régimen, la carga y la mezcla
    tps = max(0.0, 100 * abs(math.sin(t / 3.0)) - 5)
    carga = tps / 100.0

    # Régimen siguiendo al acelerador, con algo de inercia
    rpm = 1200 + 12800 * (carga ** 0.7) + random.uniform(-120, 120)

    # Velocidad: correlada con el régimen (modelo simple para el simulador)
    velocidad = 15 + 105 * (carga ** 0.8) + random.uniform(-1.5, 1.5)

    # Freno delantero: presión (bar). Frena al levantar el pie del acelerador
    freno_del = max(0.0, 45 * max(0.0, min(1.0, (6 - tps) / 6)) + random.uniform(-0.4, 0.4))

    # Temperaturas: suben poco a poco y responden a la carga
    ect = 88 + 6 * math.sin(t / 20.0) + 4 * carga + random.uniform(-0.3, 0.3)
    taceite = 98 + 8 * math.sin(t / 25.0) + 6 * carga + random.uniform(-0.4, 0.4)

    # MAP: motor atmosférico -> depresión en ralentí, cerca de atmosférica a fondo
    mapa = 28 + 72 * carga + random.uniform(-1.5, 1.5)

    # Presión de combustible: cae ligeramente al pedir caudal
    pcomb = 3.8 - 0.35 * carga + random.uniform(-0.04, 0.04)

    # Presión de aceite: sube con el régimen (regla aproximada de 1 bar/1000 rpm)
    paceite = max(0.8, min(6.5, rpm / 2200.0)) + random.uniform(-0.05, 0.05)

    # Lambda: la ECU enriquece en carga (objetivo ~0,88) y ronda 1,00 en crucero
    lambda_obj = 1.00 - 0.13 * carga
    lambda_val = lambda_obj + random.uniform(-0.025, 0.025)

    # Batería: cargando con el motor en marcha
    vbatt = 13.9 + 0.25 * math.sin(t / 7.0) - 0.3 * carga

    if FORMATO_CLAVE_VALOR:
        campos = {
            'ect': f'{ect:.1f}',
            'rpm': f'{int(rpm)}',
            'vbatt': f'{vbatt:.2f}',
            'pcomb': f'{pcomb:.2f}',
            'taceite': f'{taceite:.1f}',
            'paceite': f'{paceite:.2f}',
            'map': f'{mapa:.0f}',
            'lambda': f'{lambda_val:.3f}',
            'lambda_obj': f'{lambda_obj:.3f}',
            'tps': f'{tps:.0f}',
            'velocidad': f'{velocidad:.0f}',
            'freno_del': f'{freno_del:.1f}',
        }
        mensaje = ';'.join(f'{clave}={valor}' for clave, valor in campos.items())
    else:
        # Formato antiguo de tres campos, el que emite el firmware actual
        mensaje = f"{ect:.1f}|{int(rpm)}|{vbatt:.1f}"

    sock.sendto(mensaje.encode('utf-8'), (UDP_IP, UDP_PORT))

    t += 0.1
    time.sleep(0.05)  # 20 paquetes por segundo, como la ESP32

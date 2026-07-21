import socket
import os
import csv
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.gridspec import GridSpec
from matplotlib.patches import Rectangle
from matplotlib.lines import Line2D
from matplotlib.colors import to_rgb
from collections import deque
import time
import numpy as np
from datetime import datetime

# --- CONFIGURACIÓN DE RED ---
UDP_IP = "0.0.0.0"  # Escuchamos en Todas las interfaces posibles (WiFi, Ethernet...)
UDP_PORT = 4210     # Mismo puerto que usamos para la ESP32
TIMEOUT_SEG = 1.5   # Para ver si existe desconexión

# --- CONFIGURACIÓN DE DATOS ---
FRECUENCIA_HZ = 20  # Frecuencia a la que emite la ESP32 (un paquete cada 50 ms)
VENTANA_SEG = 10    # Segundos de historia visibles en la gráfica de temperatura
MAX_PUNTOS = FRECUENCIA_HZ * VENTANA_SEG

# --- UMBRALES ---
# Temperatura del refrigerante (ECT)
# El rango visible cubre la escala completa: un valor fuera de escala se recortaría
# contra el borde y pasaría por un dato normal
ECT_MIN_VIS = 0
ECT_MAX_VIS = 130
ECT_FRIO = 65        # Por debajo: motor sin temperatura de trabajo
ECT_PRECAUCION = 95  # A partir de aquí: vigilar
ECT_CRITICO = 105    # A partir de aquí: parar

# Régimen de motor
MAX_RPM = 15000      # Fondo de escala del indicador
RPM_CORTE = 12000    # Zona roja / corte de inyección

# Tensión de batería. La escala arranca en 0: si el coche se queda sin tensión
# hay que verlo, no que la barra se quede pegada al mínimo del indicador
BATT_MIN_VIS = 0.0
BATT_MAX_VIS = 16.0
BATT_CRITICO = 11.8   # Por debajo: batería descargada
BATT_BAJO = 12.4      # Por debajo: no está cargando bien
BATT_SOBRECARGA = 14.8  # Por encima: el alternador se está pasando

# --- PALETA (tomada del logo oficial del equipo) ---
FONDO = '#080D1A'      # Navy del logo llevado casi a negro
PANEL = '#0E1526'      # Fondo de los paneles
GRID = '#1C2946'       # Rejilla y separadores
TXT = '#E9EEF8'        # Texto principal
TXT_DIM = '#7C8AA8'    # Etiquetas secundarias
AZUL = '#6E9BE0'       # Azul del logo, aclarado para fondo oscuro
NARANJA = '#F08A4B'    # Naranja del logo, aclarado para fondo oscuro
VERDE = '#35D07F'      # Semáforo: correcto
AMBAR = '#FFB627'      # Semáforo: precaución
ROJO = '#FF4D4D'       # Semáforo: crítico
CIAN = '#4FC3F7'       # Motor frío
APAGADO = '#18213A'    # Segmento / relleno inactivo

RUTA_BASE = os.path.dirname(os.path.abspath(__file__))
RUTA_LOGO = os.path.join(RUTA_BASE, 'logo_gades.png')
RUTA_SESIONES = os.path.join(RUTA_BASE, 'sesiones')
RUTA_CAPTURAS = os.path.join(RUTA_BASE, 'capturas')

# --- ESTADO ---
data_ect = deque([np.nan] * MAX_PUNTOS, maxlen=MAX_PUNTOS)
ultimo_tiempo_dato = 0.0
conectado = False

# Estadísticas de sesión
inicio_sesion = None
ect_max = None
ect_min = None
rpm_pico = 0
batt_min = None
paquetes_ok = 0
paquetes_error = 0
sellos_tiempo = deque(maxlen=FRECUENCIA_HZ * 3)  # Para calcular la frecuencia real

# Grabación a CSV
grabando = False
inicio_grabacion = None
fichero_csv = None
escritor_csv = None
nombre_grabacion = ''
muestras_grabadas = 0

# Pie de pantalla: estado permanente + avisos temporales que lo tapan unos segundos
texto_pie = ''
aviso_pie = ''
aviso_hasta = 0.0

# Configuración del socket UDP
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.setblocking(False)


# --- FUNCIONES DE COLOR ---
def color_ect(temp):
    if temp >= ECT_CRITICO:
        return ROJO
    elif temp >= ECT_PRECAUCION:
        return AMBAR
    elif temp >= ECT_FRIO:
        return VERDE
    else:
        return CIAN


def estado_ect(temp):
    if temp >= ECT_CRITICO:
        return 'CRÍTICO'
    elif temp >= ECT_PRECAUCION:
        return 'PRECAUCIÓN'
    elif temp >= ECT_FRIO:
        return 'NORMAL'
    else:
        return 'EN CALENTAMIENTO'


def color_batt(voltios):
    if voltios < BATT_CRITICO or voltios > BATT_SOBRECARGA:
        return ROJO
    elif voltios < BATT_BAJO:
        return AMBAR
    else:
        return VERDE


def formato_tiempo(segundos):
    if segundos is None:
        return '--:--'
    segundos = int(segundos)
    return f"{segundos // 60:02d}:{segundos % 60:02d}"


def cargar_logo(ruta):
    """Devuelve el logo con los azules oscuros aclarados para que se lea sobre
    fondo oscuro. El naranja corporativo se mantiene intacto."""
    if not os.path.isfile(ruta):
        return None
    img = plt.imread(ruta)
    if img.dtype == np.uint8:
        img = img.astype(float) / 255.0
    else:
        img = img.astype(float).copy()
    if img.ndim != 3 or img.shape[2] < 3:
        return None
    if img.shape[2] == 3:  # Sin canal alfa: se lo añadimos opaco
        img = np.dstack([img, np.ones(img.shape[:2])])

    rgb = img[:, :, :3]
    luminancia = rgb @ np.array([0.299, 0.587, 0.114])
    oscuro = luminancia < 0.30                                     # Navy corporativo
    medio = (luminancia >= 0.30) & (luminancia < 0.58) & (rgb[:, :, 2] > rgb[:, :, 0])
    img[oscuro, :3] = to_rgb(TXT)
    img[medio, :3] = to_rgb(AZUL)
    return img


# --- ESTILO GLOBAL ---
plt.rcParams['toolbar'] = 'None'
# Sin barra de herramientas matplotlib ignora su propio atajo de guardado, así que
# la captura la gestionamos nosotros. Y 'r' viene asignada de fábrica a "reiniciar
# vista": se la quitamos para que sea inequívocamente la tecla de grabar.
plt.rcParams['keymap.save'] = []
plt.rcParams['keymap.home'] = [t for t in plt.rcParams['keymap.home'] if t != 'r']
plt.rcParams['font.family'] = 'sans-serif'
plt.rcParams['font.sans-serif'] = ['Bahnschrift', 'Segoe UI', 'Franklin Gothic Medium', 'DejaVu Sans']
plt.rcParams['figure.facecolor'] = FONDO
plt.rcParams['text.color'] = TXT
plt.rcParams['axes.edgecolor'] = GRID
plt.rcParams['xtick.color'] = TXT_DIM
plt.rcParams['ytick.color'] = TXT_DIM

fig = plt.figure(figsize=(16, 9))
fig.canvas.manager.set_window_title('G26 Telemetry - Formula Gades')

# --- CABECERA ---
logo = cargar_logo(RUTA_LOGO)
if logo is not None:
    alto_logo = 0.42 / 9.0                                  # 0,42 pulgadas de alto
    ancho_logo = (0.42 * (logo.shape[1] / logo.shape[0])) / 16.0
    ax_logo = fig.add_axes([0.035, 0.9315, ancho_logo, alto_logo])
    ax_logo.imshow(logo)
    ax_logo.set_axis_off()
    x_titulo = 0.035 + ancho_logo + 0.018
else:
    x_titulo = 0.035

fig.text(x_titulo, 0.962, 'TELEMETRÍA G26', fontsize=15, fontweight='bold', color=TXT, va='center')
fig.text(x_titulo, 0.937, 'MURO DE BOXES · UNIVERSIDAD DE CÁDIZ', fontsize=8, color=TXT_DIM, va='center')

txt_reloj = fig.text(0.975, 0.962, '--:--:--', fontsize=19, color=TXT, ha='right', va='center')


def crear_led(x, y, color):
    """Los pilotos de estado van como marcadores, no como carácter: la tipografía
    condensada no incluye el glifo del círculo."""
    led = Line2D([x], [y], marker='o', markersize=8, color=color,
                 transform=fig.transFigure, figure=fig)
    fig.add_artist(led)
    return led


led_estado = crear_led(0.7035, 0.937, TXT_DIM)
txt_estado = fig.text(0.713, 0.937, 'SIN SEÑAL', fontsize=10, color=TXT_DIM, va='center')
txt_hz = fig.text(0.830, 0.937, '-- Hz', fontsize=10, color=TXT_DIM, va='center')
led_rec = crear_led(0.9015, 0.937, APAGADO)
txt_rec = fig.text(0.911, 0.937, 'SIN GRABAR', fontsize=10, color=TXT_DIM, va='center')

fig.add_artist(Line2D([0.035, 0.975], [0.916, 0.916], color=GRID, lw=1.2,
                      transform=fig.transFigure))
fig.text(0.035, 0.017, 'R  grabar sesión        S  captura        F  pantalla completa        Q  salir',
         fontsize=8.5, color=TXT_DIM, va='center')
txt_fichero = fig.text(0.975, 0.017, '', fontsize=8.5, color=TXT_DIM, ha='right', va='center')

# --- REJILLA PRINCIPAL ---
gs = GridSpec(3, 2, height_ratios=[0.62, 3.1, 1.05], width_ratios=[1, 3.3],
              left=0.035, right=0.975, top=0.895, bottom=0.048,
              hspace=0.30, wspace=0.09)


def preparar_panel(ax, color_acento=None):
    """Fondo de panel + barra de acento a la izquierda, el mismo lenguaje visual
    que las tarjetas de la plataforma web."""
    ax.set_facecolor(PANEL)
    for spine in ax.spines.values():
        spine.set_visible(False)
    ax.set_xticks([])
    ax.set_yticks([])
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    if color_acento is None:
        return None
    acento = Rectangle((0, 0), 0.011, 1, transform=ax.transAxes,
                       facecolor=color_acento, zorder=5, clip_on=False)
    ax.add_patch(acento)
    return acento


# --- 1. TIRA DE LUCES DE CAMBIO (RPM) ---
ax_rpm = fig.add_subplot(gs[0, :])
ax_rpm.set_facecolor(FONDO)
for spine in ax_rpm.spines.values():
    spine.set_visible(False)
ax_rpm.set_xticks([])
ax_rpm.set_yticks([])
ax_rpm.set_xlim(0, 100)
ax_rpm.set_ylim(0, 1)

N_SEGMENTOS = 22
ANCHO_TIRA = 79.0
paso = ANCHO_TIRA / N_SEGMENTOS
segmentos = []
colores_segmento = []
for i in range(N_SEGMENTOS):
    fraccion = (i + 1) / N_SEGMENTOS
    if fraccion <= 0.55:
        color = VERDE
    elif fraccion <= RPM_CORTE / MAX_RPM:
        color = AMBAR
    else:
        color = ROJO
    colores_segmento.append(color)
    seg = Rectangle((i * paso, 0.30), paso * 0.76, 0.48, facecolor=APAGADO)
    ax_rpm.add_patch(seg)
    segmentos.append(seg)

ax_rpm.text(0, 0.13, '0', fontsize=7.5, color=TXT_DIM, va='top')
ax_rpm.text(ANCHO_TIRA * (RPM_CORTE / MAX_RPM), 0.13, f'{RPM_CORTE // 1000}.000',
            fontsize=7.5, color=ROJO, va='top', ha='center')
ax_rpm.text(ANCHO_TIRA, 0.13, f'{MAX_RPM // 1000}.000', fontsize=7.5, color=TXT_DIM,
            va='top', ha='right')

ax_rpm.text(100, 0.97, 'RPM', fontsize=9, color=TXT_DIM, ha='right', va='top')
txt_rpm = ax_rpm.text(100, 0.48, '0', fontsize=28, fontweight='bold', color=TXT_DIM,
                      ha='right', va='center')

# --- 2. LECTURA GRANDE DE TEMPERATURA ---
ax_ect_num = fig.add_subplot(gs[1, 0])
acento_ect = preparar_panel(ax_ect_num, TXT_DIM)
ax_ect_num.text(0.5, 0.90, 'ECT', fontsize=13, fontweight='bold', color=TXT_DIM, ha='center', va='center')
ax_ect_num.text(0.5, 0.835, 'TEMPERATURA REFRIGERANTE', fontsize=7.5, color=TXT_DIM, ha='center', va='center')
txt_ect = ax_ect_num.text(0.5, 0.55, '--', fontsize=76, fontweight='bold', color=TXT_DIM,
                          ha='center', va='center')
ax_ect_num.text(0.5, 0.345, '°C', fontsize=18, color=TXT_DIM, ha='center', va='center')
txt_ect_estado = ax_ect_num.text(0.5, 0.16, 'SIN SEÑAL', fontsize=11, fontweight='bold',
                                 color=TXT_DIM, ha='center', va='center')

# --- 3. GRÁFICA DE TEMPERATURA ---
ax_ect = fig.add_subplot(gs[1, 1])
ax_ect.set_facecolor(PANEL)
ax_ect.set_ylim(ECT_MIN_VIS, ECT_MAX_VIS)
ax_ect.set_xlim(0, MAX_PUNTOS)
for lado in ('top', 'right'):
    ax_ect.spines[lado].set_visible(False)
for lado in ('bottom', 'left'):
    ax_ect.spines[lado].set_color(GRID)

# Bandas de referencia: se ve de un vistazo en qué zona está el motor
ax_ect.axhspan(ECT_MIN_VIS, ECT_FRIO, color=CIAN, alpha=0.05, zorder=0)
ax_ect.axhspan(ECT_FRIO, ECT_PRECAUCION, color=VERDE, alpha=0.07, zorder=0)
ax_ect.axhspan(ECT_PRECAUCION, ECT_CRITICO, color=AMBAR, alpha=0.11, zorder=0)
ax_ect.axhspan(ECT_CRITICO, ECT_MAX_VIS, color=ROJO, alpha=0.14, zorder=0)
ax_ect.axhline(ECT_PRECAUCION, color=AMBAR, lw=1, ls='--', alpha=0.5, zorder=1)
ax_ect.axhline(ECT_CRITICO, color=ROJO, lw=1, ls='--', alpha=0.6, zorder=1)
ax_ect.text(MAX_PUNTOS * 0.005, ECT_CRITICO + 1.5, f'LÍMITE {ECT_CRITICO} °C', fontsize=7.5,
            color=ROJO, ha='left', va='bottom', zorder=5,
            bbox=dict(facecolor=PANEL, edgecolor='none', boxstyle='square,pad=0.25'))

ax_ect.set_yticks(range(20, 131, 20))  # Sin el 0, que chocaría con la etiqueta del eje X
ax_ect.set_ylabel('°C', fontsize=10, color=TXT_DIM)
ax_ect.set_xticks([0, MAX_PUNTOS * 0.25, MAX_PUNTOS * 0.5, MAX_PUNTOS * 0.75, MAX_PUNTOS])
ax_ect.set_xticklabels([f'-{VENTANA_SEG:g} s'.replace('.', ','),
                        f'-{VENTANA_SEG * 0.75:g} s'.replace('.', ','),
                        f'-{VENTANA_SEG * 0.5:g} s'.replace('.', ','),
                        f'-{VENTANA_SEG * 0.25:g} s'.replace('.', ','), 'ahora'],
                       fontsize=8)
ax_ect.grid(True, color=GRID, alpha=0.55, linestyle='--', lw=0.7, zorder=1)
ax_ect.tick_params(length=0, labelsize=9)

line_ect, = ax_ect.plot([], [], color=TXT, lw=2.2, zorder=3, solid_capstyle='round')
punto_ect, = ax_ect.plot([], [], marker='o', markersize=7, color=TXT, zorder=4)

# --- 4. BATERÍA ---
ax_batt = fig.add_subplot(gs[2, 0])
preparar_panel(ax_batt, NARANJA)
ax_batt.text(0.09, 0.74, 'BATERÍA', fontsize=10, fontweight='bold', color=TXT_DIM, va='center')
txt_batt = ax_batt.text(0.93, 0.72, '-- V', fontsize=22, fontweight='bold', color=TXT_DIM,
                        ha='right', va='center')

X_BARRA, ANCHO_BARRA, Y_BARRA, ALTO_BARRA = 0.09, 0.84, 0.30, 0.22


def x_voltios(v):
    """Posición horizontal, en coordenadas del panel, de una tensión dada."""
    fraccion = (v - BATT_MIN_VIS) / (BATT_MAX_VIS - BATT_MIN_VIS)
    return X_BARRA + ANCHO_BARRA * max(0.0, min(1.0, fraccion))


ax_batt.add_patch(Rectangle((X_BARRA, Y_BARRA), ANCHO_BARRA, ALTO_BARRA, facecolor=APAGADO))
bar_batt = Rectangle((X_BARRA, Y_BARRA), 0, ALTO_BARRA, facecolor=NARANJA)
ax_batt.add_patch(bar_batt)

# Marca de la zona de trabajo: con la escala completa el ojo necesita una
# referencia de dónde debería estar la aguja, no solo el valor absoluto
y_zona = Y_BARRA + ALTO_BARRA + 0.05
ax_batt.add_line(Line2D([x_voltios(BATT_CRITICO), x_voltios(BATT_SOBRECARGA)], [y_zona, y_zona],
                        color=VERDE, lw=2.5, alpha=0.65, solid_capstyle='butt'))
for v in (BATT_CRITICO, BATT_SOBRECARGA):
    ax_batt.add_line(Line2D([x_voltios(v)] * 2, [y_zona - 0.035, y_zona + 0.035],
                            color=VERDE, lw=1.2, alpha=0.65))
ax_batt.text(x_voltios(BATT_SOBRECARGA) + 0.02, y_zona, 'ZONA ÚTIL', fontsize=6,
             color=TXT_DIM, va='center', ha='left')

for v in range(0, int(BATT_MAX_VIS) + 1, 4):
    x = x_voltios(v)
    ax_batt.add_line(Line2D([x, x], [Y_BARRA - 0.06, Y_BARRA], color=GRID, lw=1))
    ax_batt.text(x, 0.20, str(v), fontsize=7, color=TXT_DIM, ha='center', va='top')

# --- 5. ESTADÍSTICAS DE SESIÓN ---
ax_stats = fig.add_subplot(gs[2, 1])
preparar_panel(ax_stats, AZUL)
ETIQUETAS_STATS = ['ECT MÁX', 'ECT MÍN', 'RPM PICO', 'BATT MÍN', 'SESIÓN', 'PAQUETES']
txt_stats = []
for i, etiqueta in enumerate(ETIQUETAS_STATS):
    cx = (i + 0.5) / len(ETIQUETAS_STATS)
    ax_stats.text(cx, 0.70, etiqueta, fontsize=8.5, color=TXT_DIM, ha='center', va='center')
    txt_stats.append(ax_stats.text(cx, 0.36, '--', fontsize=19, fontweight='bold',
                                   color=TXT, ha='center', va='center'))
    if i:
        x_sep = i / len(ETIQUETAS_STATS)
        ax_stats.add_line(Line2D([x_sep, x_sep], [0.22, 0.80], color=GRID, lw=1))


# --- GRABACIÓN A CSV ---
def alternar_grabacion():
    """Arranca o detiene el volcado de la sesión a CSV. El fichero resultante es
    el que se sube después a la plataforma web como registro de telemetría."""
    global grabando, fichero_csv, escritor_csv, inicio_grabacion, nombre_grabacion
    global muestras_grabadas, texto_pie

    if grabando:
        fichero_csv.close()
        fichero_csv = None
        escritor_csv = None
        grabando = False
        texto_pie = f'Guardado: sesiones/{nombre_grabacion}  ({muestras_grabadas} muestras)'
        return

    # Descartamos lo que estuviera encolado: son muestras anteriores a pulsar REC
    try:
        while True:
            sock.recvfrom(1024)
    except BlockingIOError:
        pass

    os.makedirs(RUTA_SESIONES, exist_ok=True)
    nombre_grabacion = f"sesion_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    fichero_csv = open(os.path.join(RUTA_SESIONES, nombre_grabacion), 'w', newline='', encoding='utf-8')
    escritor_csv = csv.writer(fichero_csv)
    escritor_csv.writerow(['n_muestra', 'tiempo_s', 'hora', 'ect_c', 'rpm', 'vbatt_v'])
    inicio_grabacion = time.time()
    muestras_grabadas = 0
    grabando = True
    texto_pie = f'Grabando en sesiones/{nombre_grabacion}'


def mostrar_aviso(texto, segundos=4.0):
    """Mensaje temporal en el pie, que después deja ver de nuevo el estado fijo."""
    global aviso_pie, aviso_hasta
    aviso_pie = texto
    aviso_hasta = time.time() + segundos


def capturar_pantalla():
    """Guarda la pantalla tal cual se ve. Sin diálogo de fichero: en boxes no hay
    tiempo ni ratón cómodo para navegar por carpetas."""
    os.makedirs(RUTA_CAPTURAS, exist_ok=True)
    nombre = f"captura_{datetime.now().strftime('%Y%m%d_%H%M%S')}.png"
    fig.savefig(os.path.join(RUTA_CAPTURAS, nombre), dpi=110, facecolor=fig.get_facecolor())
    mostrar_aviso(f'Captura guardada: capturas/{nombre}')


def al_pulsar_tecla(event):
    if event.key == 'r':
        alternar_grabacion()
    elif event.key == 's':
        capturar_pantalla()


fig.canvas.mpl_connect('key_press_event', al_pulsar_tecla)


def update(frame):
    global ultimo_tiempo_dato, conectado, inicio_sesion
    global ect_max, ect_min, rpm_pico, batt_min, paquetes_ok, paquetes_error
    global muestras_grabadas

    txt_reloj.set_text(datetime.now().strftime('%H:%M:%S'))
    ahora = time.time()

    val_ect = val_rpm = val_batt = None

    # --- LECTURA DEL SOCKET (vaciamos todo lo pendiente) ---
    try:
        while True:
            data, addr = sock.recvfrom(1024)
            partes = data.decode('utf-8').split('|')
            if len(partes) != 3:
                paquetes_error += 1
                continue
            try:
                val_ect = float(partes[0])
                val_rpm = float(partes[1])
                val_batt = float(partes[2])
            except ValueError:
                paquetes_error += 1
                continue

            # Sello propio de cada paquete: en una misma pasada se vacían varios
            # y compartir el instante del frame duplicaría marcas en el CSV
            t_paquete = time.time()
            ultimo_tiempo_dato = t_paquete
            paquetes_ok += 1
            sellos_tiempo.append(t_paquete)
            if inicio_sesion is None:
                inicio_sesion = t_paquete

            data_ect.append(val_ect)

            # Estadísticas de sesión
            ect_max = val_ect if ect_max is None else max(ect_max, val_ect)
            ect_min = val_ect if ect_min is None else min(ect_min, val_ect)
            rpm_pico = max(rpm_pico, val_rpm)
            batt_min = val_batt if batt_min is None else min(batt_min, val_batt)

            if grabando:
                muestras_grabadas += 1
                escritor_csv.writerow([muestras_grabadas,
                                       f'{t_paquete - inicio_grabacion:.3f}',
                                       datetime.now().strftime('%H:%M:%S.%f')[:-3],
                                       f'{val_ect:.1f}', int(val_rpm), f'{val_batt:.2f}'])
                fichero_csv.flush()
    except BlockingIOError:
        pass
    except UnicodeDecodeError:
        paquetes_error += 1

    conectado = (ahora - ultimo_tiempo_dato) <= TIMEOUT_SEG

    # --- ESTADO DEL ENLACE ---
    if conectado:
        hz = len([t for t in sellos_tiempo if ahora - t <= 1.0])
        txt_estado.set_text('EN LÍNEA')
        txt_estado.set_color(VERDE)
        led_estado.set_color(VERDE)
        txt_hz.set_text(f'{hz} Hz')
        txt_hz.set_color(TXT_DIM if hz >= FRECUENCIA_HZ * 0.7 else AMBAR)
    else:
        data_ect.append(np.nan)  # Corta la línea en vez de dibujar un cero falso
        txt_estado.set_text('SIN SEÑAL')
        txt_estado.set_color(ROJO)
        led_estado.set_color(ROJO)
        txt_hz.set_text('-- Hz')
        txt_hz.set_color(TXT_DIM)

    # --- TEMPERATURA ---
    if conectado and val_ect is not None:
        color = color_ect(val_ect)
        txt_ect.set_text(f'{val_ect:.1f}')
        txt_ect.set_color(color)
        txt_ect_estado.set_text(estado_ect(val_ect))
        txt_ect_estado.set_color(color)
        acento_ect.set_facecolor(color)
        line_ect.set_color(color)
        punto_ect.set_color(color)
        punto_ect.set_data([MAX_PUNTOS - 1], [val_ect])
    elif not conectado:
        txt_ect.set_text('--')
        txt_ect.set_color(TXT_DIM)
        txt_ect_estado.set_text('SIN SEÑAL')
        txt_ect_estado.set_color(TXT_DIM)
        acento_ect.set_facecolor(TXT_DIM)
        line_ect.set_color(TXT_DIM)
        punto_ect.set_data([], [])

    line_ect.set_data(range(MAX_PUNTOS), data_ect)

    # --- RPM: luces de cambio ---
    if conectado and val_rpm is not None:
        val_rpm = max(0, min(MAX_RPM, val_rpm))
        encendidos = int(round((val_rpm / MAX_RPM) * N_SEGMENTOS))
        en_corte = val_rpm >= RPM_CORTE
        destello = en_corte and (frame // 4) % 2 == 0
        for i, seg in enumerate(segmentos):
            if destello:
                seg.set_facecolor(ROJO)
            elif i < encendidos:
                seg.set_facecolor(colores_segmento[i])
            else:
                seg.set_facecolor(APAGADO)
        txt_rpm.set_text(f'{int(val_rpm):,}'.replace(',', '.'))
        txt_rpm.set_color(ROJO if en_corte else TXT)
    elif not conectado:
        for seg in segmentos:
            seg.set_facecolor(APAGADO)
        txt_rpm.set_text('--')
        txt_rpm.set_color(TXT_DIM)

    # --- BATERÍA ---
    if conectado and val_batt is not None:
        fraccion = (val_batt - BATT_MIN_VIS) / (BATT_MAX_VIS - BATT_MIN_VIS)
        bar_batt.set_width(ANCHO_BARRA * max(0.0, min(1.0, fraccion)))
        bar_batt.set_facecolor(color_batt(val_batt))
        txt_batt.set_text(f'{val_batt:.1f} V')
        txt_batt.set_color(color_batt(val_batt))
    elif not conectado:
        bar_batt.set_width(0)
        txt_batt.set_text('-- V')
        txt_batt.set_color(TXT_DIM)

    # --- ESTADÍSTICAS ---
    txt_stats[0].set_text('--' if ect_max is None else f'{ect_max:.1f}')
    txt_stats[1].set_text('--' if ect_min is None else f'{ect_min:.1f}')
    txt_stats[2].set_text(f'{int(rpm_pico):,}'.replace(',', '.') if rpm_pico else '--')
    txt_stats[3].set_text('--' if batt_min is None else f'{batt_min:.1f}')
    txt_stats[4].set_text(formato_tiempo(None if inicio_sesion is None else ahora - inicio_sesion))
    txt_stats[5].set_text(f'{paquetes_ok:,}'.replace(',', '.'))
    txt_stats[0].set_color(TXT if ect_max is None else color_ect(ect_max))
    txt_stats[3].set_color(TXT if batt_min is None else color_batt(batt_min))
    txt_stats[5].set_color(AMBAR if paquetes_error else TXT)

    # --- INDICADOR DE GRABACIÓN ---
    if grabando:
        parpadeo = (frame // 6) % 2 == 0
        txt_rec.set_text(f'REC {formato_tiempo(ahora - inicio_grabacion)}')
        txt_rec.set_color(ROJO)
        led_rec.set_color(ROJO if parpadeo else '#4A1414')
    else:
        txt_rec.set_text('SIN GRABAR')
        txt_rec.set_color(TXT_DIM)
        led_rec.set_color(APAGADO)

    # --- PIE: el aviso temporal tiene prioridad sobre el estado fijo ---
    txt_fichero.set_text(aviso_pie if ahora < aviso_hasta else texto_pie)

    return ()


ani = animation.FuncAnimation(fig, update, interval=60, blit=False, cache_frame_data=False)

# Arrancamos maximizado y sin barra de herramientas (pantalla de boxes)
try:
    fig.canvas.manager.window.state('zoomed')
except Exception:
    try:
        fig.canvas.manager.full_screen_toggle()
    except Exception:
        pass

plt.show()

if fichero_csv is not None:
    fichero_csv.close()

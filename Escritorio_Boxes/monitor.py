import socket
import os
import csv
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.gridspec import GridSpec
from matplotlib.patches import Rectangle, Polygon
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
VENTANA_SEG = 10    # Segundos de historia visibles en las gráficas
MAX_PUNTOS = FRECUENCIA_HZ * VENTANA_SEG

# --- FORMATO DEL PAQUETE UDP ---
# Se admiten dos formatos y se distinguen solos:
#
#   1. "87.3|9200|14.2"   ->   ECT | RPM | BATERÍA
#
#   2. Nuevo (clave-valor, para cuando el firmware envíe el resto de canales):
#        "ect=87.3;rpm=9200;vbatt=14.2;velocidad=64;tps=45;freno_del=8.5;
#         pcomb=3.6;taceite=104;paceite=4.2;map=98;lambda=0.88;lambda_obj=0.88"
CLAVES_LEGADO = ('ect', 'rpm', 'vbatt')

# --- PALETA (tomada del logo oficial del equipo) ---
FONDO = '#080D1A'      # Navy del logo llevado casi a negro
PANEL = '#0E1526'      # Fondo de los paneles
GRID = '#1C2946'       # Rejilla y separadores
TXT = '#E9EEF8'        # Texto principal
TXT_DIM = '#7C8AA8'    # Etiquetas secundarias
AZUL = '#6E9BE0'       # Azul del logo, aclarado para fondo oscuro
VERDE = '#35D07F'      # Semáforo: correcto
AMBAR = '#FFB627'      # Semáforo: precaución
ROJO = '#FF4D4D'       # Semáforo: crítico
CIAN = '#4FC3F7'       # Motor frío
APAGADO = '#18213A'    # Segmento / relleno inactivo

# --- RÉGIMEN DE MOTOR ---
MAX_RPM = 13000      # Fondo de escala del indicador
RPM_CORTE = 12000    # Zona roja / corte de inyección

# --- PEDALES ---
VENTANA_PEDALES_SEG = 7                      # Segundos visibles en la traza de pedales
PUNTOS_PEDALES = FRECUENCIA_HZ * VENTANA_PEDALES_SEG
# El sensor de freno es de PRESIÓN (bar). Este es el valor que se dibuja como el
# 100 % de la traza. AJUSTAR cuando se mida en pista la frenada más fuerte.
FRENO_PRESION_MAX = 50.0

# --- DEFINICIÓN DE CANALES ---
# Cada canal declara su rango visible, sus zonas de color y una referencia
# opcional que se dibuja como línea en la gráfica. Las 'zonas' son pares
# (límite superior, color): se recorre en orden y gana la primera que supera
# al valor. AJUSTAR ESTOS UMBRALES A VUESTRO MOTOR.
CANALES = {
    'ect': dict(
        etiqueta='ECT', descripcion='TEMP. REFRIGERANTE', unidad='°C',
        vmin=0, vmax=130, decimales=1, referencia=None,
        zonas=[(65, CIAN), (95, VERDE), (105, AMBAR), (float('inf'), ROJO)]),
    'taceite': dict(
        etiqueta='T. ACEITE', descripcion='TEMP. DE ACEITE', unidad='°C',
        vmin=0, vmax=160, decimales=1, referencia=None,
        zonas=[(60, CIAN), (125, VERDE), (135, AMBAR), (float('inf'), ROJO)]),
    'pcomb': dict(
        etiqueta='P. COMBUSTIBLE', descripcion='PRESIÓN COMBUSTIBLE', unidad='bar',
        vmin=0, vmax=6, decimales=2, referencia=3.5,
        zonas=[(2.5, ROJO), (3.0, AMBAR), (4.5, VERDE), (float('inf'), AMBAR)]),
    'paceite': dict(
        etiqueta='P. ACEITE', descripcion='PRESIÓN DE ACEITE', unidad='bar',
        vmin=0, vmax=8, decimales=2, referencia=None,
        zonas=[(1.0, ROJO), (2.0, AMBAR), (6.5, VERDE), (float('inf'), AMBAR)]),
    'lambda': dict(
        etiqueta='LAMBDA', descripcion='MEZCLA (λ)', unidad='',
        vmin=0.70, vmax=1.30, decimales=2, referencia=1.00,
        zonas=[(0.75, ROJO), (0.80, AMBAR), (1.02, VERDE),
               (1.08, AMBAR), (float('inf'), ROJO)]),
    'map': dict(
        etiqueta='MAP', descripcion='PRESIÓN DE ADMISIÓN', unidad='kPa',
        vmin=0, vmax=120, decimales=0, referencia=101.3,  # Presión atmosférica
        zonas=[(float('inf'), AZUL)]),                    # Es carga, no alarma
    'vbatt': dict(
        etiqueta='BATERÍA', descripcion='TENSIÓN DE BATERÍA', unidad='V',
        vmin=0, vmax=16, decimales=1, referencia=None,
        zonas=[(11.8, ROJO), (12.4, AMBAR), (14.8, VERDE), (float('inf'), ROJO)]),
    'tps': dict(
        etiqueta='TPS', descripcion='ACELERADOR', unidad='%',
        vmin=0, vmax=100, decimales=0, referencia=None,
        zonas=[(float('inf'), AZUL)]),
    # Frenos: sensores de PRESIÓN. Se guardan en bar (real) y en la traza se
    # normalizan a 0-100 % contra FRENO_PRESION_MAX. Solo el delantero está
    # instalado; el trasero queda declarado a la espera de montarse.
    'freno_del': dict(
        etiqueta='FRENO DEL.', descripcion='PRESIÓN FRENO DELANTERO', unidad='bar',
        vmin=0, vmax=FRENO_PRESION_MAX, decimales=1, referencia=None,
        zonas=[(float('inf'), ROJO)]),
    'freno_tra': dict(
        etiqueta='FRENO TRA.', descripcion='PRESIÓN FRENO TRASERO', unidad='bar',
        vmin=0, vmax=FRENO_PRESION_MAX, decimales=1, referencia=None,
        zonas=[(float('inf'), ROJO)]),
    'velocidad': dict(
        etiqueta='VELOCIDAD', descripcion='VELOCIDAD', unidad='km/h',
        vmin=0, vmax=160, decimales=0, referencia=None,
        zonas=[(float('inf'), AZUL)]),
    'rpm': dict(
        etiqueta='RPM', descripcion='RÉGIMEN DE MOTOR', unidad='',
        vmin=0, vmax=MAX_RPM, decimales=0, referencia=None,
        zonas=[(RPM_CORTE, VERDE), (float('inf'), ROJO)]),
}

# Canales que ocupan las tarjetas inferiores, en orden. Su posición es además la
# tecla que lleva ese canal a la gráfica grande (1 = primera tarjeta, etc.).
TARJETAS = ['ect', 'taceite', 'paceite', 'pcomb', 'lambda', 'map', 'vbatt']
CANAL_FOCO_INICIAL = 'ect'

# Lambda se colorea por desviación respecto al objetivo que manda la ECU, no
# por umbrales fijos: mezcla pobre funde pistones, rica solo pierde potencia
LAMBDA_POBRE_CRITICO = 0.06
LAMBDA_POBRE_AVISO = 0.03
LAMBDA_RICA_AVISO = -0.08

RUTA_BASE = os.path.dirname(os.path.abspath(__file__))
RUTA_LOGO = os.path.join(RUTA_BASE, 'logo_gades.png')
RUTA_SESIONES = os.path.join(RUTA_BASE, 'sesiones')
RUTA_CAPTURAS = os.path.join(RUTA_BASE, 'capturas')

# --- ESTADO ---
historial = {clave: deque([np.nan] * MAX_PUNTOS, maxlen=MAX_PUNTOS) for clave in CANALES}
ultima_lectura = {}          # Último valor recibido de cada canal
maximos = {}                 # Máximo de sesión por canal
minimos = {}                 # Mínimo de sesión por canal
canal_foco = CANAL_FOCO_INICIAL

ultimo_tiempo_dato = 0.0
conectado = False
inicio_sesion = None
paquetes_ok = 0
paquetes_error = 0
sellos_tiempo = deque(maxlen=FRECUENCIA_HZ * 3)

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

# Orden de las columnas del CSV de sesión
COLUMNAS_CSV = ['ect', 'rpm', 'velocidad', 'vbatt', 'tps', 'freno_del', 'freno_tra',
                'pcomb', 'taceite', 'paceite', 'map', 'lambda']

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.setblocking(False)


# --- LÓGICA DE DATOS ---
def parsear_mensaje(msg):
    """Convierte el paquete UDP en un diccionario canal -> valor.

    Acepta el formato clave-valor y el posicional antiguo, para que el monitor
    siga funcionando con el firmware que ya está flasheado en el coche."""
    if '=' in msg:
        lectura = {}
        for par in msg.replace(';', '|').split('|'):
            clave, sep, valor = par.partition('=')
            if not sep:
                continue
            try:
                lectura[clave.strip().lower()] = float(valor)
            except ValueError:
                continue    # Una clave ilegible no invalida el resto del paquete
        if not lectura:
            raise ValueError('paquete clave-valor sin ningún campo legible')
        return lectura

    partes = msg.split('|')
    if len(partes) != len(CLAVES_LEGADO):
        raise ValueError(f'se esperaban {len(CLAVES_LEGADO)} campos, llegaron {len(partes)}')
    return {clave: float(valor) for clave, valor in zip(CLAVES_LEGADO, partes)}


def color_de(clave, valor):
    """Color semáforo de un valor según las zonas declaradas para su canal."""
    if valor is None or (isinstance(valor, float) and np.isnan(valor)):
        return TXT_DIM
    if clave == 'lambda':
        return color_lambda(valor, ultima_lectura.get('lambda_obj'))
    for limite, color in CANALES[clave]['zonas']:
        if valor < limite:
            return color
    return TXT


def color_lambda(valor, objetivo):
    """Mezcla pobre es peligrosa (funde pistones); rica solo desperdicia."""
    if not objetivo or objetivo <= 0:
        objetivo = CANALES['lambda']['referencia']
    desviacion = valor - objetivo
    if desviacion >= LAMBDA_POBRE_CRITICO:
        return ROJO
    if desviacion >= LAMBDA_POBRE_AVISO:
        return AMBAR
    if desviacion <= LAMBDA_RICA_AVISO:
        return AMBAR
    return VERDE


def referencia_de(clave):
    """Valor de referencia del canal. En lambda lo manda la propia ECU."""
    if clave == 'lambda':
        return ultima_lectura.get('lambda_obj') or CANALES['lambda']['referencia']
    return CANALES[clave]['referencia']


def formatear(clave, valor):
    if valor is None or (isinstance(valor, float) and np.isnan(valor)):
        return '--'
    decimales = CANALES[clave]['decimales']
    if decimales == 0:
        return f'{int(round(valor)):,}'.replace(',', '.')
    return f'{valor:.{decimales}f}'


def valor_csv(clave):
    """Valor para el fichero de sesión: sin separador de millares, que rompería
    el CSV, y con los decimales propios del canal (RPM y velocidad son enteros)."""
    valor = ultima_lectura.get(clave)
    if valor is None:
        return ''
    decimales = CANALES[clave]['decimales'] if clave in CANALES else 0
    return f'{valor:.{decimales}f}'


def formato_tiempo(segundos):
    if segundos is None:
        return '--:--'
    segundos = int(segundos)
    return f'{segundos // 60:02d}:{segundos % 60:02d}'


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
    if img.shape[2] == 3:
        img = np.dstack([img, np.ones(img.shape[:2])])

    rgb = img[:, :, :3]
    luminancia = rgb @ np.array([0.299, 0.587, 0.114])
    oscuro = luminancia < 0.30
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


def preparar_panel(ax, color_acento=None, fondo=PANEL):
    """Fondo de panel + barra de acento a la izquierda, el mismo lenguaje visual
    que las tarjetas de la plataforma web."""
    ax.set_facecolor(fondo)
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


# --- CABECERA ---
logo = cargar_logo(RUTA_LOGO)
if logo is not None:
    alto_logo = 0.42 / 9.0
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

# Banda de alarma: con tantos canales ya no se pueden vigilar todos a la vez,
# así que la alarma viene a buscarte en lugar de esperar a que mires la casilla
txt_alarma = fig.text(0.50, 0.952, '', fontsize=12, fontweight='bold', color=TXT,
                      ha='center', va='center', zorder=10,
                      bbox=dict(facecolor=ROJO, edgecolor='none', boxstyle='square,pad=0.45'))
txt_alarma.set_visible(False)

fig.add_artist(Line2D([0.035, 0.975], [0.916, 0.916], color=GRID, lw=1.2,
                      transform=fig.transFigure))

txt_sesion = fig.text(0.035, 0.017, '', fontsize=8.5, color=TXT_DIM, va='center')
fig.text(0.50, 0.017, 'R  grabar        S  captura        1-7  gráfica        F  pantalla completa        Q  salir',
         fontsize=8.5, color=TXT_DIM, ha='center', va='center')
txt_fichero = fig.text(0.975, 0.017, '', fontsize=8.5, color=TXT_DIM, ha='right', va='center')

# --- BANDA A: LO QUE HACE EL PILOTO (RPM + VELOCIDAD + PEDALES) ---
gs_a = GridSpec(1, 3, width_ratios=[2.6, 0.8, 1.7],
                left=0.035, right=0.975, top=0.893, bottom=0.712, wspace=0.030)

# A1. Luces de cambio
ax_rpm = fig.add_subplot(gs_a[0, 0])
preparar_panel(ax_rpm)
ax_rpm.set_xlim(0, 100)

N_SEGMENTOS = 22
ANCHO_TIRA = 78.0
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
    seg = Rectangle((2 + i * paso, 0.34), paso * 0.76, 0.40, facecolor=APAGADO)
    ax_rpm.add_patch(seg)
    segmentos.append(seg)

ax_rpm.text(2, 0.26, '0', fontsize=7.5, color=TXT_DIM, va='top')
ax_rpm.text(2 + ANCHO_TIRA * (RPM_CORTE / MAX_RPM), 0.26, f'{RPM_CORTE // 1000}.000',
            fontsize=7.5, color=ROJO, va='top', ha='center')
ax_rpm.text(2 + ANCHO_TIRA, 0.26, f'{MAX_RPM // 1000}.000', fontsize=7.5, color=TXT_DIM,
            va='top', ha='right')
ax_rpm.text(2, 0.88, 'RPM', fontsize=9, color=TXT_DIM, va='center')
txt_rpm = ax_rpm.text(98, 0.55, '--', fontsize=30, fontweight='bold', color=TXT_DIM,
                      ha='right', va='center')

# A2. Velocidad: número grande, hereda el hueco glanceable que dejó la marcha
ax_vel = fig.add_subplot(gs_a[0, 1])
preparar_panel(ax_vel, AZUL)
ax_vel.text(0.5, 0.86, 'VELOCIDAD', fontsize=9, color=TXT_DIM, ha='center', va='center')
txt_vel = ax_vel.text(0.5, 0.45, '--', fontsize=54, fontweight='bold', color=TXT_DIM,
                      ha='center', va='center')
ax_vel.text(0.5, 0.13, 'km/h', fontsize=12, color=TXT_DIM, ha='center', va='center')

# A3. Traza de pedales: acelerador (verde) y freno (rojo) oscilando de 0 a 100 %,
# como en la telemetría real. El freno es presión (bar) normalizada a % en pantalla.
ax_ped = fig.add_subplot(gs_a[0, 2])
ax_ped.set_facecolor(PANEL)
for lado in ('top', 'right'):
    ax_ped.spines[lado].set_visible(False)
for lado in ('bottom', 'left'):
    ax_ped.spines[lado].set_color(GRID)
ax_ped.set_xlim(0, PUNTOS_PEDALES)
ax_ped.set_ylim(0, 105)
ax_ped.set_yticks([0, 50, 100])
ax_ped.set_yticklabels(['0', '50', '100'], fontsize=7)
ax_ped.set_xticks([0, PUNTOS_PEDALES / 2, PUNTOS_PEDALES])
ax_ped.set_xticklabels([f'-{VENTANA_PEDALES_SEG:g} s'.replace('.', ','),
                        f'-{VENTANA_PEDALES_SEG / 2:g} s'.replace('.', ','), 'ahora'], fontsize=7)
ax_ped.tick_params(length=0, labelsize=7.5)
ax_ped.grid(True, color=GRID, alpha=0.5, linestyle='--', lw=0.6, zorder=1)
ax_ped.text(PUNTOS_PEDALES * 0.015, 99, 'PEDALES', fontsize=9, color=TXT_DIM, va='top', zorder=5)

fill_tps = Polygon([[0, 0], [0, 0]], closed=True, facecolor=VERDE, alpha=0.22,
                   edgecolor='none', zorder=2)
fill_freno = Polygon([[0, 0], [0, 0]], closed=True, facecolor=ROJO, alpha=0.20,
                     edgecolor='none', zorder=2)
ax_ped.add_patch(fill_tps)
ax_ped.add_patch(fill_freno)
line_tps, = ax_ped.plot([], [], color=VERDE, lw=2, zorder=4, solid_capstyle='round')
line_freno, = ax_ped.plot([], [], color=ROJO, lw=2, zorder=3, solid_capstyle='round')
_caja_ped = dict(facecolor=PANEL, alpha=0.65, edgecolor='none', boxstyle='square,pad=0.2')
txt_ped_tps = ax_ped.text(PUNTOS_PEDALES * 0.985, 80, 'ACEL --', fontsize=10.5,
                          fontweight='bold', color=VERDE, ha='right', va='center',
                          zorder=6, bbox=_caja_ped)
txt_ped_freno = ax_ped.text(PUNTOS_PEDALES * 0.985, 62, 'FRENO --', fontsize=10.5,
                            fontweight='bold', color=ROJO, ha='right', va='center',
                            zorder=6, bbox=_caja_ped)


def area_pedales(xs, ys):
    """Vértices del polígono relleno bajo una curva de pedal, saltándose los NaN
    (que aparecen al desconectar), para que el relleno no se rompa."""
    m = np.isfinite(ys)
    if not m.any():
        return [[0, 0], [0, 0]]
    xf, yf = xs[m], ys[m]
    return [(xf[0], 0)] + list(zip(xf, yf)) + [(xf[-1], 0)]

# --- BANDA B: CANAL CON FOCO (número grande + gráfica) ---
gs_b = GridSpec(1, 2, width_ratios=[1, 3.3],
                left=0.035, right=0.975, top=0.678, bottom=0.318, wspace=0.09)

ax_foco_num = fig.add_subplot(gs_b[0, 0])
acento_foco = preparar_panel(ax_foco_num, TXT_DIM)
txt_foco_etiqueta = ax_foco_num.text(0.5, 0.90, '', fontsize=13, fontweight='bold',
                                     color=TXT_DIM, ha='center', va='center')
txt_foco_desc = ax_foco_num.text(0.5, 0.835, '', fontsize=7.5, color=TXT_DIM,
                                 ha='center', va='center')
txt_foco_valor = ax_foco_num.text(0.5, 0.55, '--', fontsize=76, fontweight='bold',
                                  color=TXT_DIM, ha='center', va='center')
txt_foco_unidad = ax_foco_num.text(0.5, 0.345, '', fontsize=18, color=TXT_DIM,
                                   ha='center', va='center')
txt_foco_estado = ax_foco_num.text(0.5, 0.16, 'SIN SEÑAL', fontsize=11, fontweight='bold',
                                   color=TXT_DIM, ha='center', va='center')

ax_foco = fig.add_subplot(gs_b[0, 1])
ax_foco.set_facecolor(PANEL)
for lado in ('top', 'right'):
    ax_foco.spines[lado].set_visible(False)
for lado in ('bottom', 'left'):
    ax_foco.spines[lado].set_color(GRID)
ax_foco.set_xlim(0, MAX_PUNTOS)
ax_foco.set_xticks([0, MAX_PUNTOS * 0.25, MAX_PUNTOS * 0.5, MAX_PUNTOS * 0.75, MAX_PUNTOS])
ax_foco.set_xticklabels([f'-{VENTANA_SEG:g} s'.replace('.', ','),
                         f'-{VENTANA_SEG * 0.75:g} s'.replace('.', ','),
                         f'-{VENTANA_SEG * 0.5:g} s'.replace('.', ','),
                         f'-{VENTANA_SEG * 0.25:g} s'.replace('.', ','), 'ahora'],
                        fontsize=8)
ax_foco.grid(True, color=GRID, alpha=0.55, linestyle='--', lw=0.7, zorder=1)
ax_foco.tick_params(length=0, labelsize=9)
line_foco, = ax_foco.plot([], [], color=TXT, lw=2.2, zorder=3, solid_capstyle='round')
punto_foco, = ax_foco.plot([], [], marker='o', markersize=7, color=TXT, zorder=4)
adornos_foco = []   # Bandas, líneas de umbral y etiquetas: se rehacen al cambiar de canal


def ticks_bonitos(vmin, vmax, objetivo=6):
    """Escalones redondos para el eje Y del canal que tenga el foco.

    Se descarta el tick del borde inferior porque se solaparía con la etiqueta
    del eje de tiempos en la esquina."""
    rango = vmax - vmin
    if rango <= 0:
        return []
    mejor_paso, mejor_error = None, None
    for exponente in range(-4, 6):
        for multiplo in (1, 2, 5):
            paso = multiplo * (10.0 ** exponente)
            error = abs(rango / paso - objetivo)
            if mejor_error is None or error < mejor_error:
                mejor_paso, mejor_error = paso, error
    primero = np.ceil(vmin / mejor_paso) * mejor_paso
    ticks = np.arange(primero, vmax + mejor_paso * 0.5, mejor_paso)
    return [t for t in ticks if t > vmin + rango * 0.001 and t <= vmax]


def aplicar_foco(clave):
    """Reconfigura la gráfica grande para el canal indicado."""
    global canal_foco, adornos_foco
    canal_foco = clave
    cfg = CANALES[clave]

    for adorno in adornos_foco:
        adorno.remove()
    adornos_foco = []

    ax_foco.set_ylim(cfg['vmin'], cfg['vmax'])
    ax_foco.set_yticks(ticks_bonitos(cfg['vmin'], cfg['vmax']))
    ax_foco.set_ylabel(cfg['unidad'] or cfg['etiqueta'], fontsize=10, color=TXT_DIM)

    # Bandas de zona: se ve de un vistazo en qué régimen está el canal
    inferior = cfg['vmin']
    for limite, color in cfg['zonas']:
        superior = min(limite, cfg['vmax'])
        if superior <= inferior:
            continue
        alpha = 0.05 if color in (AZUL, CIAN) else (0.14 if color == ROJO else 0.08)
        adornos_foco.append(ax_foco.axhspan(inferior, superior, color=color, alpha=alpha, zorder=0))
        if color in (AMBAR, ROJO) and inferior > cfg['vmin']:
            adornos_foco.append(ax_foco.axhline(inferior, color=color, lw=1, ls='--',
                                                alpha=0.55, zorder=1))
            adornos_foco.append(ax_foco.text(
                MAX_PUNTOS * 0.005, inferior + (cfg['vmax'] - cfg['vmin']) * 0.012,
                f'{inferior:g} {cfg["unidad"]}'.strip(), fontsize=7.5, color=color,
                ha='left', va='bottom', zorder=5,
                bbox=dict(facecolor=PANEL, edgecolor='none', boxstyle='square,pad=0.25')))
        inferior = superior

    referencia = referencia_de(clave)
    if referencia is not None:
        adornos_foco.append(ax_foco.axhline(referencia, color=TXT_DIM, lw=1, ls=':',
                                            alpha=0.7, zorder=2))

    txt_foco_etiqueta.set_text(cfg['etiqueta'])
    txt_foco_desc.set_text(cfg['descripcion'])
    txt_foco_unidad.set_text(cfg['unidad'])
    for tarjeta in TARJETAS:
        tarjetas[tarjeta]['marco'].set_visible(tarjeta == clave)


# --- BANDA C: TARJETAS DE VIGILANCIA ---
gs_c = GridSpec(1, len(TARJETAS), left=0.035, right=0.975, top=0.282, bottom=0.052,
                wspace=0.030)

tarjetas = {}
for indice, clave in enumerate(TARJETAS):
    cfg = CANALES[clave]
    ax = fig.add_subplot(gs_c[0, indice])
    acento = preparar_panel(ax, TXT_DIM)

    # Marco que señala qué tarjeta está en la gráfica grande
    marco = Rectangle((0.004, 0.01), 0.992, 0.98, transform=ax.transAxes, fill=False,
                      edgecolor=AZUL, lw=1.6, zorder=6)
    marco.set_visible(False)
    ax.add_patch(marco)

    ax.text(0.09, 0.86, cfg['etiqueta'], fontsize=10, fontweight='bold', color=TXT_DIM, va='center')
    ax.text(0.955, 0.86, str(indice + 1), fontsize=8, color=TXT_DIM, ha='right', va='center')
    txt_valor = ax.text(0.09, 0.60, '--', fontsize=26, fontweight='bold', color=TXT_DIM, va='center')
    txt_unidad = ax.text(0.955, 0.55, cfg['unidad'], fontsize=10, color=TXT_DIM,
                         ha='right', va='center')
    txt_extremos = ax.text(0.09, 0.36, '', fontsize=7, color=TXT_DIM, va='center')

    # Sparkline: cada canal lleva su propia historia, no solo el que tiene el foco.
    # Se normaliza contra el rango fijo del canal, nunca autoescalado: si no, una
    # señal plana con ruido de milésimas parecería una montaña rusa.
    X0_SPARK, X1_SPARK, Y0_SPARK, Y1_SPARK = 0.09, 0.955, 0.10, 0.30
    linea_spark, = ax.plot([], [], color=TXT_DIM, lw=1.3, zorder=3, solid_capstyle='round')
    linea_ref = Line2D([X0_SPARK, X1_SPARK], [0, 0], color=TXT_DIM, lw=0.8, ls=':',
                       alpha=0.6, zorder=2)
    ax.add_line(linea_ref)
    linea_ref.set_visible(cfg['referencia'] is not None)

    tarjetas[clave] = dict(ax=ax, acento=acento, marco=marco, valor=txt_valor,
                           unidad=txt_unidad, extremos=txt_extremos, spark=linea_spark,
                           ref=linea_ref,
                           caja=(X0_SPARK, X1_SPARK, Y0_SPARK, Y1_SPARK))

aplicar_foco(CANAL_FOCO_INICIAL)


def puntos_sparkline(clave, caja):
    """Proyecta la historia del canal dentro del recuadro de su tarjeta."""
    x0, x1, y0, y1 = caja
    cfg = CANALES[clave]
    datos = np.array(historial[clave], dtype=float)
    recorrido = cfg['vmax'] - cfg['vmin']
    if recorrido <= 0:
        return [], []
    normalizado = (datos - cfg['vmin']) / recorrido
    normalizado = np.clip(normalizado, 0.0, 1.0)
    xs = np.linspace(x0, x1, len(datos))
    return xs, y0 + normalizado * (y1 - y0)


def y_sparkline(clave, valor, caja):
    """Altura, dentro de la tarjeta, que corresponde a un valor del canal."""
    _, _, y0, y1 = caja
    cfg = CANALES[clave]
    recorrido = cfg['vmax'] - cfg['vmin']
    if recorrido <= 0:
        return y0
    fraccion = min(max((valor - cfg['vmin']) / recorrido, 0.0), 1.0)
    return y0 + fraccion * (y1 - y0)


# --- GRABACIÓN Y CAPTURA ---
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
    escritor_csv.writerow(['n_muestra', 'tiempo_s', 'hora'] + COLUMNAS_CSV)
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
    elif event.key and event.key.isdigit():
        indice = int(event.key) - 1
        if 0 <= indice < len(TARJETAS):
            aplicar_foco(TARJETAS[indice])
            mostrar_aviso(f'Gráfica: {CANALES[TARJETAS[indice]]["descripcion"]}', 2.5)


fig.canvas.mpl_connect('key_press_event', al_pulsar_tecla)


def update(frame):
    global ultimo_tiempo_dato, conectado, inicio_sesion
    global paquetes_ok, paquetes_error, muestras_grabadas

    txt_reloj.set_text(datetime.now().strftime('%H:%M:%S'))
    ahora = time.time()
    hubo_dato = False

    # --- LECTURA DEL SOCKET (vaciamos todo lo pendiente) ---
    try:
        while True:
            data, _ = sock.recvfrom(1024)
            try:
                lectura = parsear_mensaje(data.decode('utf-8'))
            except (ValueError, UnicodeDecodeError):
                paquetes_error += 1
                continue

            # Sello propio de cada paquete: en una misma pasada se vacían varios
            # y compartir el instante del frame duplicaría marcas en el CSV
            t_paquete = time.time()
            ultimo_tiempo_dato = t_paquete
            paquetes_ok += 1
            hubo_dato = True
            sellos_tiempo.append(t_paquete)
            if inicio_sesion is None:
                inicio_sesion = t_paquete

            ultima_lectura.update(lectura)
            for clave, valor in lectura.items():
                if clave in historial:
                    historial[clave].append(valor)
                    maximos[clave] = valor if clave not in maximos else max(maximos[clave], valor)
                    minimos[clave] = valor if clave not in minimos else min(minimos[clave], valor)

            if grabando:
                muestras_grabadas += 1
                escritor_csv.writerow(
                    [muestras_grabadas, f'{t_paquete - inicio_grabacion:.3f}',
                     datetime.now().strftime('%H:%M:%S.%f')[:-3]]
                    + [valor_csv(col) for col in COLUMNAS_CSV])
                fichero_csv.flush()
    except BlockingIOError:
        pass

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
        if not hubo_dato:
            # Cortamos las líneas en vez de dibujar ceros falsos
            for cola in historial.values():
                cola.append(np.nan)
            ultima_lectura.clear()
        txt_estado.set_text('SIN SEÑAL')
        txt_estado.set_color(ROJO)
        led_estado.set_color(ROJO)
        txt_hz.set_text('-- Hz')
        txt_hz.set_color(TXT_DIM)

    # --- BANDA A: RPM, VELOCIDAD, PEDALES ---
    valor_rpm = ultima_lectura.get('rpm') if conectado else None
    if valor_rpm is not None:
        valor_rpm = max(0, min(MAX_RPM, valor_rpm))
        encendidos = int(round((valor_rpm / MAX_RPM) * N_SEGMENTOS))
        en_corte = valor_rpm >= RPM_CORTE
        destello = en_corte and (frame // 4) % 2 == 0
        for i, seg in enumerate(segmentos):
            if destello:
                seg.set_facecolor(ROJO)
            elif i < encendidos:
                seg.set_facecolor(colores_segmento[i])
            else:
                seg.set_facecolor(APAGADO)
        txt_rpm.set_text(formatear('rpm', valor_rpm))
        txt_rpm.set_color(ROJO if en_corte else TXT)
    else:
        for seg in segmentos:
            seg.set_facecolor(APAGADO)
        txt_rpm.set_text('--')
        txt_rpm.set_color(TXT_DIM)

    # Velocidad: número grande
    valor_vel = ultima_lectura.get('velocidad') if conectado else None
    txt_vel.set_text(formatear('velocidad', valor_vel))
    txt_vel.set_color(TXT if valor_vel is not None else TXT_DIM)

    # Traza de pedales: acelerador (0-100 %) y freno (presión -> % de FRENO_PRESION_MAX)
    serie_tps = np.array(historial['tps'], dtype=float)[-PUNTOS_PEDALES:]
    serie_freno = np.array(historial['freno_del'], dtype=float)[-PUNTOS_PEDALES:]
    freno_pct = np.clip(serie_freno / FRENO_PRESION_MAX * 100.0, 0, 100)
    xs_ped = np.arange(len(serie_tps), dtype=float)
    line_tps.set_data(xs_ped, serie_tps)
    line_freno.set_data(xs_ped, freno_pct)
    fill_tps.set_xy(area_pedales(xs_ped, serie_tps))
    fill_freno.set_xy(area_pedales(xs_ped, freno_pct))

    val_tps = ultima_lectura.get('tps') if conectado else None
    val_freno = ultima_lectura.get('freno_del') if conectado else None
    txt_ped_tps.set_text(f'ACEL {int(round(val_tps))}%' if val_tps is not None else 'ACEL --')
    if val_freno is not None:
        txt_ped_freno.set_text(f'FRENO {int(round(min(100.0, val_freno / FRENO_PRESION_MAX * 100)))}%')
    else:
        txt_ped_freno.set_text('FRENO --')

    # --- BANDA C: TARJETAS ---
    alarmas = []
    for clave, widgets in tarjetas.items():
        valor = ultima_lectura.get(clave) if conectado else None
        color = color_de(clave, valor)
        widgets['valor'].set_text(formatear(clave, valor))
        widgets['valor'].set_color(color)
        widgets['unidad'].set_color(TXT_DIM)
        widgets['acento'].set_facecolor(color)
        widgets['spark'].set_color(color if valor is not None else TXT_DIM)

        xs, ys = puntos_sparkline(clave, widgets['caja'])
        widgets['spark'].set_data(xs, ys)

        referencia = referencia_de(clave)
        if referencia is not None:
            y_ref = y_sparkline(clave, referencia, widgets['caja'])
            widgets['ref'].set_ydata([y_ref, y_ref])
            widgets['ref'].set_visible(True)
        else:
            widgets['ref'].set_visible(False)

        if clave in maximos:
            widgets['extremos'].set_text(
                f'máx {formatear(clave, maximos[clave])}   mín {formatear(clave, minimos[clave])}')
        else:
            widgets['extremos'].set_text('')

        if color == ROJO and valor is not None:
            alarmas.append(CANALES[clave]['etiqueta'])

    # --- BANDA B: CANAL CON FOCO ---
    valor_foco = ultima_lectura.get(canal_foco) if conectado else None
    color_foco = color_de(canal_foco, valor_foco)
    txt_foco_valor.set_text(formatear(canal_foco, valor_foco))
    txt_foco_valor.set_color(color_foco)
    acento_foco.set_facecolor(color_foco)
    line_foco.set_color(color_foco if valor_foco is not None else TXT_DIM)
    line_foco.set_data(range(MAX_PUNTOS), historial[canal_foco])

    if valor_foco is None:
        txt_foco_estado.set_text('SIN SEÑAL' if not conectado else 'SIN DATO')
        txt_foco_estado.set_color(TXT_DIM)
        punto_foco.set_data([], [])
    else:
        if color_foco == ROJO:
            estado = 'CRÍTICO'
        elif color_foco == AMBAR:
            estado = 'PRECAUCIÓN'
        elif color_foco == CIAN:
            estado = 'EN CALENTAMIENTO'
        else:
            estado = 'NORMAL'
        txt_foco_estado.set_text(estado)
        txt_foco_estado.set_color(color_foco)
        punto_foco.set_color(color_foco)
        punto_foco.set_data([MAX_PUNTOS - 1], [valor_foco])

    # --- ALARMA ---
    if alarmas:
        txt_alarma.set_text('  ALARMA:  ' + '   ·   '.join(alarmas) + '  ')
        txt_alarma.set_visible(True)
        encendida = (frame // 5) % 2 == 0
        txt_alarma.get_bbox_patch().set_facecolor(ROJO if encendida else '#6E1414')
    else:
        txt_alarma.set_visible(False)

    # --- INDICADOR DE GRABACIÓN Y PIE ---
    if grabando:
        parpadeo = (frame // 6) % 2 == 0
        txt_rec.set_text(f'REC {formato_tiempo(ahora - inicio_grabacion)}')
        txt_rec.set_color(ROJO)
        led_rec.set_color(ROJO if parpadeo else '#4A1414')
    else:
        txt_rec.set_text('SIN GRABAR')
        txt_rec.set_color(TXT_DIM)
        led_rec.set_color(APAGADO)

    sesion = formato_tiempo(None if inicio_sesion is None else ahora - inicio_sesion)
    errores = f'   ·   {paquetes_error} con error' if paquetes_error else ''
    txt_sesion.set_text(f'SESIÓN {sesion}   ·   {paquetes_ok:,}'.replace(',', '.')
                        + f' paquetes{errores}')
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

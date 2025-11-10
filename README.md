# 🌐 ESP32 Web Control — 4 LEDs y 4 Botones (All-in-One)

Proyecto didáctico desarrollado con **ESP-IDF**, que permite **controlar 4 LEDs** y **leer 4 botones físicos** mediante una **interfaz web responsiva** servida directamente por el **ESP32** en modo **Wi-Fi SoftAP**.

## 🧩 Descripción general

Implementa un **servidor HTTP embebido** que ofrece una página web con botones para **encender/apagar LEDs remotamente**, y muestra el **estado actual de los botones físicos** conectados al microcontrolador.

Los botones están configurados con **resistencias pull-up internas**, por lo que al presionarlos su nivel lógico pasa a **LOW (0)**.

## ⚙️ Características principales

- ✅ Control remoto de **4 LEDs (GPIO 17, 5, 18, 19)**
- ✅ Lectura de **4 botones (GPIO 13, 12, 14, 27)** con *pull-up* interno
- ✅ **Servidor Web HTTP** integrado (sin archivos externos)
- ✅ **Wi-Fi SoftAP**: el ESP32 crea su propia red local
- ✅ Comunicación **HTML + CSS + JavaScript (fetch API)**
- ✅ Interfaz moderna y responsive (HTML embebido)
- ✅ Intercambio de datos vía **JSON**

## 🧠 Esquema de pines

| Elemento | GPIO | Dirección | Descripción |
|-----------|------|------------|--------------|
| LED1 | 17 | OUTPUT | LED controlado por la web |
| LED2 | 5 | OUTPUT | LED controlado por la web |
| LED3 | 18 | OUTPUT | LED controlado por la web |
| LED4 | 19 | OUTPUT | LED controlado por la web |
| BTN1 | 13 | INPUT_PULLUP | Botón físico 1 |
| BTN2 | 12 | INPUT_PULLUP | Botón físico 2 |
| BTN3 | 14 | INPUT_PULLUP | Botón físico 3 |
| BTN4 | 27 | INPUT_PULLUP | Botón físico 4 |

> Los botones deben conectarse entre el GPIO y **GND**.

## 📡 Configuración de red Wi-Fi

| Parámetro | Valor |
|------------|--------|
| SSID | `ESP32_LAB` |
| Contraseña | `12345678` |
| Canal | 1 |
| Dirección IP | `192.168.4.1` |

Conéctate desde tu computadora o smartphone y abre en el navegador:

```
http://192.168.4.1/
```

## 🖥️ Interfaz web

La página principal muestra:

- Cuatro secciones de control **LED1–LED4**
  - Botones: `ON`, `OFF`, `ALL ON`, `ALL OFF`
  - Estado visual de cada LED (pastilla verde o roja)
- Cuatro indicadores de **botones físicos** (`BTN1–BTN4`)
  - Muestran `HIGH` (reposo) o `LOW (pressed)`

## 🔄 Comunicación HTML ↔ C (ESP32)

| Ruta | Descripción | Respuesta |
|------|--------------|-----------|
| `/` | Página principal | HTML |
| `/state` | Estado actual de LEDs y botones | JSON |
| `/set?led=N&state=0|1` | Cambia el estado del LED N | JSON |
| `/all?state=0|1` | Cambia todos los LEDs | JSON |

Ejemplo de respuesta:
```json
{"leds":[1,0,1,0],"btns":[1,1,0,1]}
```

## 🧰 Requisitos

- **ESP-IDF** (v4.4 o superior)
- **ESP32 WROOM32** o similar
- **Cable USB**
- **Navegador web moderno**

## 🛠️ Compilación y ejecución

```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Luego:
1. Conéctate a la red Wi-Fi `ESP32_LAB`
2. Abre `http://192.168.4.1/` en tu navegador
3. Controla los LEDs y observa los botones 🟢🔴

## 📚 Estructura del proyecto

```
esp32-web-io/
├── main/
│   ├── main.c
│   └── CMakeLists.txt
├── CMakeLists.txt
└── README.md
```

## 🧩 Posibles mejoras

- 🔘 Agregar botón “Toggle” por LED
- 🕹️ Usar interrupciones en botones físicos
- 🧠 Añadir autenticación simple (`/set?key=1234`)
- 💡 Cambiar a modo STA para red existente
- 🌈 Servir HTML/CSS/JS desde SPIFFS

## 👨‍🏫 Créditos

**Autor:** Dr. Roger Chiu Zárate  
**Centro Universitario de los Lagos – Universidad de Guadalajara**

## 📜 Licencia

Licencia **MIT** — uso libre educativo y académico.

---
✳️ *Desarrollado con pasión por la docencia y la ingeniería embebida.*


# Empezar desde cero (sin haber usado ESP-IDF)

Vienes de Arduino: aquí es parecido pero se llama ESP-IDF (el entorno "de verdad" de Espressif). Lo más cómodo es
**Visual Studio Code con la extensión de Espressif**, que instala todo sola. Cuenta con 30-60 min la primera vez
(descarga varios GB) y unos 10 GB de disco. Sirve igual en Windows, macOS y Linux.

> Esto grabará SLOOP en el Tab5 **en lugar del firmware de fábrica**. Se restaura con M5Burner (guía oficial:
> docs.m5stack.com, "Tab5 Restore Factory Firmware"). No pierdes nada permanente.

## 1. Requisitos previos
- **Git** y **Python 3** instalados (en Windows, al instalar Python marca "Add python.exe to PATH").
- Linux (Debian/Ubuntu): `sudo apt install git wget flex bison gperf python3 python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0`
- macOS: Xcode command line tools (`xcode-select --install`) y `brew install cmake ninja dfu-util ccache`.
- Un cable **USB-C de datos** (no uno solo de carga).
- Instala todo en rutas **sin espacios** y cortas (Windows: `C:\esp\...`).

## 2. Visual Studio Code + extensión
1. Instala VS Code (code.visualstudio.com).
2. Pestaña de extensiones, busca **ESP-IDF** (autor: Espressif Systems) e instálala.
3. Pulsa **F1**, escribe **ESP-IDF: Configure ESP-IDF Extension**, elige **EXPRESS** y una versión **v5.5.x**
   (vale >= 5.4). Espera a que termine (descarga ESP-IDF, el compilador y un entorno Python).

## 3. El código
En una terminal (en una carpeta sin espacios):

    git clone https://github.com/isod89/sloop-fm1.git
    cd sloop-fm1
    git checkout d315ad88c5fbaad232ce9426b06db66395bd87db

Descomprime `sloop-tab5-platform.tar.gz` **dentro de `sloop-fm1`** (debe quedar la carpeta `sloop-fm1/tab5/`).
Linux/macOS: `tar xzf sloop-tab5-platform.tar.gz`. Windows 10+: `tar -xzf sloop-tab5-platform.tar.gz` en la
terminal, o 7-Zip.

## 4. Compilar
1. En VS Code: **File > Open Folder** y abre **`sloop-fm1/tab5/idf`** (esa carpeta, no la raíz).
2. **F1 > ESP-IDF: Set Espressif Device Target** > **esp32p4**. Si pregunta por la configuración, deja la de por
   defecto del proyecto (`sdkconfig.defaults`).
3. **F1 > ESP-IDF: Build Project**. La primera vez descarga el soporte del Tab5 desde el registro de componentes
   de Espressif (necesita internet) y tarda unos minutos.

**Es normal que falle la primera vez**, sobre todo en `board_bsp.c`: lo escribí sin poder compilarlo. Copia el
primer error (el de más arriba) y me lo pasas, suele ser un nombre de función.

## 5. Grabar en el Tab5
1. Pon el Tab5 en **modo de grabación**: con el USB-C conectado, mantén pulsado el botón **reset** unos 2 segundos
   hasta que el LED verde interno parpadee rápido; suéltalo. En el PC aparece como "USB JTAG/serial debug unit".
2. **F1 > ESP-IDF: Select Port to Use** y elige ese puerto (COMx en Windows, ttyACMx en Linux, usbmodem en macOS).
3. **F1 > ESP-IDF: Flash (UART) your Project** (si pregunta el método, elige UART).
4. Pulsa **reset** una vez para arrancar. Con **F1 > ESP-IDF: Monitor Device** ves lo que escribe el Tab5.
   (Atajo: el icono de llama de la barra inferior hace compilar + grabar + monitor.)

## 6. Qué esperar y qué contarme
- Al arrancar, un **pitido** de 440 Hz (unos 0,4 s): prueba del altavoz.
- El logo de SLOOP y luego su pantalla en el cuadrado de arriba a la izquierda (480x480).
- Abajo, dos filas de **teclas grises** (27); se ponen verdes al tocarlas.
- A la derecha, los **12 botones** (FX, SCL, ENV, LFO, EDIT, GLO / HOME, SAVE, ARP, SEQ, PLAY, REC), los **knobs**
  (KNOB 1 a 4, MASTER, SELECT, PRESETS, ALGORITHM) y OCT- / OCT+. Los LEDs del aparato se ven: un botón se ilumina en
  ámbar cuando el firmware lo enciende.
- **Knobs:** arrastra el dedo hacia arriba o abajo sobre ellos. MASTER es el volumen.
- **Botones:** mientras los tocas están pulsados. Se pueden usar **varios dedos a la vez** (mantén un botón con uno y toca
  teclas con otros, que es como se maneja el FM-1). Si mantienes un botón **solo, 0,6 s**, y levantas el dedo, se queda
  **enganchado** (borde amarillo) para dejarte las dos manos libres; tócalo otra vez para soltarlo.
- **Importante:** la pista 1 es un bajo 808 y sus notas caen entre 64 y 194 Hz, que el altavoz del Tab5 casi no
  reproduce. **Pulsa OCT+ dos veces** antes de tocar, o cambia a otra pista con ALGORITHM.
### Volumen y salida de audio (altavoz / jack de 3,5 mm)
- **Nivel de salida:** en la página de **MIDI** (botón MIDI) tienes **LEVEL -** y **LEVEL +** (de 0 a 8, 3 dB por paso; empieza
  en 4, que es +12 dB sobre el nivel del firmware). Se guarda. Lleva un limitador suave: sube sin que distorsione de golpe.
  Además del nivel, **MASTER** sigue funcionando (por defecto está al 70 %).
- **Jack de 3,5 mm:** al enchufar unos auriculares el Tab5 **corta el altavoz interno** y baja el nivel 12 dB para tus
  oídos (el codec pasa también a un volumen algo menor); al sacarlos, vuelve al altavoz. La página de MIDI muestra
  `SPEAKER` o `HEADPHONES`. El monitor escribe `3.5 mm jack: plug inserted / removed` y `audio output: ...`.
- **Si la detección va al revés** (al enchufar sigue sonando el altavoz, o con el jack vacío se corta): abre
  `tab5/idf/boards/bsp/board/CMakeLists.txt`, añade la línea `target_compile_definitions(${COMPONENT_LIB} PRIVATE BOARD_HP_ACTIVE_LOW)`
  y recompila. La línea `3.5 mm jack at boot:` del arranque dice qué ha leído, y las líneas `jack: expander input 0x..` muestran el valor crudo del expansor (bit 7).
- **Códec (volumen):** el ES8388 se configura como lo hace M5Unified (salidas a +4,5 dB, mezclador, DAC a 0 dB). El monitor escribe `ES8388 regs: ... (expect 3C B8 21)`; si ves otros valores, dímelo.

### Si no suena (diagnóstico)
1. Abre el monitor y da un toque corto a reset. La **primera línea** debe decir `SLOOP for the Tab5, build <fecha hora>`:
   si la fecha no es la de tu última compilación, no se grabó la versión nueva.
2. Pulsa **MIDI** y luego **TONE** (arriba a la derecha de la página de aprendizaje): suena un tono de medio segundo, con
   el nivel de una nota del firmware y pasando por la misma ganancia. Si **se oye**, el altavoz y el códec funcionan: ajusta
   **LEVEL +**. Si **no se oye**, es el altavoz o su alimentación.
3. Cada 5 s el monitor escribe `firmware peak N% of full scale, gain +X dB`: el nivel más alto que genera el firmware y la
   ganancia aplicada. Si tocas teclas y sale `0%`, el sintetizador no genera sonido; si sale 5 % o más y no lo oyes, es salida.
4. Copia las **primeras 40 líneas** del arranque (buscando `E (`, `board`, `speaker`, `codec`) y las líneas `audio:`.

### Teclado o controlador MIDI por USB
- Conéctalo al puerto **USB-A** del Tab5 (debe ser *class-compliant*, es decir, que funcione en un PC sin driver). Según
  la documentación del BSP, la alimentación de ese puerto sale del USB-C: si el teclado no se enciende, deja el USB-C
  conectado a corriente.
- En el monitor verás `MIDI device at address ...` al enchufarlo y `removed` al sacarlo. Se puede enchufar y desenchufar
  en caliente, y hasta 3 dispositivos a la vez.
- Lo que no esté asignado va al sintetizador del firmware, igual que en el aparato (según el código, los canales 1 a 3
  tocan las pistas 1 a 3, el 10 la batería y el resto la pista seleccionada). Las notas MIDI suenan a su altura real, así
  que la pista 1 también se oye bien así, sin tocar octavas.

### MIDI learn: controlar el panel desde un controlador
1. Pulsa el botón **MIDI** (abajo a la derecha). El cuadrado de la izquierda pasa a ser la página de aprendizaje, que
   también te dice si hay un dispositivo conectado y muestra el último mensaje MIDI recibido.
2. **Toca un control** del panel (un botón, un knob o una tecla): se marca con un marco azul.
3. **Mueve un knob o pulsa un pad** de tu controlador: queda asignado. Los controles que ya tienen asignación llevan un
   punto azul mientras estás en este modo.
4. Botones de la página: **MODE** cambia entre momentáneo / enganche (botones y teclas) o absoluto / relativo (knobs),
   **CLEAR** borra la asignación del control elegido, **CLEAR ALL** borra todas, **EXIT** (o el botón MIDI otra vez) sale.
- Los **botones** y las **teclas** aceptan notas y CC. Los **knobs** y MASTER, CC: *absoluto* para potenciómetros (un paso
  del firmware por cada 2 unidades de CC; el primer valor solo fija el punto de partida, no salta) o *relativo* para
  encoders sin tope (1 a 63 sube, 65 a 127 baja). En modo **enganche**, cada pulsación del pad bloquea o suelta el botón.
- Mientras aprendes no suena nada. Lo asignado **no** toca además una nota. El mapa se guarda solo (en la NVS del Tab5) y
  vuelve al arrancar.

- En el monitor, cada 5 s, una línea `audio: compute avg ... worst ...`: es la carga de CPU.
- **Imagen boca abajo**: abre `tab5/idf/boards/bsp/board/CMakeLists.txt`, quita el `#` de la última línea y recompila.
- Si algo falla, copia las primeras líneas del arranque (las que digan `board`, `keys` o `touch`).

## Con terminal en vez de VS Code
Tras instalar ESP-IDF, abre su terminal ("ESP-IDF 5.x CMD/PowerShell" o `. ./export.sh`) y:

    cd sloop-fm1/tab5/idf
    idf.py set-target esp32p4
    idf.py build
    idf.py -p PUERTO flash monitor

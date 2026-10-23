# SLOOP on the M5Stack Tab5

Port of [SLOOP](https://github.com/isod89/sloop-fm1) (a Felucca fork for the M-Vave FM-1) to the M5Stack Tab5 (ESP32-P4), with a full touch panel, USB MIDI host and MIDI learn.

GPL-3.0-only, like SLOOP.

## Build

    git clone https://github.com/isod89/sloop-fm1.git
    cd sloop-fm1
    git checkout d315ad88c5fbaad232ce9426b06db66395bd87db

Then copy the `tab5` folder of this repository into `sloop-fm1`, open `tab5/idf` in VS Code with ESP-IDF v6.1 and run **Build, Flash and Monitor**. See `tab5/EMPEZAR.md` (Spanish) for the step by step.

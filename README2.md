# Automated Greenhouse Prototype

[![Platform](https://img.shields.io/badge/platform-Arduino%20UNO%20R3-00979D?style=flat-square&logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Language](https://img.shields.io/badge/language-C%2B%2B%20(Arduino)-blue?style=flat-square)](https://www.arduino.cc/reference/en/)
[![License](https://img.shields.io/badge/license-MIT-green?style=flat-square)](./LICENSE)

Firmware and hardware design for an embedded environmental-monitoring and irrigation-control prototype, built on an Arduino UNO R3. The system samples ambient temperature, air humidity, and soil moisture, renders readings on a local LCD, and streams them as serial telemetry over a Bluetooth (SPP) link for consumption by a mobile client.

Developed by the AMIP group as an academic project applying embedded systems and automation to agricultural greenhouse management on Cutijuba Island.

---

## Table of Contents

- [System Overview](#system-overview)
- [Hardware Architecture](#hardware-architecture)
- [Pin Mapping](#pin-mapping)
- [Bill of Materials](#bill-of-materials)
- [Firmware](#firmware)
  - [Dependencies](#dependencies)
  - [Program Structure](#program-structure)
  - [Control Flow](#control-flow)
  - [Serial / Bluetooth Protocol](#serial--bluetooth-protocol)
  - [Build and Upload](#build-and-upload)
- [Known Limitations](#known-limitations)
- [Mobile Client](#mobile-client)
- [Project Photo](#project-photo)
- [Project Videos](#project-videos)
- [Repository Layout](#repository-layout)
- [License](#license)
- [Team](#team)

---

## System Overview

The prototype implements a closed monitoring loop for a small-scale greenhouse enclosure:

1. **Acquisition** — a DHT11 sensor provides ambient temperature and relative air humidity; a resistive soil-moisture probe provides a normalized soil hydration reading via an analog input.
2. **Local feedback** — readings are rendered on a 16x2 character LCD (I2C backpack) on a rotating basis.
3. **Remote telemetry** — the same readings are serialized as a formatted string and transmitted over UART to an HC-05 Bluetooth module, which exposes them to any paired Bluetooth serial-terminal mobile app.
4. **Actuation** — a 12V DC water pump and a 5V ventilation fan provide the physical irrigation/cooling actuators for the enclosure (currently wired for manual/switch-based control in this prototype revision).

The firmware runs a single-threaded, blocking loop (`loop()` in `main.ino`) with fixed `delay()`-based timing rather than an event-driven or interrupt-based scheduler — appropriate for the prototype's sampling cadence but a known constraint (see [Known Limitations](#known-limitations)).

---

## Hardware Architecture

```
                +-------------------+
                |   Arduino UNO R3  |
                |                   |
  DHT11  ------>| D2                |
  Soil Sensor -->| A3                |
                |                   |
                | I2C (SDA/SCL) --->| LCD 16x2 (I2C, addr 0x27)
                | UART (TX/RX)  --->| HC-05 Bluetooth module ---(SPP)---> Mobile device
                |                   |
  Fan (5V) <-----| digital/switch    |
  Pump (12V) <---| relay/switch      |
                +-------------------+
```

- **Compute:** Arduino UNO R3 (ATmega328P), 16 MHz, 5V logic.
- **Sensing:** DHT11 (digital, single-wire) for temperature/humidity; analog resistive probe for soil moisture.
- **Display:** 16x2 character LCD driven through a PCF8574-based I2C serial backpack (I2C address `0x27`).
- **Connectivity:** HC-05 Bluetooth-to-serial module bridging the Arduino's hardware UART to a Bluetooth SPP profile.
- **Actuation:** 12V DC water pump (DC30A-1230) and 5V 30x30mm fan, switched via on/off slide switches in this revision.
- **Power:** separate 5V supply for the Arduino/logic and 12V supply for the pump, isolating motor noise/inrush from the logic rail.

---

## Pin Mapping

| Signal | Arduino Pin | Notes |
|---|---|---|
| DHT11 data | D2 | Single-wire digital, defined as `DHT11_PIN` |
| Soil-moisture sensor | A3 | Analog input, defined as `PIN_SensorUmidadeSolo`; 0–1023 raw, mapped to 0–100% |
| LCD (I2C) | SDA / SCL (A4 / A5 on UNO) | I2C address `0x27`, 16x2 |
| HC-05 Bluetooth | RX/TX (hardware UART) | Shares the Arduino's `Serial` at 9600 baud — also used for USB programming/debug |
| Fan | switch-controlled | 5V rail, on/off slide switch |
| Water pump | switch-controlled | 12V rail, on/off slide switch |

> Note: because Bluetooth telemetry and USB serial debugging share the same hardware UART (`Serial`, 9600 baud), the HC-05 should be disconnected or the module's enable line held low during USB reprogramming to avoid bus contention.

---

## Bill of Materials

| Qty | Component |
|---|---|
| 1 | Arduino UNO R3 |
| 1 | DHT11 temperature/humidity sensor |
| 1 | Resistive soil-moisture sensor |
| 1 | 16x2 character LCD |
| 1 | I2C serial backpack for LCD (PCF8574, addr 0x27) |
| 1 | HC-05 Bluetooth module |
| 1 | 5V 30x30 mm cooling fan |
| 1 | 12V DC30A-1230 water pump |
| 2 | On/off slide switches |
| 1 | 0.5 m irrigation hose |
| 1 | 5V power supply (logic/Arduino) |
| 1 | 12V power supply (water pump) |

---

## Firmware

### Dependencies

Install via the Arduino Library Manager (Sketch → Include Library → Manage Libraries):

| Library | Purpose |
|---|---|
| [`LiquidCrystal_I2C`](https://www.arduinolibraries.info/libraries/liquid-crystal-i2c) | Drives the 16x2 LCD over I2C |
| [`DFRobot_DHT11`](https://github.com/DFRobot/DFRobot_DHT11) | Reads temperature/humidity from the DHT11 |

### Program Structure

`main.ino` is organized around a fixed set of module-level state variables and small, single-purpose functions:

| Function | Responsibility |
|---|---|
| `setup()` | Initializes the LCD, configures the soil-sensor pin as input, opens `Serial` at 9600 baud |
| `loop()` | Orchestrates one full acquisition/display/transmit cycle |
| `temperatura()` | Reads DHT11 temperature, writes it directly to the LCD |
| `umidadeDoAr()` | Reads DHT11 humidity into shared message buffers |
| `umidadeDaTerra()` | Reads and normalizes the soil-moisture analog value (0–100%) into shared message buffers |
| `display()` | Renders the current `mensagem1`/`mensagem2` buffers to the LCD |
| `bluetoohMensagem()` | Formats all three readings into a single string and writes it to `Serial` (→ HC-05) |

State is passed between functions through shared globals (`mensagem1`, `mensagem2`, `x1`, `x2`, `x3`) rather than return values or parameters — acceptable at this scale, but a candidate for refactoring if the firmware grows.

### Control Flow

Each iteration of `loop()` executes sequentially and blocks for the sum of all internal `delay()` calls (~11 seconds/cycle):

```
loop()
 ├─ temperatura()      → LCD: "Temperatura: NN°C"      (3s)
 ├─ umidadeDoAr()       → buffers updated, not yet shown
 ├─ display()           → LCD: "Umidade do Ar: NN%"     (3s)
 ├─ umidadeDaTerra()    → buffers updated, not yet shown
 ├─ display()           → LCD: "Umidade do Solo: NN%"   (3s)
 └─ bluetoohMensagem()  → Serial/Bluetooth telemetry     (2s)
```

### Serial / Bluetooth Protocol

Once per loop, a single line is written to `Serial` (and thus relayed by the HC-05) at 9600 baud:

```
[ Umid do Ar: <humidity>% ]   [ Temp: <temperature>°C ]   [ Umid da Terra: <soil>% ]
```

This is a human-readable, unstructured format intended for direct display in a generic Bluetooth serial-terminal app rather than machine parsing. Consumers that need structured data should parse this line with a regex/split rather than relying on a stable schema, or the firmware should be extended to emit a delimited/JSON payload.

### Build and Upload

1. Open `main.ino` in the Arduino IDE (or install the Arduino CLI).
2. Install the dependencies listed above via Library Manager.
3. Select **Board:** Arduino UNO, and the correct **Port** for your USB connection.
4. Disconnect or disable the HC-05 module before uploading (shared UART, see [Pin Mapping](#pin-mapping)).
5. Upload the sketch.
6. Pair the HC-05 with a mobile device (default pairing code is typically `1234` or `0000`) and open any Bluetooth serial-terminal app to view telemetry (see [Mobile Client](#mobile-client)).

Arduino CLI equivalent:

```bash
arduino-cli compile --fqbn arduino:avr:uno .
arduino-cli upload -p <PORT> --fqbn arduino:avr:uno .
```

---

## Known Limitations

- **Blocking timing model:** all pacing is done with `delay()`, so the loop cannot service other events (e.g., a button press or a fault condition) while waiting — a `millis()`-based non-blocking scheduler would allow concurrent tasks.
- **No structured telemetry format:** the Bluetooth payload is a formatted display string, not a stable, parseable schema (see [Serial / Bluetooth Protocol](#serial--bluetooth-protocol)).
- **No closed-loop actuation:** the pump and fan are switch-driven in this revision; sensor readings are not yet used to automatically trigger irrigation or ventilation.
- **DHT11 read frequency:** the DHT11 has a minimum ~1s sampling interval; it is currently read multiple times per cycle (`temperatura()`, `umidadeDoAr()`, `bluetoohMensagem()`), which is within spec given the cycle's ~11s duration but is redundant and could be consolidated into a single read per cycle.
- **Shared UART contention:** USB programming and Bluetooth telemetry cannot be used simultaneously without disconnecting the HC-05, since both use the same hardware serial port.

---

## Mobile Client

Telemetry was displayed on a phone using a Bluetooth serial-terminal app: a black screen with a plant image in the background, overlaid with the incoming air temperature, air humidity, and soil moisture readings described in [Serial / Bluetooth Protocol](#serial--bluetooth-protocol). The link was receive-only from the app's perspective — it connected to the HC-05 and rendered the incoming telemetry, with no commands sent back to the Arduino. The specific app used has since been lost and could not be identified for this document; the screenshot below is the only remaining record.

<img src="./assets/mobile.jpg" width="40%" alt="Mobile app displaying greenhouse sensor data"/>

Any generic Bluetooth SPP terminal app that can display incoming serial text is compatible with this firmware's telemetry format.

---

## Project Photo

<img src="https://github.com/user-attachments/assets/79a7904a-b725-4893-8924-1a20300b37dc" width="70%" alt="Automated greenhouse prototype"/>

---

## Project Video
https://github.com/lianeheidemann/ProjetoDeIrrigacaoMonitoramentoAutomatico_AMIP/assets/54177181/5dc2416f-5027-4611-a1f7-5dbb007b2bb0

<details>
  <summary>YouTube links</summary>

**Portuguese:** [Watch on YouTube](https://youtu.be/CeOX5DaF4m8)

**English:** [Watch on YouTube](https://youtu.be/XomKhprOvek)

</details>

---

## Repository Layout

```
.
├── main.ino      # Firmware entry point (Arduino sketch)
├── LICENSE       # MIT License
├── .gitignore    # Build artifact / editor exclusions
├── README.md     # Project overview (non-technical)
└── README2.md    # This document
```

---

## License

Distributed under the MIT License. See [LICENSE](./LICENSE) for the full text.

---

## Team

| Name | Email |
|------|-------|
| Liane Ferreira Heidemann | liane22070222@aluno.cesupa.br |
| Luis Imhotep | luis22070056@aluno.cesupa.br |
| Fabio Gabriel Areas | fabio21070209@aluno.cesupa.br |
| Fellipe Santos | fellipe20070001@aluno.cesupa.br |
| Luan Augusto | luan22070212@aluno.cesupa.br |

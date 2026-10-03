# iDrive Controller Emulator

Arduino sketch and browser GUI that emulate a BMW iDrive controller on CAN. Buttons, knob directions, push and rotary scrolling are sent as `0x25B` frames, so a head unit sees a real controller.


Rotary Knob being used:
![Rotary knob](demo_gif/bmw_idriveweb_rotayknob.gif)


Shortcut buttons being used:
![Shortcuts](demo_gif/bmw_idriveweb_shortcuts.gif)

## Hardware

- Arduino with an MCP2515 CAN shield (CS on pin 10, 16 MHz crystal)
- CAN bus at 500 kbps
- Library: [mcp_can](https://github.com/coryjfowler/MCP_CAN_lib)

## Usage

1. Open `iDrive_Controller_Emulator.ino`, install `mcp_can`, and upload.
2. Open your preferred Serial monitor at 115200 bauds.

## GUI

`idrive_gui.html` talks to the board over Web Serial.

1. Close the Arduino Serial Monitor (IMPORTANT!).
2. Open the file in **Chrome, Edge or Opera**. Note: If the port is blocked on `file://`, run `python3 -m http.server 8000` and open `http://localhost:8000/idrive_gui.html`.
3. Click **Connect Arduino** and pick the desired port.

Keyboard: arrow keys, Enter = push, PgUp/PgDn = rotate. The mouse wheel over the knob panel also rotates.

## Protocol notes

Frames on `0x25B`: `[counter, FF, 7F, b3, b4, b5, b6, b7]`, idle `00 FF 7F 00 00 00 C0 C0`. The counter increments on every frame.


## Credits

- Button/knob frames: [LeanWasTaken/iDrive](https://github.com/LeanWasTaken/iDrive)
- Rotary delta logic: [llilakoblock/bmw-idrive-touch-esp32-s3](https://github.com/llilakoblock/bmw-idrive-touch-esp32-s3)

Both logics and can bus traces have been merged to one proper code for emulating a idrive controller.

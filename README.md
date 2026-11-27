# Pocket Looper

Pocket Looper is firmware for the Waveshare ESP32-S3-Touch-AMOLED-1.8 board. The board is a Bluetooth Low Energy MIDI controller for the Android looper app Loopify. It shows four large pads for recording, undoing, playing and starting a new idea, and it sends MIDI notes that Loopify has been told to listen for.

The firmware is written in C for ESP-IDF 5.5 or newer. It uses NimBLE for Bluetooth and LVGL 9 for the screen. Waveshare's board support component, `waveshare/esp32_s3_touch_amoled_1_8`, starts the display and the touch controller, so the project contains no display or touch drivers.

## Build and flash

Install ESP-IDF 5.5 or newer for the ESP32-S3 target, then in a shell:

```sh
. ~/esp/esp-idf/export.sh
cd ~/pocket-looper
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port of the board, for example `/dev/cu.usbmodem1101` on macOS. The first build downloads the board support component, LVGL and their dependencies into `managed_components/`. Press `Ctrl+]` to leave the serial monitor.

## Connecting

The board advertises as `Pocket Looper` and offers the standard BLE MIDI service, so Android or any other BLE MIDI host can connect to it. No pairing is required. If the phone asks, the board accepts "Just Works" bonding and remembers the bond across restarts. Every message uses MIDI channel 1.

The board can send only while a phone is connected and has enabled notifications on the MIDI characteristic. Until then the status line shows `No phone`, every gesture is treated as denied, and the tracked state does not change. When the phone reconnects, the board keeps the state it had.

## The MIDI notes

Each message is a Note On with velocity 127, followed by a Note Off for the same note 60 milliseconds later.

| Note | Loopify action |
| --- | --- |
| 60 | Start Loop Recording |
| 61 | Undo |
| 62 | Redo |
| 63 | Toggle Clock |
| 64 to 71 | Clear loop 1 to loop 8 |

When one gesture needs several notes, each Note On is sent at least 30 milliseconds after the previous one.

## Loopify bindings

Open Loopify's MIDI settings and bind each action to the note in the table above, on channel 1. The setup screen described below exists to make this easy.

- Bind "Start Loop Recording" to note 60 and keep its option "After recording" set to "Play". The board assumes that the loop starts playing by itself when recording stops.
- Bind "Undo" to note 61, "Redo" to note 62 and "Toggle Clock" to note 63.
- Bind "Clear loop 1" to "Clear loop 8" to notes 64 to 71, one binding per loop.

## Using the controller

The top line shows the current state on the left, and the number of recorded layers on the right once there is at least one. When the board cannot send, the left side shows `No phone` instead of the state.

A tap is a press released in under 600 milliseconds. It is reported when the finger lifts. A long press is reported once, 600 milliseconds after the finger touched the pad and while it is still down. Lifting the finger after a long press sends nothing, and a finger that slides off a pad before lifting sends nothing.

Every gesture gives feedback on the pad. An accepted tap flashes it green, an accepted long press flashes it orange, and a gesture that does nothing in the current state flashes it dark red.

The board tracks the state of the app from what it has sent, because Loopify sends nothing back. The states are `Ready`, `Recording`, `Playing`, `Overdubbing`, `Paused` and `Saved`. The board also counts layers, from 0 to 8, and counts how many layers can be redone.

### Record pad (top left)

The pad shows a red circle. While recording it turns red with a white rounded square, and while overdubbing it turns orange with the same square.

- A tap in `Ready` starts recording and sends note 60.
- A tap in `Recording` stops the take, the loop starts playing, the state becomes `Playing` and there is one layer. It sends note 60.
- A tap in `Playing` starts an overdub and sends note 60. It is denied when there are already 8 layers.
- A tap in `Overdubbing` ends the overdub, adds a layer and returns to `Playing`. It sends note 60.
- Starting any recording discards the layers that could have been redone.
- A tap in `Paused` or `Saved` is denied, and a long press is always denied.

### Undo pad (top right)

The pad is dimmed when a tap would be denied.

- A tap in `Playing`, `Paused` or `Saved` with at least one layer removes the newest layer and sends note 61. When the last layer goes, the state becomes `Ready`, and if it was `Playing` the board also sends note 63 so that Loopify's clock stops.
- A tap in `Ready`, `Recording` or `Overdubbing` is denied.
- A long press restores the layer that was last undone, sends note 62 and works in `Ready`, `Playing`, `Paused` and `Saved` when there is something to redo. From `Ready` the state becomes `Paused`, because the clock is stopped.

### Play pad (bottom left)

The pad shows a pause symbol in `Playing`, a check mark in `Saved` and a play symbol otherwise. It is dimmed when a tap would be denied.

- A tap in `Playing` pauses and sends note 63. A tap in `Paused` or `Saved` plays and sends note 63. A tap in the other states is denied.
- A long press in `Playing` saves and sends note 63. A long press in `Paused` saves without sending anything, because Loopify saves by itself. A long press in the other states is denied.

### New pad (bottom right)

A tap is denied. A long press opens a confirmation that fills the screen with two pads: a red trash can on top and a grey cross below. While the confirmation is open the other pads are ignored.

- The trash can stops anything in progress and clears every loop. It sends note 60 if the state is `Recording` or `Overdubbing`, then note 63 if the clock is running (`Recording`, `Playing` or `Overdubbing`), then notes 64 to 71 one after another. The state returns to `Ready` with no layers.
- The cross, or no touch for 4 seconds, closes the confirmation and changes nothing.
- If the phone has disconnected by the time the trash can is tapped, the confirmation closes and nothing is sent.

### Setup screen

The BOOT button on the side of the board switches between the main screen and the setup screen. The setup screen is a scrollable list of twelve rows, one per note, labelled `Record`, `Undo`, `Redo`, `Play / Stop` and `Clear 1` to `Clear 8`. Tapping a row sends only that note and flashes the row, which is what Loopify's "MIDI Learn" needs. It also helps to repair a mismatch by hand, for example after changing something in Loopify directly.

Sending from the setup screen never changes the tracked state. The last row, `Reset state`, sets the board back to `Ready` with no layers and sends nothing.

## Known limit

The board cannot see Loopify's real state. If something is changed in Loopify directly, a note is lost, or Loopify is restarted, the board and the app disagree. Use the setup screen to send the notes that bring Loopify back in line, then tap `Reset state` if the board's own state also needs to start over.

## Unit tests

The state machine in `main/looper.c` has no ESP-IDF, LVGL or Bluetooth dependencies and is tested on the computer with the host C compiler:

```sh
test/run.sh
```

The script compiles `test/test_looper.c` together with `main/looper.c` and runs it. The tests are not part of the firmware build.

## Source layout

- `main/app_main.c` starts the other parts and connects them. It applies each gesture to the state machine and sends the resulting actions.
- `main/looper.c` is the state machine. It decides what a gesture does and which actions to send, and never sends anything itself.
- `main/commands.c` turns actions into MIDI notes, spaces them out and sends the Note Off messages, on its own task so the screen never waits.
- `main/ble_midi.c` is the Bluetooth MIDI peripheral and knows nothing about the screen.
- `main/ui.c`, `main/ui_main.c`, `main/ui_setup.c` and `main/ui_pad.c` are the LVGL screens. They know nothing about Bluetooth and report gestures through callbacks.
- `main/display_reset.c` pulses the display's hardware reset through the board's IO expander before the display starts.
- `main/boot_button.c` debounces the BOOT button.

## Bluetooth details

- Service UUID `03B80E5A-EDE8-4B33-A751-6CE34EC4C700`, with one characteristic `7772E5DB-3868-4112-A1A9-F2669D106BF3` that supports read, write without response and notify. A read returns an empty payload.
- The service UUID is in the advertising data and the name is in the scan response, because the two do not fit in one 31-byte packet. Advertising restarts after every disconnect.
- Outgoing notifications carry a BLE MIDI header byte and a timestamp byte, derived from a millisecond clock, in front of the MIDI bytes.
- Incoming writes have the header and timestamp bytes removed and are logged to the serial console. The firmware does not act on them.
- After a phone connects, the board requests a connection interval of 7.5 to 15 milliseconds, no peripheral latency and a supervision timeout of 4 seconds. The phone decides whether to accept.

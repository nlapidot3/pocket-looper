# Pocket Looper

The Pocket Looper is a small touchscreen remote for building songs out of your own voice. You hum or sing a short phrase, it plays back as a repeating loop, and you add more phrases on top. A phone does the recording and playback. The remote in your hand tells the phone what to do, so the phone can stay in your pocket.

This repository holds the remote's firmware. The first half of this page explains how the system works for someone new to it; the second half, from "Build and flash" on, is the reference for building and changing it.

## How it works

### The three parts

| Part | What it is | What it does |
| --- | --- | --- |
| The board | A Waveshare ESP32-S3-Touch-AMOLED-1.8: a matchbox-sized computer with a 1.8 inch touchscreen, Bluetooth and a small rechargeable battery | Shows four large pads, notices taps and long presses, and sends a short message to the phone for each one. It handles no sound at all. |
| Loopify | A looper app on an Android phone (the Pro version, which accepts outside controllers) | Records the microphone, repeats each recording as a loop, keeps the loops in time with each other, mixes them, and saves the session automatically. |
| The headset | An ordinary headset with a microphone, connected to the phone | Carries your voice in and the loops back out. It is needed whenever the phone is in a pocket, because the board is only a remote control and the phone does all the listening and recording. With the phone in your hand, its own microphone and speaker can do the job. |

### How a tap becomes a loop

1. You tap the record pad on the board.
2. The board sends one small message to the phone over Bluetooth. The messages use MIDI, a standard language that electronic instruments use to say things like "this key was pressed". The board presents itself as a tiny wireless keyboard with twelve keys.
3. Loopify was taught once, during setup, what each of those keys means: one key starts or stops a recording, one is undo, one is redo, one pauses or resumes everything, and eight clear the eight loops.
4. Loopify does the action. For record, it starts recording into the first empty loop. A second tap stops the recording, and Loopify begins repeating it, lined up with the loops already playing.
5. You hear the result.

### The four pads

| Pad | Tap | Long press |
| --- | --- | --- |
| Record (red circle) | Start a recording, or finish it and let it loop. Each new recording becomes another layer, up to eight. | Nothing |
| Undo (backspace arrow) | Remove the most recent layer | Bring it back |
| Play (triangle) | Pause or resume everything | Stop. Loopify has already saved the session by this point. |
| New (plus) | Nothing, so it cannot be hit by accident | Ask for confirmation, then clear all eight loops for a fresh idea |

The line at the top of the screen shows where things stand (Ready, Recording, Playing, Overdubbing, Paused or Saved), how many layers exist, and the battery level. A pad flashes green when the board accepts a press and dark red when the press makes no sense right now, such as undo with nothing recorded.

### Who is responsible for what

Loopify owns everything about sound: recording, timing, mixing and saving. The board owns the experience in your hand: what the pads mean at each moment, which presses are allowed, and what the screen shows.

One limit follows from this split. Messages only travel from the board to the phone; Loopify never reports back. The board therefore keeps its own running account of what Loopify should be doing, based on the messages it has sent. If the two ever disagree, for example after the Bluetooth link drops for a moment, the screen can show the wrong state. The board has a second screen for that case, opened with its side button, where each message can be sent on its own and the board's account can be reset.

### Inside the board's software

The firmware is written in C for ESP-IDF 5.5 or newer, Espressif's standard toolkit for this chip. It uses NimBLE for Bluetooth and LVGL 9 for the screen, and Waveshare's board support component, `waveshare/esp32_s3_touch_amoled_1_8`, starts the display and the touch controller, so the project contains no display or touch drivers. It has four main parts that know as little about each other as possible:

- **The rules.** A small piece of plain logic that takes "this pad was tapped" and answers "the new state is Playing, and these messages should be sent". It has no knowledge of screens or Bluetooth, so it is tested on an ordinary computer with about a hundred automatic checks.
- **The screen.** Draws the pads and the status line and reports taps and long presses.
- **The Bluetooth link.** Makes the board appear to the phone as a wireless MIDI device and delivers the messages.
- **The sender.** Turns each decision from the rules into the right notes, spaced a few hundredths of a second apart so that Loopify receives a burst, such as clearing all eight loops, as separate messages.

"Source layout" below names the file behind each part.

### What is not finished

- The setup has been tried without a headset only. Recording through a headset microphone, and using all eight layers, still need testing.
- Three behaviours are assumed and not yet confirmed: that undo removes exactly one layer, that pausing silences every loop, and that clearing everything leaves Loopify stopped.
- Battery life has not been measured.
- When the Bluetooth link drops, Loopify may need to be reopened before it reconnects.

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

At the far right of the top line, the board shows a battery symbol that fills in five steps, with a lightning bolt in front of it while the battery is charging. The charge is read from the board's power chip every 5 seconds. Nothing is shown when no battery is connected or the chip cannot be read, and the layer count sits just left of the symbol.

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

The state machine in `main/looper.c` and the battery level choice in `main/battery_level.c` have no ESP-IDF, LVGL or Bluetooth dependencies and are tested on the computer with the host C compiler:

```sh
test/run.sh
```

The script compiles each test file together with the source file it covers and runs it. The tests are not part of the firmware build.

## Source layout

- `main/app_main.c` starts the other parts and connects them. It applies each gesture to the state machine and sends the resulting actions.
- `main/battery.c` reads the battery state from the AXP2101 power chip over I2C, and `main/battery_level.c` maps a percentage to one of five symbol levels.
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

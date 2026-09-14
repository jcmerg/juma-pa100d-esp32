# JUMA PA controller on an ESP32

Controls a **JUMA** amplifier through its RS-232 port — developed and tested
with the **PA-100D**, see below for the PA1000. Web dashboard
over Wi-Fi, automatic band selection via **TCI** (ExpertSDR, deskHPSDR, Thetis),
OTA updates and a diagnostic console over telnet — all on an ESP32 costing a few
euros.

![Dashboard](docs/dashboard-de.png)

```
SDR software ──TCI (WebSocket)──► ESP32 ──UART2──► MAX3232 ──RS-232──► JUMA PA
                                    │
                 browser ◄──HTTP :80 + WebSocket :81
```

- all 13 status fields of the PA live in the browser, bar and dial gauges
- band selection follows the SDR's frequency, with settle time and a TX lock
- control of OPERATE/STANDBY, band, attenuator and alarm acknowledgement
- alarms with tone, banner and a blinking tab title — the PA only beeps locally
- interface in English and German, light and dark
- firmware updates over Wi-Fi, no USB cable at the amplifier
- telnet console for configuration and troubleshooting

The screenshots show real operation — 59.8 W on 20 m, SWR 1.2, 12.3 A, and the
supply sagging from 13.68 V to 12.98 V under load. Only the SSID and the TCI
host have been replaced.

<table>
<tr>
<td width="50%"><a href="docs/dashboard-light.png"><img src="docs/dashboard-light.png" alt="Light theme"></a></td>
<td width="50%"><a href="docs/alarm.png"><img src="docs/alarm.png" alt="Alarm"></a></td>
</tr>
<tr>
<td>Light theme — system, light or dark</td>
<td>Alarm: banner, tone and blinking tab title</td>
</tr>
<tr>
<td><a href="docs/settings-de.png"><img src="docs/settings-de.png" alt="Settings"></a></td>
<td><a href="docs/dashboard-en.png"><img src="docs/dashboard-en.png" alt="English UI"></a></td>
</tr>
<tr>
<td>Settings behind the gear icon</td>
<td>The same interface in English</td>
</tr>
</table>

> **No warranty.** This firmware switches the band filters of an amplifier. The
> wrong filter can destroy the output stage. Before the first serious use, check
> the wiring with the PA's built-in RS-232 loopback test and rehearse the band
> switching in STANDBY.

---

## Hardware

You need an ESP32 (tested on ESP32-WROOM, 4 MB flash) and a **MAX3232** level
shifter. The PA has true RS-232 levels — the manual states explicitly that it is
"designed to provide and accept the standard levels" — so connecting it directly
to the ESP32 destroys its input.

**MAX232 is the wrong part**: it runs on 5 V only, and its receiver output
swings to 5 V against a 3.3 V input.

### Wiring

The RS-232 socket of the PA-100D is a **3.5 mm stereo jack**, not a DB9.

| ESP32 | MAX3232 module | JUMA (3.5 mm jack) |
|---|---|---|
| GPIO17 (`TX2`, U2TXD) | TTL **TXD** | RS-232 driver output → **tip** |
| GPIO16 (`RX2`, U2RXD) | TTL **RXD** | RS-232 receiver input → **ring** |
| 3V3 | VCC | — |
| GND | GND | **sleeve** |

Run the module **on 3.3 V**, not on 5 V.

GPIO16/17 are the default pins of UART2 and free on WROOM modules. On **WROVER**
the PSRAM occupies them — use e.g. 25/26 there and adjust `include/config.h`.
UART0 stays the USB console.

### JUMA PA1000 — untested

The PA1000 has an RS-232 remote port too, but on the **DB9 BAND DATA / COM2**
connector instead of a jack:

| ESP32 | MAX3232 module | PA1000 COM2 (DB9) |
|---|---|---|
| GPIO17 (`TX2`) | TTL **TXD** | RS-232 driver output → **pin 3** (COM2 RS232 IN) |
| GPIO16 (`RX2`) | TTL **RXD** | RS-232 receiver input → **pin 2** (COM2 RS232 OUT) |
| GND | GND | **pin 5** |

Set the **COM2 baud rate** in the PA1000 service pages to the rate in
`include/config.h` (`JUMA_BAUD`, 115200). Note that COM2 is also the firmware
update port, and that COM1 — the 3.5 mm jack — is for incoming band data, not
for remote control.

**Whether the protocol matches is unknown.** The PA-100D manual documents its
remote commands in annex D; the PA1000 manual (v1.65) does not document a
protocol at all, it only refers to a Windows remote application. Nobody here
has a PA1000 to try it on. To find out in five minutes:

```
pa =R        # ask for a status line
raw          # what came back
show         # 'bytes' counts what arrived, with the last bytes as hex
```

`bytes 0` means nothing arrives — wrong pins, wrong baud rate, or remote mode
not enabled. Bytes arriving but `lines ok 0` means the reply has a different
format than `O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0`, and the hex dump
shows what it actually sends. Band numbers are the other open question: the
mapping in `src/bands.cpp` follows the PA-100D's `=B1`…`=B9`.

Reports either way are welcome.

### Which pad is which?

The labelling of cheap modules is inconsistent — some label the TTL side, some
the RS-232 side, and the smallest boards carry no text at all. **A single
measurement resolves this regardless of variant:**

1. Apply **3.3 V and GND only**, nothing else — no connection to the PA.
2. Measure all data pins against GND. Exactly one sits at about **−5.5 V**: that
   is the **RS-232 driver output** → to the **tip** of the jack.
3. The other RS-232 pin is the receiver input → to the **ring**.

Cross-check at the PA: **ring against sleeve** must read about **−5 V** at idle,
since RS-232 mark is negative. If the tip shows that instead, the jumpers on the
frequency sense board are in the "software update" position and tip and ring are
swapped.

#### Example: the blue "RS232<->TTL" board

The widespread board from the usual marketplaces (about €2, roughly 15 × 10 mm)
is electrically suitable: charge-pump capacitors on board, and with the right
chip 3.0–5.5 V. Identifying marks:

| | |
|---|---|
| Chip (front) | `MAX3232ESE+` in SOIC-16 |
| Silkscreen (back) | `RS232<->TTL` plus a batch number such as `110` |
| Connections | 8 solder pads, 4 per edge — no connectors fitted |

**Check the chip marking.** Visually identical boards ship with either chip: one
variant carries a **MAX3232** (3.0–5.5 V), the other a **MAX232** — 5 V only,
and its receiver output drives 5 V into a 3.3 V input. Sellers list them side by
side as "3.3 V" and "5 V" versions, and the pictures often show the wrong one.
The label on the chip decides, not the listing: it has to read **MAX3232**.

The pads carry **no text**, only symbols:

| Symbol | Meaning |
|---|---|
| `\|` | GND |
| `+` | VCC |
| `→` / `←` | data direction through the board |

VCC and GND are present on **both** sides, so power can come from whichever side
is more convenient. The silkscreen says what the board does, but not which edge
is RS-232 and which is TTL — with the measurement above you do not need to know:

1. The pad at about **−5.5 V** is the RS-232 driver output → **tip**.
2. Note its **arrow direction**. The pad with the same arrow on the other side
   is the matching TTL input → **GPIO17**.
3. The other arrow pair is the return path: RS-232 input → **ring**,
   TTL output → **GPIO16**.

### RF environment

The device sits next to a 100 W linear:

- keep cables short and shielded, put a ferrite on the jack lead
- 100 nF from TX and RX to GND right at the MAX3232
- ESP32 in a metal enclosure
- do **not** tap the supply unfiltered from the PA's 13.8 V
- against ground loops: an isolated transceiver (e.g. ADM3251E) instead of the
  MAX3232

### Settings on the PA

Three things have to be right or the PA will not answer at all:

| | |
|---|---|
| Serial Speed | **115200** (factory setting is 9600) |
| Serial Port Mode | **Remote** |
| Auto Band Detect | **F-Sense** or **FT-817** |

The last point is not arbitrary: per the manual, remote and test mode are only
active with those two settings.

### Bench test without a PC

The PA has a built-in loopback test: **hold DISPLAY/CONFIG from the off state
and switch on**. That lets you check the wiring before the first band command
goes out. Exit with a short press of PWR.

---

## Commissioning

### Setting passwords

There are two, both **placeholders** in `include/config.h`, and both must be
replaced before use:

| Constant | Placeholder | Protects |
|---|---|---|
| `OTA_PASSWORD` | `changeme` | firmware upload — basic auth `admin` / password on `POST /update`, and espota |
| `AP_PASSWORD` | `changeme01` | the fallback AP `JUMA-PA` when no Wi-Fi is configured |

The first one matters more: anyone who can reach the device on the network can
upload arbitrary firmware with it.

They are set in **`platformio_local.ini`** — that file is in `.gitignore` and so
never ends up in the repository:

```ini
; platformio_local.ini
[secrets]
flags =
    -DAP_PASSWORD='"your-ap-password"'      ; at least 8 characters
    -DOTA_PASSWORD='"your-ota-password"'
```

The versioned `platformio.ini` pulls it in via `extra_configs` and holds only an
empty `[secrets]` section itself. **Without the local file the project still
builds** and falls back to the placeholders from `config.h` — enough for a first
try at the bench, not enough for operation.

Two things worth knowing:

- **Chicken and egg.** The upload that installs the new password still needs the
  **old** one. So once `JUMA_OTA_PASS=changeme ./tools/flash-wifi.sh`, never
  again afterwards.
- `tools/flash-wifi.sh` reads the password from `platformio_local.ini` itself
  when `JUMA_OTA_PASS` is unset — the same source the firmware was built from,
  so there is never a second place to maintain.

### Building and flashing

```sh
pio run                                  # build
pio run -t upload                        # first time over USB
./tools/flash-wifi.sh juma-pa.local      # over Wi-Fi afterwards
./tests/run.sh                           # host tests, no ESP32 required
```

### Setting up Wi-Fi

Three equivalent routes:

1. **Serial console** (`pio device monitor`), right after flashing:
   ```
   scan                  # list networks
   ssid MyNetwork
   pass secret123
   save                  # stores to NVS and restarts
   ```
2. **Telnet**, once Wi-Fi is up: `telnet juma-pa.local`, same commands.
3. **AP fallback**: without a valid configuration the ESP32 raises the AP
   **`JUMA-PA`**, dashboard at `http://192.168.4.1/`.

Hidden SSIDs work — the ESP32 finds them with an active scan.

The **device name** is configurable (default `juma-pa`) and applies to the Wi-Fi
hostname, mDNS and OTA — the dashboard then lives at `http://<name>.local/`.
Lower-case letters, digits and hyphens are allowed.

---

## Operation

The dashboard goes two-column from 900 px width and shows all 13 status fields
at once. Configuration sits behind the gear icon so the main view holds only
what is needed during operation.

### Level displays

| Display | Range | Warn from | High from | Rendering |
|---|---|---|---|---|
| RF | 0–150 W | 100 | 120 | segmented bar, 36 segments |
| VSWR | 1–3 | configurable, default 2.0 | configurable, default 2.5 | segmented bar |
| PA temp | 20–80 °C | 50 | 60 | dial, 180° |
| Fan | 4 steps | medium | fast | dial with step label |
| Voltage | 10–15.5 V | < 11.2 / > 14.0 | < 11.0 / > 14.8 | dial with a marker |
| Current | 0–24 A | 19.2 | 21.6 | dial |

The voltage display uses a **marker on a fixed zone scale** rather than a bar
filled from the bottom. For a quantity that moves between 12 and 14 V while the
scale starts at 10, a filled bar misleads in both directions: in normal
operation it covers the red under-voltage zone, and on over-voltage it colours
the entire range red as if everything were critical. The manual describes the
PA's own instrument as a *suppressed-zero voltmeter* — exactly this kind.

RF, current and fan keep the filled bar: those are quantities that really do
grow from zero.

The voltage limits are the **PA's defaults**: under-voltage 11.00 V, pre-limit
11.20 V, over-voltage 14.80 V (adjustable from 14.00 V), nominal 13.80 V. For
current the manual names only the **24 A hardware trip** of the MAX4373 — which
is also full scale on the unit's own meter. The warning zones at 80 % and 90 %
of it are derived. If your PA is set to different thresholds, adjust the
constants in `src/index_html.h`.

The temperature display follows the PA's unit: if it reports `F`, labels and
unit switch to Fahrenheit while the zones stay the same temperatures.

**The temperature zones belong to the dashboard, not to the PA.** The unit
itself has two adjustable thresholds, and neither appears in the status message:

| | Default | Adjustable |
|---|---|---|
| Over-Temperature Limit | **70 °C** | 50–100 °C |
| Fan Cut-In Temperature | 40 °C | 0–80 °C |

At the cut-out threshold the PA raises the alarm bit and runs the fan at
maximum. If your unit is set differently, adjust `TWARN` and `THIGH` in
`src/index_html.h` accordingly — a warning zone well **below** the cut-out makes
sense, because the alarm bit only arrives when it trips and by then there is no
time left.

A fan at step 3 is an early warning in itself: by default it starts at 40 °C and
only goes all the way up under load.

The warning threshold drives both things: the orange zone of the gauge **and**
the pre-warning with tone and banner. It should sit well below the unit's
cut-out limit.

**There is no input power.** The PA measures RF only at the output — channel 12
reverse, channel 13 forward, giving output power and SWR. The unit has no
measuring point for drive power at all.

Colours: normal `#00b33c`, warn `#ff9900`, high `#e60000`, unlit `#595959`. The
segments are coloured by their **own** position — so the bar shows the zones
throughout instead of flipping over entirely once a threshold is passed. Only
the number and the step label take the colour of the current zone.

### Language

English and German, switchable in the settings. The choice lives in the
browser's `localStorage`, so every device keeps its own; on first visit
`navigator.language` decides.

For this to work completely the firmware sends **no finished text**: hints go out
as a code plus argument (`tcidis`, `unsupported`+band, `bandok`+band …), and
`/api/config` answers with `saved` rather than a sentence in one language.

The dictionary must contain **no HTML entities** — the text is set via
`textContent`, so `&amp;` would appear literally.

### Alarm signalling

The PA beeps on an alarm — but only locally. The dashboard turns that into:

- a red banner at the top naming the alarms concerned
- an alarm tone, repeated every 5 s until acknowledged or the alarm clears
- a blinking tab title so it is noticeable in the background too

**The dashboard also warns before the cut-out.** The PA's alarm bit only arrives
once it shuts down because of over-temperature — and then there is no time left
to react; the serial connection dies with the shutdown, so the message would
never arrive at all. When the temperature passes the configured warning
threshold the same banner and tone therefore trigger, even though the PA reports
no alarm yet. Threshold and red zone are configurable in the settings
(`tempwarn`, `temphigh`, `tempalarm`), defaults 50 ° and 60 °.

The same applies to SWR: the PA's trip limit is **3.0** by factory default
(adjustable 1.0–10.0) and likewise absent from the status message. Warning and
red zone are configurable via `swrwarn`, `swrhigh` and `swralarm`, defaults 2.0
and 2.5. While receiving the PA reports 0.0 — so the warning only applies during
transmit, and the PA's alarm bit would only come when the cut-out fires.

These settings take effect **immediately**, without "save & restart" — just like
the controls in the main view. They need no reboot, and a switch that only
becomes effective through a distant save button looks like it does nothing. The
same holds for "standby before update".

The tone only starts after the page has been clicked once — that is the
browsers' autoplay policy. It can be switched off in the settings.

**Browser notifications do not work over `http://`.** The Notification API is
restricted to "secure contexts". The tricky part: Chrome does not remove the
`Notification` object, it silently sets the permission to `denied` — which looks
as if you had refused yourself. The dashboard therefore queries
`isSecureContext`, names the real reason and disables the button. Tone and
banner are unaffected.

### Appearance

System, light or dark, switchable in the settings and remembered in
`localStorage`. Without a choice the page follows `prefers-color-scheme`. The
signal colours are slightly darkened in the light theme — `#ff9900` is barely
readable as text on white.

### The hint below the state

It always shows the **current reason**, never a one-off event; otherwise a stale
message would linger after a change and there would be none at all right after
start-up. `bandControl()` sets it afresh on every pass:

| Code | Meaning |
|---|---|
| `aboff` | TCI band selection off |
| `tcioff` / `tcidis` | TCI client disabled / not connected |
| `tcinofreq` | connected, but no frequency yet |
| `paoff` | PA not responding |
| `unsupported` | band not covered by the PA |
| `bandok` / `bandset` | band follows TCI / has been switched |
| `tciauto` | TCI gone, PA put back on its own band select via `=A` |
| `selstuck` | PA stays on `A` although TCI band selection wants it on `M` |

Only genuine obstacles are coloured orange; "automatic off" and "band follows
TCI" are state information.

### The "PA band select" switch

Shows and sets field 2 (`A`/`M`) — the **state** of the AUTO button, not the
configured method. The method (F-Sense, Yaesu CAT, KX2/KX3, JUMA-TRX2, FT-817,
Manual) lives in the unit's configuration and does not appear in the status
message at all.

`Auto` sends `=A`. For `Manual` there is no command of its own — the firmware
sends `=B<current band>` instead: per the manual that is a manual band
selection, so it moves `A` to `M` without changing the band.

The switch is **locked while TCI band selection is running**, and the firmware
additionally rejects the command then. There the ESP32 determines the band and
actively holds the PA on `M` — the two must not share band selection. With TCI
band selection off, `A` and `M` are free to choose.

---

## Band automation over TCI

TCI is not tied to ExpertSDR — **deskHPSDR** and **Thetis** speak it as well,
and the ports differ (ExpertSDR3 40001, deskHPSDR 50002). Host and port are
therefore freely configurable.

Evaluated messages:

| Message | Use |
|---|---|
| `vfo:0,0,<hz>;` | receive frequency, primary source |
| `dds:0,<hz>;` + `if:0,0,<offset>;` | fallback while no `vfo` has been seen |
| `trx:0,<bool>;` | TX state, blocks band changes |

The automation is **off** after a reset (fail-safe) and is enabled in the
dashboard; the state lives in NVS.

1. A frequency arrives via TCI.
2. **Settle time 150 ms** — only send once the frequency has stopped moving,
   otherwise tuning across a band edge fires a band command every time.
3. If the PA does not cover the band (6 m, 4 m, 2 m, 60 m, LF/MF) **no** `=Bn`
   goes out; the dashboard explains why.
4. **Never switch during TX** — neither when TCI reports `trx:0,true` nor when
   the PA itself indicates TX. The change is made up for afterwards.
5. A command is only sent when the PA reports a band other than the target. That
   also catches up when the PA was off in between.
6. If the PA reports `A`, the ESP32 pulls it back to `M` with a `=Bn` on the
   band already in use — otherwise F-Sense would eventually drag it elsewhere.
   At most three times at 5 s intervals; if the PA will not take it, that is
   reported instead of firing forever.

### Fallback on losing TCI

`=Bn` is a *manual* band selection and thereby moves the PA from `A` to `M`. So
after losing TCI the PA does **not** fall back to its own band selection by
itself, but stays on the last commanded band.

The switch **"fall back to the PA's automatic on TCI loss"** (`tcilosta 1`)
sends `=A` 15 s after the connection breaks. **Which method** then applies is
set in the PA's configuration; if that is on `Manual`, `=A` achieves nothing.

The 15 s deliberately sit above the 5 s reconnect interval so a brief dropout
does not reconfigure the PA.

What matters is the **state**, not the event: as long as TCI is missing and the
PA sits on `M`, the firmware keeps trying — at most three times at 5 s
intervals, so a deliberate choice made at the unit is not overridden forever.
The counter restarts when the PA comes back up; otherwise switching the PA off
and on would have disabled the fallback for good, because it returns on `M`.

Nothing is sent during TX or while the PA is offline. When TCI returns, the next
`=Bn` puts the PA back on `M` anyway.

### Two pitfalls with TCI

**Clear `Sec-WebSocket-Protocol`.** `arduinoWebSockets` sends
`Sec-WebSocket-Protocol: arduino` by default. deskHPSDR then closes the
connection **silently** — no HTTP error, just a TCP teardown showing up as
`TIME_WAIT`. Measured in isolation:

```
standard headers only                -> HTTP/1.1 101 Switching Protocols
+ Sec-WebSocket-Protocol: arduino    -> (no reply)
+ Origin: file://                    -> HTTP/1.1 101 Switching Protocols
+ User-Agent: arduino-...            -> HTTP/1.1 101 Switching Protocols
```

TCI has no subprotocol, which is why `tci.cpp` calls
`ws.begin(host, port, "/", "")` with an empty fourth argument. Without it the
client **never** connects, and from the outside the fault looks like a network
or firewall problem.

**A client cannot stop TX.** The obvious idea is to take the transceiver out of
transmit via TCI on high SWR or an alarm. Measured against deskHPSDR with a
running, locally keyed TX:

```
trx:0,false;         -> no effect, server answers with trx:0,true
trx:0,false,tci;     -> no effect
trx:0,0;             -> no effect
tx_enable:0,false;   -> no effect
```

The server *reads* the command — it answers immediately with the current state —
and refuses it. Sensibly so: a foreign process should not be able to take the
key away from you.

`tools/mock-tci.py` is a minimal TCI server for testing without SDR software.
`tools/tci-trx-test.py` and `tools/tci-stop-variants.py` re-check the question
above against your own software; neither ever sends anything that turns TX *on*.

---

## Failure behaviour

Principle: on any fault **nothing is switched**, rather than guessing.

| Fault | Behaviour |
|---|---|
| TCI software closed | retry every 5 s, indefinitely. No `=Bn`, the PA stays on its band. Frequency and TX state are **discarded**, not frozen |
| Wi-Fi gone | the ESP32 reconnects by itself; the PA is still polled, the serial side does not depend on the network |
| ESP32 restarts | polling stops → the PA falls back to STANDBY after 5 s, provided it was put into OPERATE *by remote control*. OPERATE set at the front panel stays |
| PA off / cable unplugged | `OFFLINE` after 3 s, no band commands, recovers by itself |
| Band not covered | no `=Bn`, the PA stays on the old band, the hint explains it |
| TX active | the band change is deferred and made up once TX ends |
| Alarm from the PA | displayed, **not** acknowledged automatically |
| Main loop stuck | task watchdog (20 s) reboots |
| Wi-Fi stays gone | reconnect every 15 s, reboot after 5 minutes |
| RSSI persistently poor | reconnect after a minute below −75 dBm, picking the strongest AP |

On a TCI disconnect, frequency and TX state are reset deliberately. Otherwise a
frozen `tx = true` — a break in the middle of transmitting — would block band
changes **permanently** after the reconnect. The server sends the complete state
again on connect anyway.

---

## The PA's protocol

Commands in ASCII, terminated with `\n\r` (0x0A 0x0D):

| | |
|---|---|
| `=R` | request status |
| `=O` / `=S` | OPERATE / STANDBY |
| `=A` | the PA's automatic band selection |
| `=Bn` | band, n = 1 (160 m) … 9 (10 m) |
| `=Gn` | attenuator, n = 1…4 |
| `=C` | clear alarm |
| `=Pn` | power off, n = 0 without / 1 with saving the state |

Reply to `=R`, **13** fields, e.g. `O:A:T:C: 5:1:1.0:14.09: 8.1: 27.2: 26:0: 0`:

| # | Value | Meaning |
|---|---|---|
| 1 | `O`/`S` | operate / standby |
| 2 | `A`/`M` | band selection automatic / manual |
| 3 | `T`/`R` | transmit / receive |
| 4 | `C`/`F` | temperature scale |
| 5 | 1–9, 10 | band, 10 = unknown |
| 6 | 1–4 | attenuator: G1 = 6 dB, G2 = 4 dB, G3 = 2 dB, G4 = 0 dB |
| 7 | n.n | VSWR |
| 8 | nn.nn | supply voltage |
| 9 | nn.n | current |
| 10 | nnn.n | output power |
| 11 | nnn | temperature |
| 12 | 0–3 | fan: off / slow / medium / fast |
| 13 | **HH** | alarms, **hexadecimal** |

Alarm bits: 0 high SWR · 1 over-current · 2 high temperature · 3 high voltage ·
4 low voltage pre-limit · 5 low voltage final limit.

### Three things the manual does not quite tell you

**Field 13 is hexadecimal.** Reading it as decimal decodes wrongly from `0x0A`
on: `10` (low voltage pre-limit, bit 4) then appears as over-current plus high
voltage. `tests/test_parse.cpp` checks exactly that.

**The line terminator is three bytes.** Measured: 45 bytes per line for 42
characters of payload, and the hex dump reproducibly ends on `0D 0A 0D`
(CR LF CR) — not the documented `\n\r`. The parser treats every CR and LF as a
line end and discards empty lines. Anyone matching `\n\r` exactly is relying on
the surplus bytes landing harmlessly by chance.

**The attenuator is stored per band.** Field 6 changes along with a band switch
— that is not a bug in the firmware.

Two further points that are in the manual but easily missed: the PA falls back
to STANDBY by itself **5 s** after a remote `=O` when no more messages arrive
(hence the 500 ms poll). And field 6 is an **attenuator**, not a gain factor.

---

## Maintenance and troubleshooting

| | |
|---|---|
| Dashboard | `http://juma-pa.local/` — the raw status line is in the header |
| State as JSON | `curl http://juma-pa.local/api/state` |
| Console | `telnet juma-pa.local`, or `pio device monitor` over USB |
| Flashing | `./tools/flash-wifi.sh` or the form in the dashboard |

The PA link, the TCI client and the band logic each run in a **task of their
own**, guarded by a mutex. The PA leaves remote mode 5 s after the last
command, and a band change that arrives late puts the PA on the wrong band for
the next transmission — neither may wait behind a web server that can block
for seconds. The web server itself stays in the main loop; a long block there
costs the dashboard its WebSocket, and the browser reconnects on its own.

### Console commands

```
show                current configuration and state
scan                scan for Wi-Fi networks
hostname <name>     network name for Wi-Fi, mDNS and OTA
ssid <name>         set the Wi-Fi SSID
pass <secret>       set the Wi-Fi password
tci <host> [port]   TCI host of the SDR software (port default 50002)
tcien <0|1>         TCI client off/on
autoband <0|1>      band selection via TCI off/on
otastby <0|1>       send =S to the PA before a firmware update
tcilosta <0|1>      on TCI loss send =A (the PA selects again)
tempwarn <deg>      pre-warning from this temperature
temphigh <deg>      red in the gauge from here
tempalarm <0|1>     temperature pre-warning off/on
swrwarn <value>     SWR pre-warning from this value
swrhigh <value>     SWR red in the gauge from here
swralarm <0|1>      SWR pre-warning off/on
sel <a|m>           PA band select to automatic / manual
save                save and restart
reboot              restart only
pa <cmd>            raw command to the PA, e.g.  pa =R
raw                 last status line from the PA
quit                close the telnet session
debug <0|1>         trace timings (web, loop, Wi-Fi) - not stored
```

`debug 1` traces at runtime — timings per step of the loop, what goes to the
PA and how it answers, WebSocket and Wi-Fi events, each line with a timestamp.
Costs nothing while off, and a restart turns it off again.

```
[31.801] web: / 15775 B in 68 ms (231 kB/s), RSSI -67 dBm
[35.649] loop: 2234 runs, avg 2221 us, worst 116645 us | RSSI -67 dBm, ps 0 | heap 224124
[40.112] slow: http.handleClient took 10059 ms
[40.180] pa: 13479 ms since the last command - remote drops out at 5000
```

`show` also names the AP it is on (BSSID, channel), the boot count and the last
reset reason; `scan` lists BSSIDs. The telnet line editor behaves as usual:
backspace, cursor up, Ctrl-C, Ctrl-U, Ctrl-D.

`show` counts received **bytes** separately from understood lines and shows the
last raw bytes as hex. That narrows down the wiring:

| Display | Meaning |
|---|---|
| `bytes 0` | nothing arrives at all — PA off, remote mode not set, or tip/ring swapped |
| `bytes > 0`, `lines ok 0`, hex looks like junk | wrong baud rate or framing |
| `bytes > 0`, `dropped > 0` | wiring is fine, but the reply is not a status line |

### Firmware update over Wi-Fi

`espota`/ArduinoOTA opens a listener on the **host** and has the **device
connect back**. In segmented networks that fails: authentication on port 3232
works, then comes `No response from device`.

The ESP32 therefore also accepts the firmware itself via `POST /update`
(`Update.h`, basic auth `admin`):

```sh
./tools/flash-wifi.sh juma-pa.local      # takes the password from platformio_local.ini
# or by hand:
curl -u admin:$JUMA_OTA_PASS -F firmware=@.pio/build/esp32dev/firmware.bin \
     http://juma-pa.local/update
```

The dashboard offers an upload field as well. There the browser asks for `admin`
and the password beforehand — the page triggers that with a protected
`GET /update` before the file is sent. Without it the whole megabyte would be
uploaded first, then a 401 would arrive, and after the prompt it would start
over.

This runs host → device and is independent of network segmentation. mDNS
(`juma-pa.local`) does not resolve across segment boundaries either, so use the
IP address there. `[env:ota]` in `platformio.ini` remains for the case of
flashing within the same segment.

Before writing, the firmware sends the PA a `=S` — `loop()` does not run while
flashing. **Switchable** via `otastby 0`: with frequent development uploads it
otherwise takes away the operating mode every time.

---

## Development

The dashboard is **served compressed**. `tools/gzip_html.py` runs as a `pre:`
step before every build, reads the raw string from `src/index_html.h` and
generates `src/index_html_gz.h`. So only the readable file is ever edited; the
generated byte array is in `.gitignore`.

| | uncompressed | gzip |
|---|---|---|
| transferred | 46.4 kB | **16.0 kB** (34 %) |
| page load | 0.24–0.27 s | **0.08–0.12 s** |
| flash | 81.7 % | **78.9 %** |

This pays off twice: the script sits at the end of the document and only runs
once everything has arrived. With a poor radio link the page previously sat
there for seconds with its frames but without buttons or gauges — which looks
like a hang but is merely a half-loaded page.

| File | Content |
|---|---|
| `include/config.h` | pins, timings, passwords, `FW_VERSION` |
| `src/juma_status.h/.cpp` | status parser, free of Arduino → testable on the host |
| `src/juma.h/.cpp` | serial driver: polling, command queue, line splitting |
| `src/bands.h/.cpp` | frequency → JUMA band index |
| `src/tci.h/.cpp` | TCI client, supplies frequency and TX state |
| `src/web.h/.cpp` | HTTP + WebSocket server, settings in NVS, OTA endpoint |
| `src/console.h/.cpp` | console on UART0 and telnet :23 |
| `src/index_html.h` | dashboard, one file — the only one that gets edited |
| `tools/gzip_html.py` | compresses it at build time into `src/index_html_gz.h` |
| `src/main.cpp` | band controller, watchdog, mDNS, wiring |
| `tests/test_parse.cpp` | 102 checks for the status parser and band mapping |

`./tests/run.sh` runs on the host and needs no ESP32 — the status parser and the
band mapping are deliberately free of Arduino dependencies.

### Platform pitfalls

**`WiFi.setSleep(false)` must come AFTER `WiFi.begin()`.** Set before, it is
discarded again on connect. With modem sleep active every round trip waits for
the next beacon:

| | with sleep | without |
|---|---|---|
| page (24 kB) | 6–20 s, partly aborted | **0.19–0.35 s** |
| throughput | ~1.2 kB/s | ~100 kB/s |
| `/api/state` | 26–80 ms | 26–80 ms |

That the small JSON reply was *not* slower is the decisive clue: one round trip
cost a beacon interval, so many round trips weighed accordingly.

**`WIFI_FAST_SCAN` takes the first AP it finds, not the strongest.** That is the
arduino-esp32 default. With several access points on one SSID the device easily
ends up on the weakest — with packet loss that looks like a firmware fault.
Measured on a setup with two APs of the same SSID (channel 5 weak, channel 10
strong) and a foreign network on the overlapping channel 3:

| | before | with `WIFI_ALL_CHANNEL_SCAN` + `WIFI_CONNECT_AP_BY_SIGNAL` |
|---|---|---|
| RSSI | −74 dBm | **−64 dBm** |
| packet loss | 45 % | **0 %** |
| ping median / max | 19 ms / 5010 ms | **5.7 ms / 29 ms** |
| page (40 kB) | 0.4–0.5 s | **0.15–0.21 s** |

**And the ESP32 does not roam.** Once associated it stays with its AP even when
the signal collapses — arduino-esp32 has no roaming logic. With several APs on
one SSID (CAPsMAN, UniFi and the like) it then clings to the worst one without
the connection ever formally dropping. That is why the supervisor reconnects
when RSSI stays below −75 dBm for more than a minute, at most every five
minutes. The scan then picks the strongest AP again.

On the other side, an access list that rejects clients that are too weak helps —
in CAPsMAN for instance `signal-range=-120..-80 action=reject`. The client then
has to look for another AP instead of clinging to a poor one. Choose the
threshold with care: set too high, the device cannot get onto the network from
some locations at all.

The `scan` command on the console shows what the device itself hears — that
separates "too far away" from "channel congested" before you go looking in the
firmware.

**No Wi-Fi supervisor means the watchdog does not help.** Calling `WiFi.begin()`
only in `setup()` is not enough. If Wi-Fi drops, the loop keeps running happily,
keeps feeding the watchdog and keeps polling the PA — the device is merely
unreachable. From the outside that is indistinguishable from a crash. Hence a
supervisor that checks every 5 s, reconnects every 15 s and reboots after
5 minutes without Wi-Fi.

**`WEBSOCKETS_SERVER_CLIENT_MAX` is 5 by default.** Every tab and every reload
occupies a slot, and connections the browser did not close cleanly linger. Once
all slots are taken by such corpses the page still loads but receives no data —
which looks like a hang. Hence 8 slots plus `enableHeartbeat(10000, 5000, 2)`
— the 5 s reply window rides out a stalled loop or a hiccup on the radio link,
a client that stops answering altogether is still gone within 5 s.

**`WEBSOCKETS_TCP_TIMEOUT` is 5000 ms by default.** A client that does not keep
up blocks the write — and with it the single main loop — for five seconds. On a
LAN a healthy client needs a fraction of that, so the build sets 250 ms.

**The browser needs a watchdog of its own.** The server can stop sending without
closing the connection; then no `onclose` arrives and the page silently holds
the last state — green dot, old frequency, everything frozen. If updates stop
for more than four seconds, the dashboard treats the connection as dead, says so
and rebuilds it.

---

## Licence

MIT — see [LICENSE](LICENSE).

Not affiliated with Juma Radio; "JUMA" and "PA-100D" belong to their respective
owners. The protocol details come from the official
[PA100-D Operating Manual v4.00a](https://www.jumaradio.com/juma-pa100/).

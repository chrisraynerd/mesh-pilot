# Mesh Pilot

An independent, open-source, phone-free MeshCore terminal for ESP32 touchscreens.

Current development version: **0.15.1**.
Start with [the short user guide](QUICK-START.md).
Public source: https://github.com/chrisraynerd/mesh-pilot
Licence: MIT; dependency notices are listed in THIRD_PARTY.md.

Independent touchscreen MeshCore messenger for the LCDWiki E32R28T
(ESP32-WROOM-32E N4, 4 MB flash, ILI9341V and XPT2046 resistive touch).
No WADAMESH code is used. The Heltec remains on its MeshCore companion firmware.

## Connecting and messaging

On first boot, hold each calibration cross and release between taps.
Calibration survives updates. Keep the Heltec powered nearby and the phone app
disconnected. Choose **Find MeshCore radios**, tap your Heltec, enter its six-digit
Bluetooth PIN and connect. The initial connection retries once if needed.
Bluetooth bonding remembers the radio and reconnects after a CYD restart without
storing the PIN. **Net > Auto on/off** changes startup reconnect behaviour.
Dropped links reconnect automatically while Auto link is enabled, using delays of
5, 10, 20, 40 and then 60 seconds. No PIN is silently guessed and no message is
automatically resent. **Back**, **Leave**, scanning, or turning Auto link off
cancels retry; **Saved** retries the remembered radio immediately. Missing/stale bonds require scanning and entering the PIN again.

Channels and contacts load from the Heltec after the handshake. Tap a channel,
then **Write**, type and **Send**. **ABC** changes case; **123** selects digits and
punctuation. Outgoing messages currently allow up to 120 ASCII characters.
**Inbox** shows direct messages. **Contacts > person > Message > Write** opens
a direct conversation. Incoming messages do not interrupt a draft or keyboard.
An unsent draft is saved locally after two seconds without editing and restored
for review after restart. Only one active draft is kept per radio; selecting a
different recipient clears it. No draft is sent or retried automatically.

Chat uses blue time, orange username and white body in a continuous wrapped
flow, each message beginning on the next line. Conversations now scroll
continuously instead of being split into pages. Drag the text downward to reveal
older messages and upward to return toward newer ones. **Latest** jumps to the
bottom; **Write** and **Back** remain fixed underneath. Received UTF-8 is decoded before rendering;
known emoji become readable labels such as [cat] and [hand], accents are
transliterated and unsupported characters receive [U+XXXX] labels. Raw journal
bytes remain unchanged. Names in channel messages are sender-supplied text.

## Stylus scrolling (0.12)

Channel conversations, individual direct conversations and the direct inbox use
a continuous text timeline over the same latest 80 records. New conversations
open at the bottom; short conversations are bottom-aligned. New messages follow
the bottom only while already there. When reading older messages, the top
visible message/line stays anchored, including when retention removes an earlier
message. If the anchor itself ages out, the oldest retained text is shown.
**New messages v** appears when relevant messages arrive below the reading
position. **Latest v** returns to the bottom at any time.

Dragging uses held single-touch readings, a small movement threshold, a three-
sample median filter and release debounce. It has no momentum scrolling.
Gestures beginning in the message area consume the touch until release, so
dragging onto Write or Back does not activate a button. A scrollbar indicates
position. Completed 16-bit strips are painted offscreen before being pushed to
the LCD. A 16-row strip uses about 9.5 KB; allocation retries with smaller strips
down to a single row if memory is fragmented. The buffer is released on other
screens. An emergency direct-drawing fallback remains if even one row cannot fit. Existing time/name/message colours,
history storage, send controls and dim-to-wake behaviour are retained.

Boot fixtures cover scroll bounds, follow versus hold behaviour, relevant versus
unrelated arrivals, stable anchors after an append and after an oldest-message
eviction, and the jump to latest. No test messages are written to the journal
or sent over the radio. Physical drag feel requires user review.

## Standalone operation (0.10)

- **Setup** on the channel screen shows Heltec battery voltage, radio storage
  when reported, and this screen's storage usage. Voltage is reported directly;
  no unreliable battery percentage is invented. Zero/implausible readings are
  labelled unavailable. This is the Heltec's battery, not a separate CYD battery.
- Brightness and automatic dimming are saved on the CYD. After a minute without
  touch, dimming lowers the backlight while Bluetooth and reception continue.
  The first tap wakes it and is consumed, preventing an accidental button press.
- Channel buttons, the direct inbox and contact rows display unread counts.
  Opening a conversation marks messages read; messages remain unread while the
  display is dimmed and are marked when the conversation is woken. Markers are stored per radio in
  a small checksummed, atomically replaced local file; interrupted/corrupt marker
  files fall back to unread rather than changing message history. Counts cover
  the latest 80 messages kept in RAM. Incoming messages preserve a scrolled reading position
  and a compose draft instead of moving the reader to a different position.
- An unexpected Bluetooth drop preserves the draft in RAM and returns to the
  conversation/composer after a successful reconnect. Manual Leave cancels
  retries. Missing/failed pairing stops retries and asks for a PIN through the
  normal pairing screen. A timeout in an optional battery query disables it for
  the automatic reconnect cycle, leaving messaging available.
- USB diagnostics: **STATUS?** prints only connection state, battery, contact
  count, unread count and retry status. **LINKTEST** deliberately drops only
  the local Bluetooth connection to verify recovery; it transmits no RF message.

## Draft recovery (0.11)

Unsent text is stored in a checksummed per-radio local draft file, with atomic
replacement. The original destination public key is kept for a direct draft.
Channel drafts store only a fingerprint of the channel key and are not restored
onto a slot with a different key. The local draft file contains message text,
like the existing journal; it stays on the screen's flash and is not published.
There is one active draft per radio. A sudden cut during the two-second save
delay can lose the latest edit. Accepted sends remove the saved draft. If power
is lost before that removal finishes, review any restored text before sending.

USB **STORAGETEST** uses a reserved synthetic namespace to check read-marker and
draft replacement, corruption rejection, destination/fingerprint matching and
the production restore path. It restores the user's RAM state and removes its
temporary fixture files. It does not send an RF message or modify user journals.

Build time now lives in a generated header under .pio, avoiding a full rebuild
of every dependency just to refresh the application clock baseline.

## Contacts, nodes and custom channels (0.9)

- **Contacts** on the channel screen opens people by default. **Net → Nodes**
  starts with all known node types. Both lists have a cycling type filter,
  **Find** by name/key prefix, and alphabetical sorting with favourites first.
  Filters include all, people, repeaters, rooms, sensors, favourites and heard
  this session. Blank search clears the search. P/R/G/S identifies node type;
  green means an advert was heard this session, not proof of a working route.
- Tap a row for details. **Fave/Unfav** updates the Heltec's favourite flag.
  **Message** opens a person's conversation. Repeaters/rooms retain **Status**;
  **Find path** is available as before. Neither loading nor browsing sends an
  RF probe. **Load** refreshes the radio's contact table.
- **Edit** allows renaming and type changes, confirmed removal and resetting
  the saved route. Updates preserve the full received contact frame, including
  other flag bits, path, original UTF-8 name until edited, GPS and advert time.
  Radio contact writes may take five seconds to reach permanent storage; keep
  the Heltec powered during that interval. A reported route update requires a
  reload before renaming/favouriting, so an older cached route is not restored.
- **Add** accepts a name, type and full 64-digit hexadecimal public key. A
  newly entered contact starts without a saved route. Manual public-key entry
  is supported; QR codes/contact URI import are not yet implemented.
- New full adverts may describe nodes that are not saved on the radio. They
  are conservatively labelled **New** until a contact-table reload or explicit
  save confirms storage. **Edit → Save** adds one without retyping its key.
  Direct messaging requires saving first. Radio contact-deleted/full events
  are handled, and confirmation uses the intended key even if the list changes.
- **Channels → Manage** shows every supported slot, including empty ones.
  Tap a slot, enter a name and choose **#channel** or a shared private key.
  Hashtag keys use the first 16 bytes of SHA-256 of the exact typed name;
  names are case-sensitive and anyone knowing the name can derive its key.
  **Enter key** accepts 32 hexadecimal digits to join a private channel;
  **New key** uses the ESP32's random source to create a 16-byte key.
  Give intended members the same name and key. Keys are displayed only in the
  channel editor and are never included in serial logs or message history.
- **Save** and **Remove** have confirmation screens. Changes are sent to the
  Heltec and read back before the local channel list changes. Replacing the
  key starts a new local chat view for that slot; a journal boundary preserves
  the earlier raw records without showing them under the new channel identity.
  Existing stored records and Bluetooth/touch settings survive app-only upload.

Protocol reference: https://github.com/meshcore-dev/MeshCore/blob/main/docs/companion_protocol.md

## Temporary send feedback

The existing bottom line shows sending status without adding permanent chat rows.

- **Radio accepted; listening for a repeat** confirms local command acceptance.
  Channel sends are watched for 45 seconds. A byte-for-byte encrypted-payload
  match in a relayed flood with a nonempty path shows **Repeat heard**. This
  confirms an echoed copy, not delivery to a particular recipient.
- **No repeat heard; unconfirmed** does not establish delivery failure: the
  outgoing message or return copy may have followed a different path or RX
  notifications may have been missed.
- Direct messages match the radio's expected acknowledgement tag. **Delivered**
  confirms receipt at the recipient radio, not reading by the person. **No ACK**
  is unconfirmed; the return acknowledgement may have been lost.
- Results disappear after about twelve seconds. Four recent sends are tracked;
  late evidence can still match for three minutes if its slot remains available.
  Restart/disconnect ends tracking. No automatic resends occur in this client.
  A rejected command retains the draft; timeout/write failure may already have
  transmitted, so check before manually retrying.

Echo correlation reconstructs MeshCore AES/HMAC payloads using the original
UTF-8 node name, channel key, send epoch and exact text. Scoped/unscoped flood
paths and one-, two- and three-byte path identifiers are validated. Keys remain
in RAM and never enter serial logs or message journals.

## History and clock

The CYD polls the radio queue every three seconds, immediately continuing when
messages are waiting. Full received frames are journalled before requesting the
next message. This consumes the Heltec's offline queue, so collected messages
may not later appear in the phone app. History belongs to this screen, per radio
address. The latest 80 records are loaded in RAM; the raw journal persists up to
512 KiB per radio. Storage failure/fullness pauses reception rather than silently
overwriting old history. App-only uploads preserve history, bonding and calibration.

Times are UTC. Received messages retain the sender's timestamp. The radio clock
is accepted only if at least as recent as the terminal's build/saved lower bound.
**Net > Clock** accepts twelve digits, **YYYYMMDDhhmm** in UTC, with calendar
validation. USB at 115200 baud accepts **TIME UnixSeconds** followed by newline;
**CLOCK?** reports time and source. Local corrections are sent to the Heltec,
which may reject backwards changes. Local time is retained if the radio rejects.
A checkpoint is saved each minute; full power loss cannot reveal elapsed off-time,
so the build/checkpoint fallback is explicitly approximate. Old recorded times
are not rewritten. Bluetooth connection itself proves only screen-to-radio access.

## Network checks

**Net > Read stats** reads local radio counters and queue length. Counters are
since Heltec boot; changes are relative to the first read this connection.
RX indicates reception and TX indicates local transmissions, neither establishes
end-to-end delivery. Frequency/bandwidth/SF/coding rate are read, not modified.

**Net > Nodes > repeater/room > Status** sends one status request using its
saved route. **Find path** sends one flood discovery request. Only a matching
remote reply establishes a round trip. Path results show outward/back hop counts;
zero means no intermediate repeater. Timeout can reflect coverage, stale routes,
permissions, unsupported firmware or lost packets. Opening lists, loading contacts
and reading local stats send no RF probes. No remote tests repeat automatically.

## Build, installation and recovery

For a normal checkout, install PlatformIO and run `pio run` in this directory.
The included platformio.ini pins the target and dependencies. Set the upload port
for your own computer. The application-only install command below preserves an
existing compatible partition table; a fresh board needs the bootloader and
partition table generated by PlatformIO as well.

For the original development workspace run
`outputs/cyd-mesh-terminal/build.ps1` in PowerShell.
It uses the bundled Python/PlatformIO environment and a temporary R: alias to
avoid Windows long-path compiler failures. Platform espressif32 is pinned to
6.10.0, Arduino/esp32dev with huge_app partitioning. Libraries are TFT_eSPI 2.5.43,
NimBLE-Arduino 1.4.3 and the pinned XPT2046 commit in platformio.ini.
The TFT_eSPI TOUCH_CS warning is expected: touch uses its own controller/library.

Application-only update with esptool 5 (verify the port and CYD MAC first):

```
python -m esptool --chip esp32 --port COM8 --baud 460800 write-flash 0x10000 firmware.bin
```

The saved prototype binaries contain only the application: never flash them at 0.
Existing huge_app layout: NVS 0x9000, otadata 0xe000, app 0x10000 (3 MiB),
LittleFS 0x310000 (896 KiB), coredump 0x3f0000. Flash only the CYD, not the Heltec.
USB identity: ESP32-D0WD-V3 rev 3.1, 4 MB. Verify your own device identity before flashing.

In the original development workspace, the sibling
**device-backup/original-cyd-flash.bin** is the original full 4 MiB
image with SHA-256 recorded alongside. Restoration writes that full backup at
address 0 and replaces the prototype, calibration, bonds and stored history.
Keep it separate; ordinary updates must not overwrite it or the data partitions.

## Hardware reference

https://www.lcdwiki.com/2.8inch_ESP32-32E_Display

Display: SCK14, MOSI13, MISO12, CS15, DC2, reset tied to EN, backlight21.
Touch on separate HSPI: SCK25, MOSI32, MISO39, CS33, IRQ36.

## Verification history

- Original flash backed up before modifications; device identity and 4 MB verified.
- 0.1-0.3: user confirmed touch calibration, Heltec discovery, encrypted connection,
  channels, keyboard and local message queue acceptance.
- 0.4: network diagnostics compiled/flashed and boot fixtures passed; remote tests
  timed out, leaving repeater reachability unconfirmed.
- 0.5: user confirmed the saved 23:09 UTF-8 message became readable. Encrypted
  bonded reconnection without a PIN was observed after restart.
- 0.6: UTC synchronisation was acknowledged by Heltec; after restart the clocks
  matched epoch 1793402684, 23:24:44 UTC on 30 October 2026. Calendar checks passed.
- 0.7: compact coloured chat compiled/flashed and booted with automatic reconnect.
- 0.8: delivery crypto/framing fixtures passed at boot, automatic encrypted
  reconnection worked and clocks matched. User observed **No repeat heard /
  unconfirmed**, validating the timeout feedback, not successful relaying.
- 0.9: compiled and installed with verified flash hash on the confirmed CYD.
  Final application size 771,616 bytes (765,037 bytes linked flash usage),
  static RAM 41,108 bytes; free heap at Ready 195,072 bytes. Boot passed message,
  Unicode, network, clock, AES/HMAC/ACK, channel-key/read-back and management
  fixtures. Management checks exercised people/repeater/favourite filters,
  case-insensitive search, favourite-first sorting, contact frame preservation,
  new-contact route defaults and journal boundary replay without BLE writes.
  Automatic encrypted/bonded reconnect succeeded, clocks matched, all 40
  channel slots were read and a complete list of 121 contact frames loaded.
  Normal receive polling resumed without overflow. Physical navigation and
  user-selected contact/channel writes await user testing.

Release SHA-256: `90ffb8cb5bd4ef020831e2285b4225141f5186cdc85373a8c7716324446e736d`.
**Net > Leave** disconnects without clearing saved bonding or history.

No RF test messages are transmitted by boot fixtures or installation checks.

### 0.10 hardware verification

Compiled and flashed with verified hashes. Boot passed all existing fixtures and
the new battery parsing, retry/wraparound, unread isolation and read-marker
fixtures in RAM. A diagnostic local BLE disconnect produced a five-second retry
and authenticated automatic reconnection without a PIN or resend. A later status
query reported ready=1, 121 contacts and Heltec battery **3999 mV**. Five messages
in the current local history were counted unread. Display interaction, dim/wake
feel and real-world notification use still need human review.

## Current scope

Mesh Pilot is a usable standalone messaging client, not full phone-app parity.
Maps/GPS UI, room-server login/conversations, camera-based QR import and advanced
radio configuration are not implemented. Read/unread counts cover the 80 recent
messages held in RAM. Over-the-air repeat/recipient-ACK success depends on the
actual mesh and has not been demonstrated by these local hardware checks.
Touch ergonomics, contact/channel config writes and live RF use still benefit
from human testing. The v0.9.0 checkpoint preserves the earlier user-tested UI.

### 0.11 hardware verification

Installed on the confirmed CYD with verified flash hashes. The final app image
is 795,648 bytes; SHA-256:
`287505676fb1eb95a8c459655e7d32699bac3b6b9b274d0b49bb963d49fc2c77`.
Boot passed protocol, Unicode, clock, delivery, management, terminal and draft
codec fixtures. Free heap at Ready was about 194 KB.

The isolated on-device storage test passed read-marker save/replacement, corrupt
marker rejection, draft save/replacement, channel fingerprint matching, direct
draft restoration and corrupt draft rejection. Test files were removed and the
user's RAM state restored. A diagnostic BLE disconnect then automatically
recovered a complete session: ready=1, battery 3999 mV, at least 121 known
contacts, five unread messages and no synthetic draft left behind. No test RF
message was sent. Screen brightness/dimming and wake-button ergonomics await
human interaction; draft persistence is verified through the production storage
and restore helpers without injecting a draft into a user's journal.

### 0.12 hardware verification

Compiled and flashed the application-only image with verified hashes. Boot
passed all existing checks plus scroll-model and timeline-anchor fixtures;
automatic encrypted/bonded reconnection succeeded. Free heap at Ready was
191,320 bytes before opening a chat buffer. The user confirmed scrolling felt
good, but reported flashing. UART diagnosis then showed the full-size buffer
had failed to allocate and the screen was using direct drawing. No test RF message was sent and message journals were preserved.

### 0.12.1 flicker correction

The full-size drawing buffer in 0.12 required a contiguous 38 KB allocation.
Live diagnostics showed **low-memory fallback** despite 120 KB total free heap,
which caused a visible background clear on each frame. The replacement renders
completed 16-bit strips, normally 16 rows at a time, before updating the LCD.
This avoids the large contiguous allocation and keeps exact RGB565 colours.
Allocation can shrink to a one-row strip using 594 bytes. A RAM reference-image
fixture compares pixels between complete and strip-rendered images, including
text crossing strip and viewport boundaries. It performs no display or RF writes.

## Nearby discovery and flood adverts (0.13)

**Net > Discover > Discover nearby > Scan** sends one zero-hop discovery request
and listens for 30 seconds. Only matching replies to that request appear here;
the saved Nodes list remains separate. Up to 32 unique repeaters are shown.
A reply confirms a direct two-way radio exchange at that moment, not onward mesh
coverage or delivery of a channel message. Hidden, forwarding-disabled, older or
rate-limited repeaters may not reply, so an empty result is inconclusive.

Tap a result for request SNR measured by the repeater, reply SNR/RSSI measured by
the Heltec, and reply age. Existing contacts supply names; unknown keys use a
short public-key label. **Contact details** opens an existing record or a new
repeater record for review and explicit Save. Discovery responses are not signed;
this is a reachability indication, not authenticated proof of identity.

**Net > Discover > Flood advert > Send flood advert** announces the Heltec's name
and public key, plus location if its existing advert policy enables location.
Repeaters may relay it under the radio's existing flood scope. The command's OK
means the local radio accepted the advert, not that another radio received it.
It does not solicit responses and is not a repeater search.

Both actions require a tap, send once, and have a 60-second retry cooldown.
Neither runs on boot/reconnect or automatically repeats. Discovery needs
companion protocol v8 or newer. Command failure/timeouts remain unconfirmed.
Read stats is now beside Leave on the Network checks screen.

Protocol sources: [MeshCore control payloads](https://github.com/meshcore-dev/MeshCore/blob/main/docs/payloads.md),
[companion commands and control pushes](https://github.com/meshcore-dev/MeshCore/blob/main/examples/companion_radio/MyMesh.cpp),
and [repeater discovery responder](https://github.com/meshcore-dev/MeshCore/blob/main/examples/simple_repeater/MyMesh.cpp).

Boot fixtures exercise request encoding, signed signal values, current-tag
matching, stale/truncated/routed/non-repeater rejection, self exclusion,
deduplication, capacity, expiry across clock wrap, and separation of local OK
from a received discovery reply. They do not transmit any RF requests.

## Connection dashboard, saved results and coverage book (0.14)

**Net** now opens a connection dashboard. It separately reports the Bluetooth
link, the age of the last received RF packet, and the age/source of the last
matched repeater/path reply or direct ACK. Evidence is scoped to the current
radio session and labelled historical; there is no permanent green "mesh
connected" indicator. The latest nearby search shows its age and outcome.
**Net > Stats** retains local counters, Clock, Auto link and Leave.

Hold a message still for about 0.7 seconds, then release to inspect it. Incoming messages are identified
as received. Older outgoing messages without stored evidence say their result
was not recorded. New send attempts also appear under **Net > Sent**, including
rejected requests that left their draft intact. The latest 80 attempts per radio
retain timestamps, destination labels and the outcome (radio acceptance,
matched repeat/ACK, timeout, rejection, unavailable tracking or interruption).
Results are stored separately from the existing journal using a checksummed,
versioned, atomically replaced file. Positive evidence is never overwritten by a
later local acknowledgement or timeout. Late evidence can upgrade an uncertain
result while its existing three-minute tracking window remains available.
A restart/disconnection ends unfinished tracking and does not resend anything.
Unread markers, channel boundaries and original message journal formats remain
unchanged. Results that cannot be saved are flagged; old messages do not gain
invented delivery results.

Each send freezes its original text/destination before requesting transmission,
so edits or navigation while awaiting a reply cannot attach the result to a
changed draft. A successful local acceptance only clears the matching draft.
The next outgoing timestamp also advances past saved outgoing records, making
new result IDs distinct across restart. An ambiguous send-command timeout now
reconnects before accepting more commands, preventing a late reply from being
assigned to a subsequent send.

**Net > Coverage > New test** opens a place-name editor and a Start test button.
The test sends one standard nearby discovery request and listens for 30 seconds,
using the same firmware checks and 60-second cooldown as ordinary discovery.
Naming a place does not supply GPS coordinates. The latest 12 tests per radio
are retained, with up to 32 unique full public keys and request SNR, response
SNR and response RSSI. Tap a saved test, then a repeater, to compare the readings.
**Again** prepares another test at that place; it does not transmit by itself.

An empty completed test is inconclusive. A rejected command, uncertain write,
Bluetooth interruption or interrupted reboot has its own result. The record is
saved before requesting RF; if initial storage fails no test is transmitted.
Changes during listening are checkpointed, and the final result is saved when
the window closes. Interrupted saved tests are marked incomplete on reload.
Only matching fresh replies from this scan enter its notebook record. Existing
contacts are never counted as replies. Historical identity labels can reflect
the current saved contact name; full public keys identify the measured nodes.

The notebook uses a second checksummed, atomic file. Streaming reads/writes
avoid a large contiguous serialization buffer. No private channel keys enter
either new file; both are local, per radio and excluded from Git. Test and send
timestamps are UTC, with approximate-clock flags retained when applicable.

Boot fixtures cover result codecs, invalid flags/lengths, stable positive
results, late confirmations, restart recovery, signed readings, deduplication
and limits. USB **FIELDTEST** uses a reserved temporary namespace to exercise
production save/load, replacement, empty results, all 80 send records, all
12 x 32 coverage replies, and checksum/truncation rejection. It restores user
RAM and removes fixture files. **FIELD?** prints only counts and storage flags.
Neither command sends RF messages. Live UI feel and positive RF outcomes still
require testing on the actual mesh.

### 0.14 hardware verification

Application-only image installed on the confirmed CYD with a verified flash
hash; image size 846,176 bytes (839,605 bytes linked flash), static RAM 43,564
bytes, boot free heap about 189 KB. SHA-256:
`f3f89081b5830701c055f09dce6883653ddce5812fb18161b2fe01905e767615`.
All boot fixtures passed. Encrypted bonded reconnection succeeded and the radio
loaded 147 contacts. The isolated FIELDTEST passed, including the full on-disk
bounds and corruption rejection; a subsequent diagnostic confirmed no synthetic
results/tests remained in user RAM. User status remained ready=1, no retry,
no draft, and Heltec battery 4069 mV. These installation checks transmitted no
RF test messages. Positive over-the-air discovery, repeat and recipient-ACK
results still depend on coverage and remain unverified by these checks.

## Guided antenna comparison (0.15)

Open **Net > Coverage > Antennas > New**. Name the place and the two antennas,
then select one repeater by its full public key from known repeaters or the
current nearby-discovery results. Creating the comparison transmits nothing.
The tool retains the latest four comparisons per Heltec, independently of the
coverage notebook and message journal.

The sequence is **A first > B > A again**. Open **Run stage**, fit the named
antenna, and press **Run this stage**. Power the Heltec off before swapping
antennas; reconnect before starting the next stage. Keep the same location,
height, orientation, cable arrangement and radio settings. The tool records the
reported frequency/bandwidth/SF/coding-rate profile and TX power; it blocks
starting another stage if those handshake-reported settings have changed.
It does not change any radio settings. External configuration changes while
connected are not continuously polled; reconnect after such changes.

Each stage makes at most three completed, locally accepted discovery attempts,
using the existing 30-second response window and at least 60 seconds between
requests. A full stage normally takes 2 minutes 30 seconds, plus any existing
cooldown or local-command wait. Pressing Run authorizes this finite batch only;
advancing to a different antenna always needs another tap. Other network probes
are blocked during the batch, while message reception continues. Stop or Back
from the stage screen stops future requests. An already transmitted request
cannot be recalled; its remaining discovery window may still need to close.
Bluetooth loss/restart pauses the batch and never resumes RF automatically.
Rejected commands, uncertain writes, cancellations and interrupted windows are
retained as excluded samples, with up to eight total attempts per stage.

Only the first matching fresh reply from the selected full public key counts
for a given request; other repeaters and duplicate replies cannot inflate its
success count. No multi-hop route is substituted. Completed accepted requests
without a matching response count as no reply, not proof of transmission failure.
The Samples screen retains individual outcomes and measurements. SNR reported
by the repeater measures the request; reply SNR and RSSI are measured at Heltec.

Results show reply fractions and median request SNR, reply SNR and reply RSSI
separately for A first, B and A again. A conclusion needs three eligible attempts
per phase and at least two target replies in each phase. The conservative
small-trial rule flags changed conditions if the two A phases have different
reply counts, median SNR differs by over 2 dB in either direction, or median
RSSI differs by over 3 dB. When those checks pass, a stronger-SNR description
requires at least a 2 dB change in both directions against the mean of the two
A medians, consistent RSSI direction and no worse reply count. Otherwise it
reports mixed readings or no clear signal advantage. These are transparent
heuristics, not calibrated gain, statistical significance or an antenna quality
percentage. The tool cannot measure SWR, impedance, reflected power or tuning.

Records are stored in a checksummed, versioned, atomically replaced `.antenna`
file per radio, excluded from Git. In-progress samples load as interrupted;
completed samples remain available after power-off antenna swaps. Storage
failure stops automatic testing; a sample is saved before requesting RF.
Boot fixtures cover codecs, signed values, wrong-target/duplicate rejection,
recovery, medians and all comparison outcomes. USB **ANTTEST** uses a reserved
namespace to check maximum-size persistence, replacement, recovery and damaged
file rejection without RF or user records. **ANT?** prints counts and run/storage
flags only. Synthetic fixtures do not supply real-world antenna conclusions.

### 0.15 hardware verification

The final application-only image was flashed with a verified hash on the
confirmed CYD. Image size: 869,136 bytes; linked flash: 862,561 bytes; static
RAM: 44,140 bytes; boot free heap: 188,880 bytes. SHA-256:
`56c5fc789a6ee3974e6492f13e7e8debd936e489ff52918f5a8fd4c2a9da6cd1`.
All boot fixtures passed, including both stronger-reading directions, mixed and
similar readings, changed-condition and insufficient/incomplete outcomes.
The isolated antenna storage test passed maximum-size persistence, replacement,
interrupted-record recovery, checksum and truncation rejection. Synthetic records
were removed. Encrypted automatic reconnection succeeded, radio contacts loaded,
and the existing coverage record remained present. No antenna-test RF request
was sent by these installation checks. Real antenna comparisons require a
repeatably reachable target and unchanged physical conditions.

A Bluetooth reconnection on the same radio returns to the saved antenna stage
or results view where possible, with testing paused. A full CYD restart requires
opening the saved comparison again. Neither path resumes transmissions by itself.

### 0.15.1 deliberate message-details gesture

Short taps no longer open message details. Hold still for at least 700 ms of
observed touch, then release. Any recognised scroll, a six-pixel excursion in
either direction, invalid coordinates or a gap of over 100 ms between samples
permanently cancels details for that gesture. Returning to the starting point
does not re-enable it. Existing scroll filtering, release debounce and buffered
rendering remain unchanged. Boot fixtures cover short touches, stationary holds,
scroll cancellation, horizontal motion, gaps and clock wrap. Net > Sent remains
available for reviewing send results without a hold.

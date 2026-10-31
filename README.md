# Mesh Pilot

An independent, open-source, phone-free MeshCore terminal for ESP32 touchscreens.

Current development version: **0.11**.
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
flow, each message beginning on the next line. Eleven lines fit on a page.
Complete messages stay together when possible; very long messages continue.
**Older/Newer** browses history. Received UTF-8 is decoded before rendering;
known emoji become readable labels such as [cat] and [hand], accents are
transliterated and unsupported characters receive [U+XXXX] labels. Raw journal
bytes remain unchanged. Names in channel messages are sender-supplied text.

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
  the latest 80 messages kept in RAM. Incoming messages preserve an older page
  and a compose draft instead of moving the reader to a different page.
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

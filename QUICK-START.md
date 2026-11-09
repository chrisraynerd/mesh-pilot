# Mesh Pilot

Keep the Heltec powered and disconnect the phone app. Mesh Pilot remembers its
Bluetooth pairing and reconnects automatically while **Auto link** is on.

- **Channel > Write** sends a group message.
- **Contacts > person > Message > Write** opens a direct conversation.
- **Manage** adds or edits hashtag/private channels.
- **Net > Nodes** lists people, repeaters and other discovered nodes.
- **Setup** shows Heltec battery voltage and storage, brightness, dimming and
  automatic-link controls. **Clock** sets UTC time offline.

In a conversation, hold the stylus on the text and drag **down for older**
messages or **up toward newer** messages. **Latest v** jumps to the bottom.
New messages follow automatically only while you are at the bottom; otherwise
**New messages v** lets you jump down when ready. Write and Back stay fixed.
There is no need to press Older/Newer buttons.

Numbers beside channels and people indicate unread messages. While the screen
is dimmed, new messages stay unread. The first tap wakes the screen and does
not press a button. Bluetooth and reception continue while dimmed.

An unsent draft saves locally after about two seconds without editing. It can
return after a restart or dropped connection for review. **You must tap Send**;
Mesh Pilot never sends or retries a draft automatically. Only one active draft
is kept per radio. Switching to another recipient clears the current draft,
as in earlier versions. A channel draft is restored only if its key still matches.

Send feedback appears temporarily along the bottom:

- **Radio accepted**: your Heltec accepted the message command.
- **Repeat heard**: a relayed copy was heard; individual delivery is unconfirmed.
- **Delivered** on a direct message: a matching recipient-radio acknowledgement.
- **No repeat / No ACK**: no confirmation; this does not prove delivery failed.

If you see an uncertain send, check before retrying. A saved contact or Bluetooth
link alone does not establish a working path through the mesh.

For contact/channel entry details, see [the contact and channel guide](QUICK-START-0.9.md).

To check nearby repeaters, open **Net > Discover > Discover nearby > Scan**.
Wait 30 seconds. A listed reply confirms a direct two-way link at that moment.
Tap it for signal details. No replies means unconfirmed; hidden or older
repeaters may not answer. This does not test onward coverage through the mesh.

**Net > Discover > Flood advert > Send flood advert** announces your Heltec's
identity so others can learn about you. Repeaters may pass it onwards. It does
not ask for replies, so it cannot confirm coverage. Each action sends once;
wait one minute before retrying the same action.

**Net** now separates Bluetooth connection, heard radio traffic and confirmed
radio replies, with the age of each observation. **Stats** opens the previous
counter/settings page. **Sent** keeps recent send outcomes after the temporary
notice disappears. To inspect a message, hold it still for about 0.7 seconds, then release.
Short taps do nothing; dragging scrolls.
Older messages may say their result was not recorded.

To compare locations, choose **Net > Coverage > New test**, tap **Name this
place**, type a short label and press **Done**, then **Start test**. Wait 30
seconds. The notebook keeps your latest 12 tests, including inconclusive tests
with no replies. Tap a test and then a repeater to inspect both signal readings.
**Again** prepares a new test at the same place. Tests send only when started
and respect the one-minute retry pause. An interrupted test is marked incomplete.

For antenna comparisons, open **Net > Coverage > Antennas > New**. Set a place,
names for A and B, and one target repeater. Create the comparison, then choose
**Run stage > Run this stage** with antenna A fitted. Each stage sends three
requests at least a minute apart; allow about 2 minutes 30 seconds.

Use **Next stage** to prepare B, then A again. Always power the Heltec OFF before
swapping antennas, reconnect, and keep its position, height and orientation the
same. Results and Samples keep the measurements. An inconclusive result means
there is not enough consistent evidence; this is not an SWR or antenna-gain meter.
Stop or Back stops further automatic tests. A restart never resumes a batch.

### Clearer messages and channel bots

Messages now appear in larger-text bubbles: received messages on the left and
yours on the right. Time and outgoing send status sit at the top right beside
the sender, leaving more room for the message. Common emojis have small coloured symbols. Hold a bubble
still and release to view details; dragging still scrolls.

For automatic test replies, open **Setup > Bots**, choose a channel, check the
**Location** label and tap **Bot OFF** to enable it. Do this separately for each
channel you want. Anyone sending exactly **test**, **TEST**, **Test** or another
mix of capitals in that channel can trigger a reception report. Replies give
your location label, received hop count and last-hop signal-to-noise ratio when
available. No reply is sent for old messages loaded after reconnecting.

The bot waits a few seconds and is limited to six reply attempts per hour across
all channels, one per minute and one per sender name every five minutes. It does
not retry. See **Net > Sent** for outcomes. Your draft is kept separate. Turn it
off on the same channel screen; full screen-device restarts turn all bots off.
The location labels are remembered. A bot reply confirms this device received
the trigger, not that every node or person received it.

### Your own bot keywords and replies

Open **Setup > Bots > channel > Commands**. Choose **Custom reply 1** or **2**,
set its **Keyword**, tap **Edit reply** and write the answer, then turn the
command **ON**. For example, `meeting` can reply with your agreed meeting place.
Back on the channel bot screen, turn that channel's bot **ON** too. Configuration
changes stop the channel bot so you can review before enabling it.

The reception-report keyword is editable, while `help` lists enabled commands.
Keywords match a whole message in any mix of capitals; `meeting please` does not
match `meeting`. Existing cooldowns also apply to these commands. Wi-Fi/weather
is planned as the next stage and is not part of this offline update.

### Internet weather

1. Open **Setup > Wi-Fi**, enter your **2.4 GHz** network name and password, then
   **Save + connect**. Passwords stay hidden; **123**, then **More**, offers extra
   symbols. Wait for “Wi-Fi connected”.
2. Tap **Check weather** for a local Ramsbottom/default-location lookup. This
   check sends no mesh message.
3. Open **Setup > Bots > channel > Commands > Weather** and enable weather there.
   Return to that channel's bot setup and enable the bot too.
4. Another radio can send `wx Ramsbottom`, `weather Ramsbottom`, or just `wx`
   for that channel's saved location. The same sender cooldown is five minutes.

Weather supports UK places, uses Celsius/mph and shows UTC source time. It is
Open-Meteo model data. Cached replies say **CACHED** with their age; otherwise an
unavailable message explains why a lookup failed. Weather needs Wi-Fi internet;
normal mesh messages and offline commands continue without it. **Turn Wi-Fi off**
keeps credentials; **Forget** removes them. Full CYD restarts turn channel bots off.

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

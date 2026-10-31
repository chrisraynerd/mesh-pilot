# Mesh Pilot

Keep the Heltec powered and disconnect the phone app. Mesh Pilot remembers its
Bluetooth pairing and reconnects automatically while **Auto link** is on.

- **Channel > Write** sends a group message.
- **Contacts > person > Message > Write** opens a direct conversation.
- **Manage** adds or edits hashtag/private channels.
- **Net > Nodes** lists people, repeaters and other discovered nodes.
- **Setup** shows Heltec battery voltage and storage, brightness, dimming and
  automatic-link controls. **Clock** sets UTC time offline.

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

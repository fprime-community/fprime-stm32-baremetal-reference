# RadioTransport foundation

This library is a work in progress, not an operational radio gateway. The
STM32 flight deployment uses its telemetry queue and fragment encoder. The
SAMD gateway has an experimental telemetry-only path; command uplink is not
wired in either deployment.

Version 2 of the experimental wire format has a 16-byte header: magic `RF`,
version, kind, 32-bit session, 32-bit frame ID, 16-bit frame size, and 16-bit
offset, all multi-byte fields in network byte order. Data packets contain up
to 239 bytes; a 1,024-byte frame therefore uses five LoRa packets. A command
acknowledgement has kind 3, a zero size and offset, and no payload.
Version 2 is not compatible with the earlier 12-byte experimental format.

`TelemetryQueue` copies at most two frames and counts rejected frames; it is
intended to keep radio congestion independent of the UART path.
`TcStream` extracts complete CCSDS TC frames from arbitrary UART byte chunks
and checks the spacecraft ID, length and FECF. The caller must retain any
bytes not consumed while a complete frame awaits release, and must reset an
abandoned partial frame on a suitable UART inter-frame timeout.
`CommandSender` and `CommandReceiver` start disarmed. An operator must first
reconcile any command with an uncertain outcome and explicitly arm both with
the same fresh nonzero session. The operator must maintain a durable record
outside these volatile classes and assign a strictly increasing session value
that has never been used before, even after either board restarts. Session
values must not wrap; when exhausted, this protocol needs replacement.
Each side rejects reuse of a session during its current boot, but cannot
verify that a session was not used before a reset. A rebooted receiver remains
disarmed and rejects commands until explicitly armed; a rebooted sender
remains disarmed and cannot initiate commands.

The sender accepts strictly increasing command IDs within a session without
wrapping. It sends one command at a time, retries the complete frame after a
failed radio send or an acknowledgement timeout, and disarms when its attempt
limit is exhausted. An exhausted command has an **uncertain** outcome; do not
automatically re-arm or resend it. A new session requires operator recovery.
Call `packetResult` only after the radio has returned the packet and reported
its transmit result. Disarming the sender is rejected while a packet is owned
by the radio; disarming a sending or waiting command records `FAILED`, since
the command's outcome may be uncertain.
The receiver rejects packets outside its armed session, re-acknowledges
duplicates, and only acknowledges after its caller confirms local acceptance.
The caller must call `finish(false)` only when it knows no delivery occurred.

Deployment integration must serialize calls, provide a trustworthy manual
arming mechanism on both boards, stop the sender when a peer reset is detected,
retain pending radio buffers, and arrange scheduling and bounded queues. The
receiver must retain a pending acknowledgement until its radio send succeeds.
These classes alone cannot detect a peer reset or enforce the operator's
session record. The deployment must not silently re-arm after a reset.
Acknowledgement means the chosen local acceptance boundary was reached, not
that a command executed. Volatile duplicate suppression cannot guarantee
exactly-once execution across resets.

The SAMD project has a deployment-local copy of these portable sources under
`RadioGateway/RadioTransport`. Changes to the protocol must be kept in sync
between the two projects until a shared distribution mechanism is chosen.

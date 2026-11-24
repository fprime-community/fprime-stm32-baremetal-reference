# RadioTransport foundation

This library is a work in progress, not an operational radio gateway. Neither
deployment is wired to it yet.

Version 2 of the experimental wire format has a 16-byte header: magic `RF`,
version, kind, 32-bit session, 32-bit frame ID, 16-bit frame size, and 16-bit
offset, all multi-byte fields in network byte order. Data packets contain up
to 239 bytes; a 1,024-byte frame therefore uses five LoRa packets. A command
acknowledgement has kind 3, a zero size and offset, and no payload.
Version 2 is not compatible with the earlier 12-byte experimental format.

`TelemetryQueue` copies at most two frames and counts rejected frames; it is
intended to keep radio congestion independent of the UART path.
`CommandSender` sends one command at a time, retries a complete frame after
a failed radio send or an acknowledgement timeout, and stops after the
configured number of attempts. Call `packetResult` only after the radio has
returned the packet and reported its transmit result. `CommandReceiver`
acknowledges only after its caller confirms acceptance by the local uplink,
and re-acknowledges duplicates rather than delivering them again.

The deployment must serialize access to these classes, generate a session ID
that does not repeat for a new sender session, allocate unique command IDs
within that session, and arrange scheduling and bounded queues. The sender
must wait for a packet's return before reusing its buffer. The receiver must
retain a pending acknowledgement until its radio send succeeds. Radio
transmit completion is not command execution confirmation. This protocol
does not provide durable duplicate suppression: resets, session reuse, stale
sessions, and a reset between local delivery and recording the delivery may
cause an uncertain outcome or duplicate command. Session generation and
restart policy are not implemented yet; exactly-once execution across resets
is not guaranteed.

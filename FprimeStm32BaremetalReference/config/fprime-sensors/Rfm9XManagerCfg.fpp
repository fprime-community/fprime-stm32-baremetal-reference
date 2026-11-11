module Rfm9X {
    module ManagerCfg {
        @ Bound on time spent waiting for TxDone after requesting TX mode, in
        @ milliseconds. LoRa time-on-air grows sharply with spreading factor
        @ and shrinks with bandwidth (datasheet section 4.1.1); this default
        @ comfortably covers this project's SF7/BW125kHz profile for a
        @ 255-byte payload.
        constant TX_DONE_TIMEOUT_MS = 5000

        @ After a completed uplink packet, defer downlink this many
        @ milliseconds -- an RX-priority lease for the half-duplex link.
        constant RX_TX_HOLDOFF_MS = 500

        @ Quiet time between consecutive local transmits, in milliseconds.
        constant TX_TX_HOLDOFF_MS = 500

        @ Upper bound on time a pending downlink may remain blocked by the
        @ RX-priority lease, in milliseconds.
        constant RX_HOLDOFF_STARVATION_MS = 2 * RX_TX_HOLDOFF_MS

        @ This deployment wires resetGpio to a dedicated NRESET line (PK2),
        @ not a power-enable/load-switch GPIO, so these keep the SX127x
        @ datasheet-derived defaults (section 7.2.2). A deployment using a
        @ power-enable GPIO instead should override RESET_SETTLE_MS to its
        @ own worst-case power-on time.
        constant RESET_HOLD_MS = 1
        constant RESET_SETTLE_MS = 6

        @ 255 bytes keeps full single-packet headroom; this project's GDS/
        @ ground-station traffic isn't bounded to a smaller fixed size.
        constant MAX_PAYLOAD_LENGTH = 255

        @ Rfm9X.comStatusOut is unwired in this topology (no ComQueue retry
        @ path), so Rfm9XManager::usesLocalTxHold() is active and this queue
        @ genuinely holds frames -- keep the library default rather than
        @ shrinking it.
        constant PENDING_TX_DEPTH = 8
    }
}

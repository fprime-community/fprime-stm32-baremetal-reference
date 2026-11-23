module ReferenceDeployment {
    @ Default when the parameter database has no radio frequency.
    constant RADIO_DEFAULT_FREQUENCY_HZ = 915000000

    @ Receives radio test replies and supplies the deployment's radio frequency default.
    passive component RadioTest {
        sync input port receive: Svc.ComDataWithContext
        output port returnBuffer: Svc.ComDataWithContext
        output port transmit: Svc.ComDataWithContext
        sync input port sendReturn: Svc.ComDataWithContext

        sync command SEND_STRING(payload: string)
        command reg port CmdReg
        command recv port CmdDisp
        command resp port CmdStatus

        sync input port getParameter: Fw.PrmGet
        output port getStoredParameter: Fw.PrmGet

        time get port timeCaller
        event port logOut
        text event port logTextOut

        event TestPayloadQueued(payloadSize: FwSizeType) severity activity high \
            format "Queued {} byte command payload for transmission"
        event TestSendRejected severity warning high \
            format "Test message rejected"

        @ A printable ASCII segment of a received radio packet.
        event ReplySegment(offset: U32, total: U32, payload: string size 32) \
            severity activity high \
            format "Radio reply offset {} of {}: {}"
    }
}

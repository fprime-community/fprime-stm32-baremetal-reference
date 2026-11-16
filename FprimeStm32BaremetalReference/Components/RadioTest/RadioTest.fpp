module ReferenceDeployment {
    @ Default when the parameter database has no radio frequency.
    constant RADIO_DEFAULT_FREQUENCY_HZ = 915000000

    @ Receives radio test replies and supplies the deployment's radio frequency default.
    passive component RadioTest {
        sync input port receive: Svc.ComDataWithContext
        output port returnBuffer: Svc.ComDataWithContext

        sync input port getParameter: Fw.PrmGet
        output port getStoredParameter: Fw.PrmGet

        time get port timeCaller
        event port logOut
        text event port logTextOut

        @ A printable ASCII segment of a received radio packet.
        event ReplySegment(offset: U32, total: U32, payload: string size 32) \
            severity activity high \
            format "Radio reply offset {} of {}: {}"
    }
}

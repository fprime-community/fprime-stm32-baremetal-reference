module Deployments {
    @ Copies framed UART telemetry to a bounded, best-effort radio queue.
    passive component RadioFlight {
        sync input port frameIn: Svc.ComDataWithContext
        output port frameReturn: Svc.ComDataWithContext
        output port uartOut: Svc.ComDataWithContext
        sync input port uartReturn: Svc.ComDataWithContext
        sync input port uartStatus: Fw.SuccessCondition
        output port frameStatus: Fw.SuccessCondition

        output port radioOut: Svc.ComDataWithContext
        sync input port radioReturn: Svc.ComDataWithContext
        sync input port radioStatus: Fw.SuccessCondition
        sync input port radioReceive: Svc.ComDataWithContext
        output port radioReceiveReturn: Svc.ComDataWithContext
        sync input port run: Svc.Sched
        sync input port getParameter: Fw.PrmGet
        output port getStoredParameter: Fw.PrmGet

        time get port timeCaller
        telemetry port tlmOut
        event port logOut
        text event port logTextOut

        telemetry RadioFramesDropped: U32
        telemetry RadioPacketsReceivedUnsupported: U32
        telemetry RadioPacketsFailed: U32
    }
}

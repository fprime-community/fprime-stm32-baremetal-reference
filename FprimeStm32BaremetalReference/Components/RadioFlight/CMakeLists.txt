register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/RadioFlight.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/RadioFlight.cpp"
    DEPENDS
        FprimeStm32BaremetalReference_Components_RadioTransport
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/RadioFlight.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/RadioFlightTest.cpp"
    DEPENDS
        FprimeStm32BaremetalReference_Components_RadioTransport
    UT_AUTO_HELPERS
)

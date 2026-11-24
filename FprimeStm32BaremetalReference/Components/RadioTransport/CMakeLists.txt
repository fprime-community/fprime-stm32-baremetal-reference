register_fprime_library(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/RadioFragment.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ReliableCommand.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/TelemetryQueue.cpp"
)

register_fprime_ut(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/RadioFragmentTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ReliableCommandTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/TelemetryQueueTest.cpp"
)

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/RadioTest.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/RadioTest.cpp"
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/RadioTest.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/RadioTestTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/RadioTestTester.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/RadioRoundTripTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../../lib/fprime-sensors/fprime-sensors/Rfm9X/Components/Rfm9XSim/Rfm9XSimModel.cpp"
    DEPENDS
        fprime-sensors_Rfm9X_Components_Rfm9XManager
    UT_AUTO_HELPERS
)

if(BUILD_TESTING)
    set(RADIO_MANAGER_TEST_DIR "${CMAKE_BINARY_DIR}/fprime-sensors/Rfm9X/Components/Rfm9XManager")
    set(RADIO_MANAGER_TEST_BASES
        "${RADIO_MANAGER_TEST_DIR}/Rfm9XManagerTesterBase.cpp"
        "${RADIO_MANAGER_TEST_DIR}/Rfm9XManagerGTestBase.cpp"
    )
    set_source_files_properties(${RADIO_MANAGER_TEST_BASES} PROPERTIES GENERATED TRUE)
    target_sources(FprimeStm32BaremetalReference_Components_RadioTest_ut_exe PRIVATE ${RADIO_MANAGER_TEST_BASES})
    add_dependencies(FprimeStm32BaremetalReference_Components_RadioTest_ut_exe fprime-sensors_Rfm9X_Components_Rfm9XManager_ut_exe)
    target_include_directories(FprimeStm32BaremetalReference_Components_RadioTest_ut_exe PRIVATE "${RADIO_MANAGER_TEST_DIR}")
endif()

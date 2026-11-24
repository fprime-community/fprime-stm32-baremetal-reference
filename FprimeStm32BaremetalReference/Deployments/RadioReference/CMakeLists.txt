restrict_platforms(stm32h7)

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top/")

register_fprime_deployment(
    RadioReference
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
        "${FPRIME_STM32_STARTUP_SOURCE}"
        "${FPRIME_STM32_IT_SOURCE}"
    DEPENDS
        RadioReference_Top
        FprimeStm32
        FprimeStm32Config
        FprimeStm32Allocator
        Os_Baremetal_OverrideNewDelete
)

target_include_directories(RadioReference PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    "${CMAKE_CURRENT_BINARY_DIR}/../.."
)

target_link_options(RadioReference PRIVATE
    "-T${FPRIME_STM32_LINKER_SCRIPT}"
    "-Wl,--gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/RadioReference.map"
    "-Wl,--undefined=_Znwj"
    "-Wl,--undefined=_Znaj"
    "-Wl,--undefined=_Znwjl"
    "-Wl,--undefined=_Znajl"
    "-Wl,--undefined=_ZnwjRKSt9nothrow_t"
    "-Wl,--undefined=_ZnajRKSt9nothrow_t"
    "-Wl,--undefined=_ZdlPv"
    "-Wl,--undefined=_ZdaPv"
    "-Wl,--undefined=_ZdlPvl"
    "-Wl,--undefined=_ZdaPvl"
    "-Wl,--undefined=_ZdlPvj"
    "-Wl,--undefined=_ZdaPvj"
    "--specs=nosys.specs"
)

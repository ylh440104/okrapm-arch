add_executable(lunar
    src/main.cpp
)
if(OKRAPM_WITH_CRYPTO)
    target_sources(lunar PRIVATE src/key_cli.cpp)
endif()

target_link_libraries(lunar
    PRIVATE
        okrapmlib
)

target_compile_features(lunar PRIVATE cxx_std_17)

install(TARGETS lunar DESTINATION bin)

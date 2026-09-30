option(OKRAPM_WITH_CRYPTO "Enable Ed25519 signing" ON)

if(OKRAPM_WITH_CRYPTO)
    find_package(OpenSSL REQUIRED)
endif()

add_library(okrapmlib
    src/version.cpp
    src/platform.cpp
    src/crypto.cpp
    src/file_index.cpp
    src/object.cpp
    src/object_ref.cpp
    src/operation.cpp
    src/transaction.cpp
    src/repository.cpp
    src/resolver.cpp
    src/system_store.cpp
    src/snapshot.cpp
    src/extension_api.cpp
    src/artifact_engine.cpp
    src/network_downloader.cpp
    src/pipeline_engine.cpp
    src/lunar_core.cpp
)

target_include_directories(okrapmlib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(OKRAPM_WITH_CRYPTO)
    target_compile_definitions(okrapmlib PUBLIC OKRAPM_WITH_CRYPTO=1)
    target_link_libraries(okrapmlib
        PUBLIC
            ${CMAKE_DL_LIBS}
            OpenSSL::Crypto
    )
else()
    target_link_libraries(okrapmlib
        PUBLIC
            ${CMAKE_DL_LIBS}
    )
    target_compile_definitions(okrapmlib PUBLIC OKRAPM_WITH_CRYPTO=0)
endif()

target_compile_features(okrapmlib PUBLIC cxx_std_17)

if(INCLUDE_WASM_SPEC_TESTS)
    if (CMAKE_PROJECT_NAME STREQUAL "SerenityOS")
        set(SOURCE_DIR "${SerenityOS_SOURCE_DIR}")
    else()
        set(SOURCE_DIR "${SERENITY_PROJECT_ROOT}")
    endif()
    set(WASM_SPEC_TEST_GZ_URL https://github.com/WebAssembly/testsuite/archive/refs/heads/main.tar.gz)
    set(WASM_SPEC_TEST_GZ_PATH ${CMAKE_BINARY_DIR}/wasm-spec-testsuite.tar.gz)
    set(WASM_SPEC_TEST_PATH ${SOURCE_DIR}/Userland/Libraries/LibWasm/Tests/Fixtures/SpecTests)

    if(NOT EXISTS ${WASM_SPEC_TEST_GZ_PATH})
        message(STATUS "Downloading the WebAssembly testsuite from ${WASM_SPEC_TEST_GZ_URL}...")
        file(DOWNLOAD ${WASM_SPEC_TEST_GZ_URL} ${WASM_SPEC_TEST_GZ_PATH} INACTIVITY_TIMEOUT 10)
    endif()

    set(SKIP_PRETTIER false)
    if (WASM_SPEC_TEST_SKIP_FORMATTING)
        set(SKIP_PRETTIER true)
    endif()

    if(EXISTS ${WASM_SPEC_TEST_GZ_PATH} AND NOT EXISTS ${WASM_SPEC_TEST_PATH})
        message(STATUS "Extracting the WebAssembly testsuite from ${WASM_SPEC_TEST_GZ_PATH}...")
        file(MAKE_DIRECTORY ${WASM_SPEC_TEST_PATH})
        if (CMAKE_VERSION VERSION_LESS 3.18.0)
            execute_process(COMMAND "${TAR_TOOL}" -xzf ${WASM_SPEC_TEST_GZ_PATH})
        else()
            file(ARCHIVE_EXTRACT INPUT ${WASM_SPEC_TEST_GZ_PATH} )
        endif()
        file(GLOB WASM_TESTS "${CMAKE_BINARY_DIR}/testsuite-main/*.wast")
        foreach(PATH ${WASM_TESTS})
            get_filename_component(NAME ${PATH} NAME_WLE)
            message(STATUS "Generating test cases for WebAssembly test ${NAME}...")
            # FIXME: GH 8668. loop_0.wasm causes CI timeout
            if (NAME STREQUAL "loop")
                message(STATUS "Skipping generation of ${NAME} test due to timeouts")
                continue()
            endif()
            execute_process(
                COMMAND env SKIP_PRETTIER=${SKIP_PRETTIER} bash ${SOURCE_DIR}/Meta/generate-libwasm-spec-test.sh "${PATH}" "${SOURCE_DIR}/Userland/Libraries/LibWasm/Tests/Spec" "${NAME}" "${WASM_SPEC_TEST_PATH}")
        endforeach()
        file(REMOVE testsuite-main)
    endif()
endif()

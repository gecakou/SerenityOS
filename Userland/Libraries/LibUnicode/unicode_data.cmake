option(ENABLE_UNICODE_DATABASE_DOWNLOAD "Enable download of Unicode UCD files at build time" ON)

set(UNICODE_DATA_URL https://www.unicode.org/Public/13.0.0/ucd/UnicodeData.txt)
set(UNICODE_DATA_PATH ${CMAKE_BINARY_DIR}/UCD/UnicodeData.txt)

if (ENABLE_UNICODE_DATABASE_DOWNLOAD)
    if (NOT EXISTS ${UNICODE_DATA_PATH})
        message(STATUS "Downloading UCD UnicodeData.txt from ${UNICODE_DATA_URL}...")
        file(DOWNLOAD ${UNICODE_DATA_URL} ${UNICODE_DATA_PATH} INACTIVITY_TIMEOUT 10)
    endif()

    set(UNICODE_GENERATOR CodeGenerators/GenerateUnicodeData)
    set(UNICODE_DATA_HEADER UnicodeData.h)
    set(UNICODE_DATA_IMPLEMENTATION UnicodeData.cpp)

    if (CMAKE_SOURCE_DIR MATCHES ".*/Lagom") # Lagom-only build.
        set(UNICODE_GENERATOR LibUnicode/CodeGenerators/GenerateUnicodeData)
        set(UNICODE_DATA_HEADER LibUnicode/UnicodeData.h)
        set(UNICODE_DATA_IMPLEMENTATION LibUnicode/UnicodeData.cpp)
    elseif (CMAKE_CURRENT_BINARY_DIR MATCHES ".*/Lagom") # Lagom build within the main SerenityOS build.
        set(UNICODE_GENERATOR ../../Userland/Libraries/LibUnicode/CodeGenerators/GenerateUnicodeData)
        set(UNICODE_DATA_HEADER LibUnicode/UnicodeData.h)
        set(UNICODE_DATA_IMPLEMENTATION LibUnicode/UnicodeData.cpp)
    endif()

    add_custom_command(
        OUTPUT ${UNICODE_DATA_HEADER}
        COMMAND ${write_if_different} ${UNICODE_DATA_HEADER} ${UNICODE_GENERATOR} -h -u ${UNICODE_DATA_PATH}
        VERBATIM
        DEPENDS GenerateUnicodeData
        MAIN_DEPENDENCY ${UNICODE_DATA_PATH}
    )

    add_custom_command(
        OUTPUT ${UNICODE_DATA_IMPLEMENTATION}
        COMMAND ${write_if_different} ${UNICODE_DATA_IMPLEMENTATION} ${UNICODE_GENERATOR} -c -u ${UNICODE_DATA_PATH}
        VERBATIM
        DEPENDS GenerateUnicodeData
        MAIN_DEPENDENCY ${UNICODE_DATA_PATH}
    )

    set(UNICODE_DATA_SOURCES ${UNICODE_DATA_HEADER} ${UNICODE_DATA_IMPLEMENTATION})
    add_compile_definitions(ENABLE_UNICODE_DATA=1)
else()
    add_compile_definitions(ENABLE_UNICODE_DATA=0)
endif()

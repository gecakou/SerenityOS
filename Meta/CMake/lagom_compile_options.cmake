include(${CMAKE_CURRENT_LIST_DIR}/common_compile_options.cmake)

add_compile_options(-Wno-implicit-const-int-float-conversion)
add_compile_options(-Wno-literal-suffix)
add_compile_options(-Wno-maybe-uninitialized)
add_compile_options(-Wno-unknown-warning-option)
add_compile_options(-fsigned-char)
add_compile_options(-fno-exceptions)
add_compile_options(-fdiagnostics-color=always)
add_compile_options(-fPIC -g)
add_compile_options(-O2)
if (NOT ENABLE_FUZZERS)
    add_compile_options(-fno-semantic-interposition)
endif()

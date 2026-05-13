function(scaffold_configure_tooling)
    if(SCAFFOLD_ENABLE_CLANG_TIDY)
        find_program(SCAFFOLD_CLANG_TIDY_EXE NAMES clang-tidy)
        if(NOT SCAFFOLD_CLANG_TIDY_EXE)
            message(FATAL_ERROR "clang-tidy was requested with SCAFFOLD_ENABLE_CLANG_TIDY=ON, but no clang-tidy executable was found in PATH.")
        endif()

        set(SCAFFOLD_CLANG_TIDY_EXE "${SCAFFOLD_CLANG_TIDY_EXE}" CACHE INTERNAL "clang-tidy executable")
    endif()

    if((SCAFFOLD_ENABLE_ASAN OR SCAFFOLD_ENABLE_UBSAN) AND MSVC)
        message(FATAL_ERROR "SCAFFOLD_ENABLE_ASAN and SCAFFOLD_ENABLE_UBSAN require a Clang or GCC preset. The MSVC generator is not wired for these sanitizers.")
    endif()

    if(SCAFFOLD_ENABLE_MSVC_ANALYSIS AND NOT MSVC)
        message(WARNING "SCAFFOLD_ENABLE_MSVC_ANALYSIS has no effect on non-MSVC toolchains.")
    endif()
endfunction()

function(scaffold_apply_common_options target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
        if(SCAFFOLD_ENABLE_MSVC_ANALYSIS)
            target_compile_options(${target_name} PRIVATE /analyze)
        endif()
        if(SCAFFOLD_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
        )

        if(SCAFFOLD_ENABLE_ASAN OR SCAFFOLD_ENABLE_UBSAN)
            set(sanitizers "")
            if(SCAFFOLD_ENABLE_ASAN)
                list(APPEND sanitizers address)
            endif()
            if(SCAFFOLD_ENABLE_UBSAN)
                list(APPEND sanitizers undefined)
            endif()

            string(JOIN "," sanitizer_list ${sanitizers})
            target_compile_options(${target_name} PRIVATE
                -fsanitize=${sanitizer_list}
                -fno-omit-frame-pointer
            )
            target_link_options(${target_name} PRIVATE -fsanitize=${sanitizer_list})
        endif()

        if(SCAFFOLD_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()

    if(SCAFFOLD_ENABLE_CLANG_TIDY)
        set_target_properties(${target_name} PROPERTIES
            CXX_CLANG_TIDY "${SCAFFOLD_CLANG_TIDY_EXE};--use-color"
        )
    endif()
endfunction()


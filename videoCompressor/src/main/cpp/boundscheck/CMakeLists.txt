# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)

set(can_use_assembler TRUE)
enable_language(ASM)
IF("${OHOS_ARCH}" STREQUAL "arm64-v8a")
 SET(ASM_OPTIONS "-x assembler-with-cpp")
 SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS} -march=armv8+crypto -D__OHOS__")
ENDIF()

project(boundscheck)

set(TAGET_BOUNDSCHECK_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party_bounds_checking_function/src)

add_library(boundscheck
            STATIC
            ${TAGET_BOUNDSCHECK_SRC_PATH}/memcpy_s.c
            ${TAGET_BOUNDSCHECK_SRC_PATH}/memset_s.c
            ${TAGET_BOUNDSCHECK_SRC_PATH}/securecutil.c)

include_directories(${TAGET_BOUNDSCHECK_SRC_PATH}
                    ${CMAKE_CURRENT_SOURCE_DIR}/third_party_bounds_checking_function/include
                    )

target_link_libraries(boundscheck)

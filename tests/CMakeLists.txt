cmake_minimum_required (VERSION 2.8)

project (wakaamaunittests)

include(${CMAKE_CURRENT_LIST_DIR}/../core/wakaama.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../examples/shared/shared.cmake)

add_definitions(-DLWM2M_CLIENT_MODE -DLWM2M_SUPPORT_JSON -DWITH_LOGS -DLWM2M_WITH_LOGS -DSIERRA -DCOAP_BLOCK1_SIZE=4096)
add_definitions(${SHARED_DEFINITIONS} ${WAKAAMA_DEFINITIONS})
add_definitions(-w)

include_directories (${WAKAAMA_SOURCES_DIR}
                     ${SHARED_INCLUDE_DIRS}
                     ${CUNIT_INSTALL}/include
                     ${CUNIT_INSTALL}/include/CUnit
                     ${CMAKE_CURRENT_LIST_DIR}/../../../include/platform-specific/linux
                     ${CMAKE_CURRENT_LIST_DIR}/../../../include
                     ${CMAKE_CURRENT_LIST_DIR}/../../../include/lwm2mcore
                     ${CMAKE_CURRENT_LIST_DIR}/../../../sessionManager
                     ${CMAKE_CURRENT_LIST_DIR}/../../../objectManager
                     ${CMAKE_CURRENT_LIST_DIR}/../../tinydtls
                     ${CMAKE_CURRENT_LIST_DIR}/../../../tests
                     ${CMAKE_CURRENT_LIST_DIR}/../core/er-coap-13)


SET(SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/block1tests.c
    ${CMAKE_CURRENT_LIST_DIR}/block1streamtests.c
    ${CMAKE_CURRENT_LIST_DIR}/block2streamtests.c
    ${CMAKE_CURRENT_LIST_DIR}/convert_numbers_test.c
    ${CMAKE_CURRENT_LIST_DIR}/tlv_json_lwm2m_data_test.c
    ${CMAKE_CURRENT_LIST_DIR}/tlvtests.c
    ${CMAKE_CURRENT_LIST_DIR}/unittests.c
    ${CMAKE_CURRENT_LIST_DIR}/uritests.c
    ${CMAKE_CURRENT_LIST_DIR}/stub.c
    ${CMAKE_CURRENT_LIST_DIR}/../../../objectManager/lwm2mcoreCoapHandlers.c
    )

link_directories(${CUNIT_INSTALL}/lib)
add_executable(${PROJECT_NAME} ${SOURCES} ${WAKAAMA_SOURCES} ${SHARED_SOURCES})
target_link_libraries(wakaamaunittests -lcunit)

# Enable CMake Test Framework (CTest) which make testing available by
# a "test" target. For "make" this is "make test"
enable_testing()

add_test(wakaamaunittests ${EXECUTABLE_OUTPUT_PATH}/wakaamaunittests)

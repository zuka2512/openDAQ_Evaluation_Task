set(MODULE_NAME example_module)
set(TEST_APP test_${MODULE_NAME})

set(TEST_SOURCES test_example_module.cpp
                 test_app.cpp
)

add_executable(${TEST_APP} ${TEST_SOURCES}
)

target_link_libraries(${TEST_APP} PRIVATE daq::test_utils
                                          ${SDK_TARGET_NAMESPACE}::${MODULE_NAME}
)

add_test(NAME ${TEST_APP}
         COMMAND $<TARGET_FILE_NAME:${TEST_APP}>
         WORKING_DIRECTORY bin
)

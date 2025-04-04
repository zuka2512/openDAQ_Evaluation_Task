add_executable(fb_application_example main.cpp)

target_include_directories(fb_application_example PRIVATE
    ${CMAKE_SOURCE_DIR}/example_module/include
    ${CMAKE_BINARY_DIR}/example_module/include
)

target_link_libraries(fb_application_example PRIVATE
    daq::opendaq
    example_module
)

add_dependencies(
    fb_application_example
    example_module
    daq::ref_device_module
    daq::ref_fb_module
)

cmake_minimum_required(VERSION 3.1.3)

project(rov_driver)

add_library(rov_driver MODULE
	rov_driver.c
)
set_target_properties(rov_driver PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS ON # gnu11
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO # c++11
    PREFIX ""
)
	
include_directories(
	../../include
)
link_directories(
)

target_link_libraries(rov_driver
	pthread
	dl
)

#post build
add_custom_command(TARGET rov_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:rov_driver> ${CMAKE_CURRENT_SOURCE_DIR}/../
)
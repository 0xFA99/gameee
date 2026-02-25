cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

project(rpg_game C)

set(CMAKE_C_STANDARD 99)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_BUILD_RPATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_INSTALL_RPATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)

include_directories(include)

set(SOURCES
        src/main.c
)

add_executable(rpg ${SOURCES})
if(WIN32)
    target_link_libraries(rpg raylib)
else()
    target_link_libraries(rpg raylib m)
endif()

set_target_properties(rpg PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/lib"
)

if(WIN32)
    add_custom_command(TARGET rpg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        $<TARGET_FILE_DIR:rpg>
        COMMENT "Copying all libraries to build directory")
    
    set_target_properties(rpg PROPERTIES
        VS_DEBUGLER_WORKING_DIRECTORY $<TARGET_FILE_DIR:rpg>)
endif()

add_custom_command(TARGET rpg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:rpg>/assets"
)

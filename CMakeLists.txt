cmake_minimum_required(VERSION 3.26)
project(STCMapEngine)

set(CMAKE_CXX_STANDARD 23)
set(PROJECT_NAME STCMapEngine)
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-narrowing -Wno-sign-compare -Wno-unused-variable")
endif ()
message(${CMAKE_CXX_FLAGS})
include(scripts/functions.cmake)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

find_package(directxtex CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(entt CONFIG REQUIRED)

set(src_files "")
set(local_libs_dir "")
set(src_dirs "")

scan_dirs(${CMAKE_CURRENT_SOURCE_DIR}/src src_dirs)
foreach (dir_name ${src_dirs})
    set(is_library false)
    have_cmake(${CMAKE_CURRENT_SOURCE_DIR}/src/${dir_name} is_library)
    if(${is_library})
        list(APPEND local_libs_dir ${dir_name})
    endif ()
endforeach ()
foreach (dir_name ${local_libs_dir})
    list(REMOVE_ITEM src_dirs ${dir_name})
endforeach ()

scan(src_files ${CMAKE_CURRENT_SOURCE_DIR}/src .cpp)
scan(headers ${CMAKE_CURRENT_SOURCE_DIR}/src .h)
foreach (dir_name ${src_dirs})
    set(dir_src "")
    set(dir_header "")
    scan(dir_src ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir_name} .cpp)
    foreach (item ${dir_src})
        list(APPEND src_files ${item})
    endforeach ()
    scan(dir_header ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir_name} .h)
    foreach (item ${dir_src})
        list(APPEND headers ${item})
    endforeach ()
endforeach ()

if(WIN32)
    add_link_options(
            /SUBSYSTEM:WINDOWS
            /DYNAMICBASE
            Version.lib
            Comctl32.lib
            /MACHINE:X64
    )

    set(shaders_dir ${PROJECT_SOURCE_DIR}/shaders)
    add_executable(${PROJECT_NAME} ${src_files} ${headers})
    target_compile_options(${PROJECT_NAME} INTERFACE /arch:SSE4.1)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
    target_link_libraries(${PROJECT_NAME} PRIVATE dxgi d3d12 dxguid d3dcompiler runtimeobject
            assimp::assimp
            EnTT::EnTT
            Microsoft::DirectXTex
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
            ${shaders_dir}/common
            ${shaders_dir}/rs
            ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
else ()
    add_executable(${PROJECT_NAME} ${src_files})
    target_compile_options(${PROJECT_NAME} INTERFACE -msse4.1)
endif ()

if(EXISTS ${PROJECT_SOURCE_DIR}/shaders/bin)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/shaders)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    endif ()
    file(GLOB dir RELATIVE ${PROJECT_SOURCE_DIR}/shaders/bin ${PROJECT_SOURCE_DIR}/shaders/bin/*.cso)
    foreach (file ${dir})
        file(COPY_FILE ${PROJECT_SOURCE_DIR}/shaders/bin/${file} ${CMAKE_CURRENT_BINARY_DIR}/shaders/${file})
    endforeach ()
endif ()

if(EXISTS ${PROJECT_SOURCE_DIR}/assets)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/assets)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets)
    endif()
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/assets/meshes)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets/meshes)
    endif()
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/assets/textures)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/assets/textures)
    endif()
    file(GLOB dir RELATIVE ${PROJECT_SOURCE_DIR}/assets/meshes ${PROJECT_SOURCE_DIR}/assets/meshes/*)
    foreach (file ${dir})
        file(COPY_FILE ${PROJECT_SOURCE_DIR}/assets/meshes/${file} ${CMAKE_CURRENT_BINARY_DIR}/assets/meshes/${file})
    endforeach ()
    file(GLOB dir RELATIVE ${PROJECT_SOURCE_DIR}/assets/textures ${PROJECT_SOURCE_DIR}/assets/textures/*)
    foreach (file ${dir})
        file(COPY_FILE ${PROJECT_SOURCE_DIR}/assets/textures/${file} ${CMAKE_CURRENT_BINARY_DIR}/assets/textures/${file})
    endforeach ()
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/resources)
    file(GLOB dir RELATIVE ${PROJECT_SOURCE_DIR}/resources ${PROJECT_SOURCE_DIR}/resources/*)
    foreach (file ${dir})
        set(file_source ${PROJECT_SOURCE_DIR}/resources/${file})
        set(file_target ${PROJECT_BINARY_DIR}/resources/${file})
        if(IS_DIRECTORY ${file_source})
            file(COPY ${file_source} DESTINATION ${file_target})
        else()
            configure_file(${file_source} ${file_target} COPYONLY)
        endif()
    endforeach ()
endif ()

foreach (subdir ${local_libs_dir})
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/${subdir})
endforeach ()
enable_testing()
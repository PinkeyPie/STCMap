cmake_minimum_required(VERSION 3.26)
project(STCMapEngine)

set(CMAKE_CXX_STANDARD 20)
set(PROJECT_NAME STCMapEngine)
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wno-narrowing -Wno-sign-compare -Wno-unused-variable")
endif ()
message(${CMAKE_CXX_FLAGS})
include(scripts/functions.cmake)

set(src_files "")
set(local_libs_dir "")
set(src_dirs "")
set(inc_dirs "")

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

if(MSVC)
    add_link_options(
            /SUBSYSTEM:WINDOWS
            /DYNAMICBASE
            Version.lib
            Comctl32.lib
            /MACHINE:X64
    )
    set(common_sources)
    set(common_headers)
    scan_recurse(common_sources ${PROJECT_SOURCE_DIR}/common .cpp)
    scan_recurse(common_headers ${PROJECT_SOURCE_DIR}/common .h)
    add_library(common SHARED ${common_sources} ${common_headers})
    target_link_libraries(common PUBLIC dxgi d3d12 dxguid d3dcompiler)
    target_compile_definitions(common PRIVATE COMMON_LIBRARY)

    add_executable(${PROJECT_NAME} ${src_files})
    target_compile_options(${PROJECT_NAME} INTERFACE /arch:SSE4.1)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(${PROJECT_NAME} PRIVATE common d2d1)
    target_include_directories(${PROJECT_NAME} PRIVATE common)

#    add_custom_target(shaders)
#    set(HLSL_SHADER_FILES src/shaders/color.hlsl)
#    set_source_files_properties(src/shaders/color.hlsl PROPERTIES ShaderType "ps")
#    set_source_files_properties(${HLSL_SHADER_FILES} PROPERTIES ShaderModel "5_1")
#    foreach (FILE ${HLSL_SHADER_FILES})
#        get_filename_component(FILE_WE ${FILE} NAME_WE)
#        get_source_file_property(shadertype ${FILE} ShaderType)
#        get_source_file_property(shadermodel ${FILE} ShaderModel)
#        add_custom_command(TARGET shaders
#                COMMAND fxc.exe ${FILE} /nologo /E "PS" /T ${shadertype}_${shadermodel} /Zi /Fo "${CMAKE_BINARY_DIR}/${FILE_WE}.cso" /Fd "${CMAKE_BINARY_DIR}/${FILE_WE}.pdb"
#                MAIN_DEPENDENCY ${FILE}
#                COMMENT "HLSL ${FILE}"
#                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders
#                VERBATIM
#        )
#    endforeach (FILE)
#    add_dependencies(${PROJECT_NAME} shaders)
else ()
    add_executable(${PROJECT_NAME} ${src_files})
    target_compile_options(${PROJECT_NAME} INTERFACE -msse4.1)
endif ()

if(EXISTS ${PROJECT_SOURCE_DIR}/src/shaders)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/shaders)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    endif ()
    file(GLOB dir RELATIVE ${PROJECT_SOURCE_DIR}/src/shaders ${PROJECT_SOURCE_DIR}/src/shaders/*.hlsl)
    foreach (file ${dir})
        file(COPY_FILE ${PROJECT_SOURCE_DIR}/src/shaders/${file} ${CMAKE_CURRENT_BINARY_DIR}/shaders/${file})
    endforeach ()
endif ()

if(EXISTS ${PROJECT_SOURCE_DIR}/resources)
    file(GLOB dir RELATIVE ${PROJECT_SOURCE_DIR}/resources ${PROJECT_SOURCE_DIR}/resources/*)
    foreach (file ${dir})
        file(COPY_FILE ${PROJECT_SOURCE_DIR}/resources/${file} ${CMAKE_CURRENT_BINARY_DIR}/${file})
    endforeach ()
endif ()

foreach (subdir ${local_libs_dir})
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/${subdir})
endforeach ()
enable_testing()
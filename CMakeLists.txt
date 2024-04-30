cmake_minimum_required(VERSION 3.12) 

macro(print_list list)
        foreach(item ${${list}})
            message(${item})
        endforeach(item)
endmacro()

macro(header_directories return_list relative_path infix_path symbol)
    FILE(GLOB_RECURSE new_list RELATIVE ${relative_path} 
    ${relative_path}/${infix_path}/*.h 
    ${relative_path}/${infix_path}/*.hpp 
    ${relative_path}/${infix_path}/*.cpp)

    #[[
    SET(dir_list "")
    FOREACH(file_path ${new_list})
        GET_FILENAME_COMPONENT(dir_path ${file_path} PATH)
        SET(dir_list ${dir_list} ${dir_path})
    ENDFOREACH()
    LIST(REMOVE_DUPLICATES dir_list)
    SET(${return_list} ${dir_list})
    ]]

    list(TRANSFORM new_list PREPEND ${${symbol}}/)
    set(${return_list} ${new_list})
endmacro()

macro(source_group_by_dir attr_path source_files)
    if(MSVC)
        set(sgbd_cur_dir ${CMAKE_CURRENT_SOURCE_DIR})
        foreach(sgbd_file ${${source_files}})
            string(REGEX REPLACE ${sgbd_cur_dir}/\(.*\) \\1 sgbd_fpath ${sgbd_file})
            string(REGEX REPLACE "\(.*\)/.*" \\1 sgbd_group_name ${sgbd_fpath})
            string(COMPARE EQUAL ${sgbd_fpath} ${sgbd_group_name} sgbd_nogroup)
            string(COMPARE EQUAL ${attr_path} ${sgbd_group_name} sgbd_noparent)
            string(REPLACE "${attr_path}" "" sgbd_group_name_last ${sgbd_group_name})
            if(sgbd_nogroup OR sgbd_noparent)
                set(sgbd_group_name_last "\\")
            endif(sgbd_nogroup OR sgbd_noparent)
            source_group(${sgbd_group_name_last} FILES ${sgbd_file})
        endforeach(sgbd_file)
    endif(MSVC)
endmacro(source_group_by_dir)

if(UNIX)
    set(CMAKE_CXX_FLAGS -fpermissive)
elseif(WIN32)
    ADD_DEFINITIONS(-DUNICODE)
    ADD_DEFINITIONS(-D_UNICODE)
    #[[
    if (MSVC)
    target_compile_options(${library} PUBLIC "/utf-8")
    endif()
    #]]
    cmake_host_system_information(RESULT CPU_NUMBER_OF_LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP${CPU_NUMBER_OF_LOGICAL_CORES}>)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
set(EXECUTABLE_OUTPUT_PATH ../bin/) 
set(LIBRARY_OUTPUT_PATH ../lib/) 
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
#[[
set(dir "./src")
file(GLOB_RECURSE headers_tetris "./src/tetris/*.h" "./src/tetris/*.hpp")
source_group("include" FILES ${CURRENT_HEADERS})
]]

project(TetrisBot) 

add_compile_definitions(_USE_MATH_DEFINES)

find_package(Threads REQUIRED)
link_libraries(Threads::Threads)

include_directories(./src/import ./src/tetris)

set(dir ./src)

set(bot_demo_files 
 ${dir}/bot.cpp
 ${dir}/tetris/tetrisBase.h
 ${dir}/tetris/tetrisBase.cpp
 ${dir}/tetris/tetrisCore.h
 ${dir}/tetris/tetrisGame.h
 ${dir}/tetris/tetrisGame.cpp
 ${dir}/import/pdqsort.h
 ${dir}/import/timer.hpp
 ${dir}/import/tool.h
 ${dir}/import/threadPool.h
)

header_directories(ftxui_sets ${CMAKE_SOURCE_DIR}/src import/ftxui dir)

#use atomic_sync
#header_directories(atomic_sync ${CMAKE_SOURCE_DIR}/src import/atomic_sync dir)
#list(APPEND bot_demo_files ${atomic_sync})

list(FILTER ftxui_sets EXCLUDE REGEX ".*_test.*|.*fuzzer.*" )
#print_list(ftxui_sets)
#list(APPEND bot_demo_files ${ftxui_sets})



set(classic_files
 ${dir}/classic_train.cpp
 ${dir}/tetris/tetrisBase.h
 ${dir}/tetris/tetrisBase.cpp
 ${dir}/tetris/tetrisCore.h
 ${dir}/tetris/tetrisGame.h
 ${dir}/tetris/tetrisGame.cpp
 ${dir}/import/pdqsort.h
 ${dir}/import/timer.hpp
 ${dir}/import/tool.h
 ${dir}/import/pso.h
 ${dir}/import/threadPool.h
)

#list(APPEND classic_files ${ftxui_sets})

set(tune_files 
 ${dir}/tune.cpp
 ${dir}/tetris/tetrisBase.h
 ${dir}/tetris/tetrisBase.cpp
 ${dir}/tetris/tetrisCore.h
 ${dir}/tetris/tetrisGame.h
 ${dir}/tetris/tetrisGame.cpp
 ${dir}/import/pdqsort.h
 ${dir}/import/timer.hpp
 ${dir}/import/tool.h
 ${dir}/import/sb_tree.h
 ${dir}/import/bst_base.h
 ${dir}/import/threadPool.h
)

list(APPEND tune_files ${ftxui_sets})

set(bot_library_files 
 ${dir}/dllmain.cpp
 ${dir}/tetris/tetrisBase.h
 ${dir}/tetris/tetrisBase.cpp
 ${dir}/tetris/tetrisCore.h
 ${dir}/tetris/tetrisGame.h
 ${dir}/tetris/tetrisGame.cpp
 ${dir}/import/pdqsort.h
 ${dir}/import/timer.hpp
 ${dir}/import/tool.h
 ${dir}/import/threadPool.h
)

set(app_files
 ${dir}/tetris/tetrisBase.h
 ${dir}/tetris/tetrisBase.cpp
 ${dir}/tetris/tetrisCore.h
 ${dir}/tetris/tetrisGame.h
 ${dir}/tetris/tetrisGame.cpp
 ${dir}/import/pdqsort.h
 ${dir}/import/timer.hpp
 ${dir}/import/tool.h
 ${dir}/import/threadPool.h
)

header_directories(app_sets ${CMAKE_SOURCE_DIR}/src app dir)
header_directories(chrome_sets ${CMAKE_SOURCE_DIR}/src import/chrome dir)
list(APPEND app_files ${app_sets} ${chrome_sets})


set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFIND_TARGETS_FOLDER "CMakeTargets")

source_group_by_dir(${dir} bot_demo_files)
set_property (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT bot_demo)
add_executable(bot_demo ${bot_demo_files}) 
if(MSVC)
target_compile_options(bot_demo PUBLIC "/utf-8")
endif(MSVC)

source_group_by_dir(${dir} classic_files)
add_executable(classic_train ${classic_files}) 
if(MSVC)
target_compile_options(classic_train PUBLIC "/utf-8")
endif(MSVC)

if(WIN32)
 source_group_by_dir(${dir} tune_files)
 add_executable(tune ${tune_files}) 
 if(MSVC)
 target_compile_options(tune PUBLIC "/utf-8")
 endif(MSVC)
endif()

if(WIN32)
 source_group_by_dir(${dir} app_files)
 include_directories(./src/app ./src/import/chrome)
 add_executable(app WIN32 ${app_files})
 source_group_by_dir(${dir} bot_library_files)
 add_library(bot SHARED ${bot_library_files})
endif()

if(UNIX)
    target_link_libraries(bot_demo Threads::Threads)
    target_link_libraries(bot_demo atomic)
    target_link_libraries(classic_train Threads::Threads)
endif()

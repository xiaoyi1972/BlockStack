cmake_minimum_required(VERSION 3.12) 

macro(source_group_by_dir attr_path source_files)
    if(MSVC)
        set(sgbd_cur_dir ${CMAKE_CURRENT_SOURCE_DIR})
        foreach(sgbd_file ${${source_files}})
            string(REGEX REPLACE ${sgbd_cur_dir}/\(.*\) \\1 sgbd_fpath ${sgbd_file})
            string(REGEX REPLACE "\(.*\)/.*" \\1 sgbd_group_name ${sgbd_fpath})
            string(REGEX REPLACE ".*/\(.*\)$" \\1 sgbd_group_name_last ${sgbd_group_name})
            string(COMPARE EQUAL ${sgbd_fpath} ${sgbd_group_name} sgbd_nogroup)
            string(COMPARE EQUAL ${attr_path} ${sgbd_group_name} sgbd_noparent)
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
    cmake_host_system_information(RESULT CPU_NUMBER_OF_LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP${CPU_NUMBER_OF_LOGICAL_CORES}>)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(EXECUTABLE_OUTPUT_PATH ../bin/) 
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(dir "./src")
file(GLOB_RECURSE headers_tetris "./src/tetris/*.h" "./src/tetris/*.hpp")
source_group("include" FILES ${CURRENT_HEADERS})

project(TetrisBot) 

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
 ${dir}/import/threadPool.h
 ${dir}/import/pdqsort.h
 ${dir}/import/timer.hpp
 ${dir}/import/tool.h
)

set(tune_files 
 ${dir}/tune.cpp
 ${dir}/tetris/tetrisBase.h
 ${dir}/tetris/tetrisBase.cpp
 ${dir}/tetris/tetrisCore.h
 ${dir}/tetris/tetrisGame.h
 ${dir}/tetris/tetrisGame.cpp
 ${dir}/import/threadPool.h
 ${dir}/import/pdqsort.h
 ${dir}/import/timer.hpp
 ${dir}/import/tool.h
)

set(bot_library_files 
 ${dir}/dllmain.cpp
 ${dir}/tetris/tetrisBase.h
 ${dir}/tetris/tetrisBase.cpp
 ${dir}/tetris/tetrisCore.h
 ${dir}/tetris/tetrisGame.h
 ${dir}/tetris/tetrisGame.cpp
 ${dir}/import/threadPool.h
 ${dir}/import/pdqsort.h
 ${dir}/import/timer.hpp
 ${dir}/import/tool.h
)

set(app_files
 ${dir}/tetris/tetrisBase.h
 ${dir}/tetris/tetrisBase.cpp
 ${dir}/tetris/tetrisCore.h
 ${dir}/tetris/tetrisGame.h
 ${dir}/tetris/tetrisGame.cpp
 ${dir}/import/threadPool.h
 ${dir}/import/pdqsort.h
 ${dir}/import/timer.hpp
 ${dir}/import/tool.h
 ${dir}/app/app.cpp
 ${dir}/app/app.h
 ${dir}/app/engine.cpp
 ${dir}/app/engine.h
 ${dir}/app/framework.h
 ${dir}/app/resource.h
 ${dir}/app/targetver.h
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFIND_TARGETS_FOLDER "CMakeTargets")

source_group_by_dir(${dir} bot_demo_files)
set_property ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT bot_demo )
add_executable(bot_demo ${bot_demo_files}) 

source_group_by_dir(${dir} tune_files)
add_executable(tune ${tune_files}) 

if(WIN32)
 source_group_by_dir(${dir} app_files)
 include_directories(./src/app)
 add_executable(app WIN32 ${app_files})
 source_group_by_dir(${dir} bot_library_files)
 add_library(bot SHARED ${bot_library_files})
endif()

if(UNIX)
    target_link_libraries(bot_demo Threads::Threads)
endif()

#add_executable(tune ./src/tune.cpp ${IMPORTS} ${TETRIS})
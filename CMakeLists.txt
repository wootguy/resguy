cmake_minimum_required(VERSION 2.8)
project(resguy)

set(SOURCE_FILES 
    src/globals.h 	src/main.cpp
	src/Bsp.h		src/Bsp.cpp
	src/Wad.h		src/Wad.cpp
	src/Mdl.h		src/Mdl.cpp
	src/Entity.h	src/Entity.cpp
	src/Keyvalue.h	src/Keyvalue.cpp
	src/util.h		src/util.cpp
	src/studio.h
)

if(MSVC)
    if(NOT MSVC_VERSION GREATER 1600) #MSVC 10
        message(FATAL_ERROR "Visual Studio 10 or greater is required")
    endif()                            
    
    # compile using the static runtime
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	
	# disable some warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4018 /wd4267 /wd4996")
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wno-unused -Wno-unused-result -Wno-sign-compare")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-Os")
else()
    message(WARNING "Unknown compiler, no flags set")
endif()

include_directories(src)
add_executable(resguy ${SOURCE_FILES})
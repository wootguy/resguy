cmake_minimum_required(VERSION 3.8)
project(resguy)

set(ANGELSCRIPT_SDK_PATH "angelscript")

set(ANGELSCRIPT_ADDON_SOURCES 
	${ANGELSCRIPT_SDK_PATH}/add_on/scriptany/scriptany.cpp
	${ANGELSCRIPT_SDK_PATH}/add_on/scriptstdstring/scriptstdstring.cpp
	#${ANGELSCRIPT_SDK_PATH}/add_on/scriptstdstring/scriptstdstring_utils.cpp
	${ANGELSCRIPT_SDK_PATH}/add_on/scriptdictionary/scriptdictionary.cpp
	${ANGELSCRIPT_SDK_PATH}/add_on/scriptarray/scriptarray.cpp
	${ANGELSCRIPT_SDK_PATH}/add_on/scriptbuilder/scriptbuilder.cpp
	${ANGELSCRIPT_SDK_PATH}/add_on/scriptfile/scriptfile.cpp
)

add_executable(resguy
	src/globals.h 	src/main.cpp
	src/Bsp.h		src/Bsp.cpp
	src/Wad.h		src/Wad.cpp
	src/Mdl.h		src/Mdl.cpp
	src/Entity.h	src/Entity.cpp
	src/Keyvalue.h	src/Keyvalue.cpp
	src/util.h		src/util.cpp
	src/script.h	src/script.cpp
	#src/studio.h
	
	#src/as_object/CCVar.h	src/as_object/CCVar.cp
)

if(MSVC)
    if(NOT MSVC_VERSION GREATER 1600) #MSVC 10
        message(FATAL_ERROR "Visual Studio 10 or greater is required")
    endif()                            
    
    # compile using the static runtime
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	
	# disable some warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4018 /wd4267 /wd4996 /wd4146")
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wno-unused -Wno-unused-result -Wno-sign-compare")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-Os")
else()
    message(WARNING "Unknown compiler, no flags set")
endif()

target_include_directories(resguy PRIVATE src)


set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(src/as_object)

add_library(as_objects ${ANGELSCRIPT_ADDON_SOURCES} ${GENERATED_SOURCES} ${GENERATED_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/as_object" PREFIX "Header Files" FILES ${GENERATED_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/as_object" PREFIX "Source Files" FILES ${GENERATED_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/angelscript" PREFIX "Source Files" FILES ${ANGELSCRIPT_ADDON_SOURCES})

target_include_directories(as_objects PUBLIC ${CMAKE_CURRENT_LIST_DIR}/${ANGELSCRIPT_SDK_PATH}/angelscript/include)
target_include_directories(as_objects PUBLIC ${CMAKE_CURRENT_LIST_DIR}/${ANGELSCRIPT_SDK_PATH}/add_on)
target_link_directories(as_objects PUBLIC ${CMAKE_CURRENT_LIST_DIR}/${ANGELSCRIPT_SDK_PATH}/angelscript/lib)

target_link_libraries(resguy as_objects)
target_link_libraries(resguy debug angelscript64d optimized angelscript64)
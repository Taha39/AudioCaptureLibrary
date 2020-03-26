cmake_minimum_required(VERSION 3.7)

add_definitions(/ZI)
add_subdirectory(test_app)
add_library(audio_capturer
	GetAudio.h
	GetAudio.cpp
	)
	
if(MSVC)
	#set_source_files_properties(${SOURCE_FILES}
	#	PROPERTIES COMPILE_FLAGS "/W3 /WX")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	
endif(MSVC)


target_include_directories(audio_capturer PUBLIC 
	#$<BUILD_INTERFACE:${BOOST_LIB}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	)
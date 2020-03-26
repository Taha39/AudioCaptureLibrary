cmake_minimum_required(VERSION 3.7)

add_definitions(/ZI)

add_executable( test_app
	MicCaptureWSAAPI.cpp
	)
	
	if(MSVC)
	#set_source_files_properties(${SOURCE_FILES}
	#	PROPERTIES COMPILE_FLAGS "/W3 /WX")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	
endif(MSVC)

target_link_libraries(test_app PRIVATE audio_capturer)
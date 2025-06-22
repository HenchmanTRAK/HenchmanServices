
qt_add_library(ServiceHelper SHARED "ServiceHelper.cpp")
qt_disable_unicode_defines(ServiceHelper)

message(STATUS "Checking for OpenSSL: " ${OPENSSL_VERSION})

target_link_libraries(
	ServiceHelper PRIVATE
	Qt6::Core
	RegistryManager
)

target_include_directories(
	ServiceHelper INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR} 
	#${OPENSSL_INCLUDE_DIR}
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ServiceHelper PROPERTY CXX_STANDARD 20)
endif()
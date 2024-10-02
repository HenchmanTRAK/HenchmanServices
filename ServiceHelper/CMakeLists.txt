
qt_add_library(ServiceHelper STATIC "ServiceHelper.cpp")

message(STATUS "Checking for OpenSSL: " ${OPENSSL_VERSION})

target_include_directories(
	ServiceHelper PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR} 
	#${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(
	ServiceHelper PUBLIC
	#OpenSSL::applink
	OpenSSL::SSL
	OpenSSL::Crypto
	#${OPENSSL_CRYPTO_LIBRARY} 
	#${OPENSSL_SSL_LIBRARY}
	RegistryManager
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ServiceHelper PROPERTY CXX_STANDARD 20)
endif()
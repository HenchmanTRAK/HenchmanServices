
add_library(ServiceHelper "ServiceHelper.cpp")

target_include_directories(
	ServiceHelper PUBLIC 
	"."
	${OPENSSL_INCLUDE_DIR}
)
target_link_libraries(
	ServiceHelper PRIVATE 
	OpenSSL::applink
	${OPENSSL_CRYPTO_LIBRARY} 
	${OPENSSL_SSL_LIBRARY} 
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ServiceHelper PROPERTY CXX_STANDARD 20)
endif()
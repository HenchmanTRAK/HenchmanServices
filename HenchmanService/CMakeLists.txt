# CMakeList.txt : CMake project for HenchmanServices, include source and define
# project specific logic here.
#

# Add source to this project's executable.
#add_executable(HenchmanServices "HenchmanServices.cpp")
qt_add_executable(HenchmanService "HenchmanService.cpp" "HenchmanService.h" )

target_include_directories(
	HenchmanService PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR}
#	${OPENSSL_INCLUDE_DIR}
#	${CMAKE_SOURCE_DIR}/resources
)
message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL ssl library: ${OPENSSL_SSL_LIBRARY}")
message(STATUS "OpenSSL crypto library: ${OPENSSL_CRYPTO_LIBRARY}")

target_link_libraries(
	HenchmanService PRIVATE 
	Qt6::Core
	Qt6::Network
	Qt6::Sql
	SimpleIni
	HenchmanServiceException
	EventManager
	ServiceHelper
	DatabaseManager
	RegistryManager
	SQLiteManager2
	TRAKManager
	ServiceController
)

set_target_properties(
	HenchmanService PROPERTIES 
	LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator'\""
	AUTOMOC TRUE
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanService PROPERTY CXX_STANDARD 20)
endif()

#if (CMAKE_VERSION VERSION_GREATER 2.21)
#  install(FILES $<TARGET_RUNTIME_DLLS:HenchmanService> TYPE BIN)
#endif()

# TODO: Add tests and install targets if needed.
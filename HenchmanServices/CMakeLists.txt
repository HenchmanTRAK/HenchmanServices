# CMakeList.txt : CMake project for HenchmanServices, include source and define
# project specific logic here.
#

# Add source to this project's executable.
#add_executable(HenchmanServices "HenchmanServices.cpp")
qt_add_executable(HenchmanServices "HenchmanServices.cpp" "HenchmanServices.h" )

target_include_directories(HenchmanServices PRIVATE ${OPENSSL_INCLUDE_DIR})

target_link_libraries(
	HenchmanServices PRIVATE 
	Qt6::Core
	SimpleIni
	SQLiteCpp 
	sqlite3
	OpenSSL::applink
	${OPENSSL_CRYPTO_LIBRARY} 
	${OPENSSL_SSL_LIBRARY} 
	HenchmanServiceException 
	ServiceHelper
	RegistryManager
	DatabaseManager
	SQLiteManager 
	TRAKManager
)

set_target_properties(
	HenchmanServices PROPERTIES 
	LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator'\""
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServices PROPERTY CXX_STANDARD 20)
endif()

if (CMAKE_VERSION VERSION_GREATER 2.21)
  install(FILES $<TARGET_RUNTIME_DLLS:HenchmanServices> TYPE BIN)
endif()

# TODO: Add tests and install targets if needed.

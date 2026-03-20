INCLUDE(GenerateExportHeader)
# CMakeList.txt : CMake project for HenchmanServices, include source and define
# project specific logic here.
#

QT_ADD_LIBRARY(HenchmanServiceLibrary SHARED HenchmanServiceLibrary.cpp HenchmanServiceLibrary.h)
QT_DISABLE_UNICODE_DEFINES(HenchmanServiceLibrary)

GENERATE_EXPORT_HEADER(HenchmanServiceLibrary)

message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL ssl library: ${OPENSSL_SSL_LIBRARY}")
message(STATUS "OpenSSL crypto library: ${OPENSSL_CRYPTO_LIBRARY}")


target_include_directories(
	HenchmanServiceLibrary 
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
	PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
	PRIVATE "${AUTOGEN_BUILD_DIR}/include_${CMAKE_BUILD_TYPE}"
)

# target_compile_definitions(HenchmanServiceLibrary PRIVATE HENCHMAN_SERVICE)

target_link_libraries(
	HenchmanServiceLibrary PRIVATE
	ServiceController
	TRAKManager
	DatabaseManager
	EmployeesManager
	UsersManager
	TRAKEntriesManager
	QueryManager
	NetworkManager
	SQLiteManager2
	HenchmanServiceException
	EventManager
	ServiceHelper
	RegistryManager
	SimpleIni
	Qt6::Concurrent
	Qt6::Network
	Qt6::Sql
	Qt6::Core
)



set_target_properties(HenchmanServiceLibrary PROPERTIES OUTPUT_NAME "HenchmanService")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServiceLibrary PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.

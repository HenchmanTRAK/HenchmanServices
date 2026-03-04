# CMakeList.txt : CMake project for HenchmanServices, include source and define
# project specific logic here.
#

# Add source to this project's executable.

include(generate_product_version)

generate_product_version(
   VersionFilesOutputVariable
   NAME "HenchmanService"
   ICON ${CMAKE_CURRENT_SOURCE_DIR}/henchman_trak_logo_redo.ico
   VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR}
   VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR}
   VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH}
   COMPANY_NAME HenchmanTRAK
)

#add_executable(HenchmanServices "HenchmanServices.cpp")
qt_add_executable(HenchmanService "HenchmanService.cpp" "HenchmanService.h" "${CMAKE_SOURCE_DIR}/version" ${VersionFilesOutputVariable})
qt_disable_unicode_defines(HenchmanService)

target_include_directories(
	HenchmanService PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR}
#	${ServiceController_DIR}
#	${OPENSSL_INCLUDE_DIR}
#	${CMAKE_SOURCE_DIR}/resources
)

message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL ssl library: ${OPENSSL_SSL_LIBRARY}")
message(STATUS "OpenSSL crypto library: ${OPENSSL_CRYPTO_LIBRARY}")


#target_link_directories(HenchmanService PRIVATE
#	C:/Users/Willem/source/repos/CNC_Current_Tracker/CNC_Current_Tracker/x64/Release
#)

target_link_libraries(
	HenchmanService PRIVATE 
	HenchmanServiceLibrary
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
	HenchmanServiceTaskScheduler
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
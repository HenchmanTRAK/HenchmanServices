INCLUDE(GenerateExportHeader)

qt_add_library(DatabaseManager SHARED DatabaseManager.cpp DatabaseManager.h)
qt_disable_unicode_defines(DatabaseManager)

GENERATE_EXPORT_HEADER(DatabaseManager)

message(STATUS "Checking for MYSQL: " ${MYSQL_CONCPP_ROOT_DIR})

#QT_WRAP_CPP(
#	DatabaseManager DatabaseManager.cpp DatabaseManager.h
#)

# target_include_directories(DatabaseManager PRIVATE "DatabaseManager.h")

# target_compile_definitions(DatabaseManager PRIVATE DATABASE_MANAGER)

target_include_directories(
	DatabaseManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
	PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
	PRIVATE "${AUTOGEN_BUILD_DIR}/include_${CMAKE_BUILD_TYPE}"
)

target_link_libraries(
	DatabaseManager PRIVATE
	EmployeesManager
	UsersManager
	TRAKEntriesManager
	QueryManager
	NetworkManager
	SQLiteManager2
	HenchmanServiceException
	ServiceHelper
	EventManager
	RegistryManager
	Qt6::Concurrent
	Qt6::Network
	Qt6::Sql
	Qt6::Core
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET DatabaseManager PROPERTY CXX_STANDARD 20)
endif()

# set_property(TARGET DatabaseManager PROPERTY AUTOMOC ON)
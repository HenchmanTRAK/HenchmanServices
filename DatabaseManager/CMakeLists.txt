
qt_add_library(DatabaseManager SHARED "DatabaseManager.cpp")
qt_disable_unicode_defines(DatabaseManager)

message(STATUS "Checking for MYSQL: " ${MYSQL_CONCPP_ROOT_DIR})

target_include_directories(
	DatabaseManager PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
	DatabaseManager PRIVATE
	Qt6::Core
	Qt6::Sql
	Qt6::Network
	HenchmanServiceException
	EventManager
	ServiceHelper
	RegistryManager
	SQLiteManager2
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET DatabaseManager PROPERTY CXX_STANDARD 20)
endif()
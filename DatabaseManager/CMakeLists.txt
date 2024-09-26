
qt_add_library(DatabaseManager SHARED "DatabaseManager.cpp")
message(STATUS "Checking for MYSQL: " ${MYSQL_CONCPP_ROOT_DIR})
target_include_directories(
	DatabaseManager PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
	DatabaseManager PUBLIC
	Qt6::Sql
	Qt6::Network
	http_request
	ServiceHelper
	RegistryManager
	#mysql::concpp
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET DatabaseManager PROPERTY CXX_STANDARD 20)
endif()
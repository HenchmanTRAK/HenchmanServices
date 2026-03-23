
qt_add_library(QueryManager STATIC "QueryManager.cpp" "QueryManager.h")
qt_disable_unicode_defines(QueryManager)


# target_include_directories(DatabaseManager PRIVATE "DatabaseManager.h")

# target_compile_definitions(QueryManager PRIVATE QUERY_MANAGER)

target_link_libraries(
	QueryManager PRIVATE
	HenchmanServiceException
	EventManager
	ServiceHelper
	RegistryManager
	Qt6::Network
	Qt6::Sql
	Qt6::Core
)

target_include_directories(
	QueryManager INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR}
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET QueryManager PROPERTY CXX_STANDARD 20)
endif()
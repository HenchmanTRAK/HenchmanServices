
qt_add_library(NetworkManager SHARED "NetworkManager.cpp" "NetworkManager.h")
qt_disable_unicode_defines(NetworkManager)

message(STATUS "Checking for MYSQL: " ${MYSQL_CONCPP_ROOT_DIR})


target_include_directories(
	NetworkManager INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(NetworkManager PRIVATE NETWORK_MANAGER_LIBRARY)

target_link_libraries(
	NetworkManager PRIVATE
	HenchmanServiceException
	EventManager
	ServiceHelper
	RegistryManager
	Qt6::Network
	Qt6::Core
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET NetworkManager PROPERTY CXX_STANDARD 20)
endif()
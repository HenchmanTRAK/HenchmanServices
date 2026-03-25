INCLUDE(GenerateExportHeader)

#SET(MOC_SOURCES NetworkManager.cpp)
#QT_WRAP_CPP(
#	NetworkManager NetworkManager.cpp NetworkManager.h
#)
#message(STATUS "Checking MOC_SOURCES: " ${MOC_SOURCES})

qt_add_library(NetworkManager SHARED NetworkManager.cpp NetworkManager.h)
#add_library(NetworkManager SHARED "NetworkManager.cpp" "NetworkManager.h" ${MOC_SOURCES})
qt_disable_unicode_defines(NetworkManager)

QT_WRAP_CPP(
	NetworkManager NetworkManager.cpp
)



#qt_wrap_cpp(NetworkManager NetworkManager.h)


message(STATUS "Checking for MYSQL: " ${MYSQL_CONCPP_ROOT_DIR})
message(STATUS "Checking for CMAKE_CURRENT_BINARY_DIR: " ${CMAKE_CURRENT_BINARY_DIR})
message(STATUS "Checking for CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR})

# target_compile_definitions(NetworkManager PRIVATE NETWORK_MANAGER)

target_include_directories(
	NetworkManager PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

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

GENERATE_EXPORT_HEADER(NetworkManager)
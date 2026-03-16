INCLUDE(GenerateExportHeader)

#SET(MOC_SOURCES NetworkManager.cpp)
#QT_WRAP_CPP(
#	NetworkManager NetworkManager.cpp NetworkManager.h
#)
#message(STATUS "Checking MOC_SOURCES: " ${MOC_SOURCES})

qt_add_library(NetworkManager SHARED NetworkManager.cpp NetworkManager.h)
#add_library(NetworkManager SHARED "NetworkManager.cpp" "NetworkManager.h" ${MOC_SOURCES})
qt_disable_unicode_defines(NetworkManager)

GENERATE_EXPORT_HEADER(NetworkManager)

message(STATUS "Checking for MYSQL: " ${MYSQL_CONCPP_ROOT_DIR})
message(STATUS "Checking for CMAKE_CURRENT_BINARY_DIR: " ${CMAKE_CURRENT_BINARY_DIR})
message(STATUS "Checking for CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "Checking AUTOGEN_BUILD_DIR: " ${AUTOGEN_BUILD_DIR})
message(STATUS "Checking AUTOMOC_INCLUDE_DIRECTORIES: " ${AUTOMOC_INCLUDE_DIRECTORIES})

# target_compile_definitions(NetworkManager PRIVATE NETWORK_MANAGER)

#target_include_directories(
#	NetworkManager PUBLIC 
#	${CMAKE_CURRENT_SOURCE_DIR}
#	${CMAKE_CURRENT_BINARY_DIR}
#)

target_include_directories(NetworkManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} PUBLIC ${CMAKE_CURRENT_BINARY_DIR} PRIVATE "${AUTOGEN_BUILD_DIR}/include_${CMAKE_BUILD_TYPE}")

target_link_libraries(
	NetworkManager PRIVATE
	HenchmanServiceException
	EventManager
	ServiceHelper
	RegistryManager
	Qt6::Network
	Qt6::Core
)

set_target_properties(
  NetworkManager
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN TRUE
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET NetworkManager PROPERTY CXX_STANDARD 20)
endif()

# set_property(TARGET NetworkManager PROPERTY AUTOMOC ON)


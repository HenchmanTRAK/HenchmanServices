# CMakeList.txt : CMake project for HenchmanServices, include source and define
# project specific logic here.
#

# Add source to this project's executable.

include(generate_product_version)

generate_product_version(
   VersionFilesOutputVariable
   NAME "HenchmanServiceController"
   ICON ${CMAKE_CURRENT_SOURCE_DIR}/henchman_trak_logo_redo.ico
   VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR}
   VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR}
   VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH}
   VERSION_REVISION ${BUILD_REVISION}
   COMPANY_NAME HenchmanTRAK
)

#add_executable(HenchmanServices "HenchmanServices.cpp")
add_executable(HenchmanServiceController "HenchmanServiceController.cpp" "HenchmanServiceController.h" ${VersionFilesOutputVariable})
#qt_disable_unicode_defines(HenchmanService)

target_include_directories(
	HenchmanServiceController PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR}
)


target_link_libraries(
	HenchmanServiceController PRIVATE 
	Qt6::Core
	ServiceHelper
	RegistryManager
	HenchmanServiceException
	EventManager
)

set_target_properties(
	HenchmanServiceController PROPERTIES
	LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator'\""
	AUTOMOC TRUE
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServiceController PROPERTY CXX_STANDARD 20)
endif()

#if (CMAKE_VERSION VERSION_GREATER 2.21)
#  install(FILES $<TARGET_RUNTIME_DLLS:HenchmanServiceController> TYPE BIN)
#endif()

# TODO: Add tests and install targets if needed.
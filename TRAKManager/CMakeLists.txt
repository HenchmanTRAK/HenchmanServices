
add_library(TRAKManager "TRAKManager.cpp")

target_include_directories(TRAKManager PUBLIC "." )
target_link_libraries(
	TRAKManager PRIVATE
	ServiceHelper
	SimpleIni
	RegistryManager
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TRAKManager PROPERTY CXX_STANDARD 20)
endif()
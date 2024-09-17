
qt_add_library(TRAKManager SHARED "TRAKManager.cpp")

target_include_directories(TRAKManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
	TRAKManager PRIVATE
	SimpleIni
	ServiceHelper
	RegistryManager
	DatabaseManager
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TRAKManager PROPERTY CXX_STANDARD 20)
endif()
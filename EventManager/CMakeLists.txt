
add_library(EventManager SHARED "EventManager.cpp")

target_include_directories(
	EventManager PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/resources
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET EventManager PROPERTY CXX_STANDARD 20)
endif()
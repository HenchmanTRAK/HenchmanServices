
#file(
#	REMOVE  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
#	FILES_MATCHING
#	PATTERN "event_messages.dll"
#)

#file(
#	COPY ${MSVC_ROOT_DIR}/CNC_Current_Tracker/CNC_Current_Tracker/x64/Release/
#	DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
#	FILES_MATCHING
#	PATTERN "event_messages.dll"
#	PATTERN bin EXCLUDE
#)

qt_add_library(EventManager SHARED "EventManager.cpp" "EventManager.h" "${CMAKE_SOURCE_DIR}/resources/event_log.dll")
qt_disable_unicode_defines(EventManager)

target_link_libraries(
	EventManager PRIVATE
	ServiceHelper
	RegistryManager
	Qt6::Core
)

target_include_directories(
	EventManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} PUBLIC ${CMAKE_SOURCE_DIR}/resources
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET EventManager PROPERTY CXX_STANDARD 20)
endif()
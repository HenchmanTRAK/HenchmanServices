
qt_add_library(HenchmanServiceException SHARED "HenchmanServiceException.cpp")

target_include_directories(HenchmanServiceException PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServiceException PROPERTY CXX_STANDARD 20)
endif()

add_library(HenchmanServiceException "HenchmanServiceException.cpp")

target_include_directories(HenchmanServiceException PUBLIC ".")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HenchmanServiceException PROPERTY CXX_STANDARD 20)
endif()
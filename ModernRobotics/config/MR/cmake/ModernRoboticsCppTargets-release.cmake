#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ModernRoboticsCpp" for configuration "Release"
set_property(TARGET ModernRoboticsCpp APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(ModernRoboticsCpp PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "/home/lasitha/Documents/bullet-2.89/examples/KUKAEnv/ddp_contact/standalone/ModernRobotics/config/MR/lib/libModernRoboticsCpp.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS ModernRoboticsCpp )
list(APPEND _IMPORT_CHECK_FILES_FOR_ModernRoboticsCpp "/home/lasitha/Documents/bullet-2.89/examples/KUKAEnv/ddp_contact/standalone/ModernRobotics/config/MR/lib/libModernRoboticsCpp.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
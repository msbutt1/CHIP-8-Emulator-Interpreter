include(FetchContent)

# Provide a target named `unity` built from ThrowTheSwitch/Unity
# Unity does not ship with CMake by default; we construct a simple target

if(NOT TARGET unity)
  FetchContent_Declare(
    unity_src
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.6.0
  )

  FetchContent_GetProperties(unity_src)
  if(NOT unity_src_POPULATED)
    FetchContent_Populate(unity_src)
  endif()

  add_library(unity STATIC "${unity_src_SOURCE_DIR}/src/unity.c")
  target_include_directories(unity PUBLIC "${unity_src_SOURCE_DIR}/src")

  # Apply global warnings flags to unity as well
  if(MSVC)
    target_compile_options(unity PRIVATE /W4 /WX)
  else()
    target_compile_options(unity PRIVATE -Wall -Wextra -Werror -pedantic)
  endif()
endif()



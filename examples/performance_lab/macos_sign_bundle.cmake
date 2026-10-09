if(NOT APPLE OR NOT IS_DIRECTORY "${BUNDLE}/Contents/MacOS")
  message(FATAL_ERROR "A built macOS OneUI application bundle is required")
endif()
find_program(lab_codesign codesign REQUIRED)
set(sign_options --timestamp=none)
if(NOT IDENTITY STREQUAL "-")
  set(sign_options --timestamp --options runtime)
endif()
set(frameworks "${BUNDLE}/Contents/Frameworks")
file(MAKE_DIRECTORY "${frameworks}")
foreach(library IN ITEMS "${SDK}" "${OWNER}")
  if(NOT EXISTS "${library}")
    message(FATAL_ERROR "Missing OneUI runtime: ${library}")
  endif()
  get_filename_component(name "${library}" NAME)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${library}" "${frameworks}/${name}"
      COMMAND_ERROR_IS_FATAL ANY)
  # Sign nested code first; --deep is used for verification only.
  execute_process(COMMAND "${lab_codesign}" --force --sign "${IDENTITY}" ${sign_options}
      "${frameworks}/${name}" COMMAND_ERROR_IS_FATAL ANY)
endforeach()
execute_process(COMMAND "${lab_codesign}" --force --sign "${IDENTITY}" ${sign_options}
    --identifier org.oneui.performance-lab "${BUNDLE}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${lab_codesign}" --verify --deep --strict "${BUNDLE}"
    COMMAND_ERROR_IS_FATAL ANY)

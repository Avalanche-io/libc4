# Every function declared in a public C header (include/c4/*.h) must be
# named in tests/c_api_link_test.c, which takes its address so that a
# declaration with no implementation fails to link. Run as:
#   cmake -DINCLUDE_DIR=... -DLINK_TEST=... -P c_api_declared.cmake
file(GLOB headers "${INCLUDE_DIR}/*.h")
file(READ "${LINK_TEST}" link_test)
set(missing "")
foreach(header ${headers})
  file(STRINGS "${header}" lines)
  foreach(line ${lines})
    # A declaration: a line that is not a comment or directive and names a
    # c4-prefixed function followed by an opening parenthesis.
    if(line MATCHES "^[ \t]*(//|/\\*|\\*|#)")
      continue()
    endif()
    string(REGEX MATCHALL "c4[a-z0-9_]*[ \t]*\\(" calls "${line}")
    foreach(call ${calls})
      string(REGEX REPLACE "[ \t]*\\($" "" name "${call}")
      if(NOT link_test MATCHES "\\(any_fn\\)${name},")
        list(APPEND missing "${name} (${header})")
      endif()
    endforeach()
  endforeach()
endforeach()
if(missing)
  list(REMOVE_DUPLICATES missing)
  string(REPLACE ";" "\n  " missing "${missing}")
  message(FATAL_ERROR "declared C functions not covered by c_api_link_test.c:\n  ${missing}")
endif()
message(STATUS "every declared C function is in c_api_link_test.c")

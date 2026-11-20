# Says which CipherBird checkout this build compiles and links against, and
# whether its prebuilt cipherbird_c predates that checkout's last commit.
#
# Sets AVIARY_CIPHERBIRD_REVISION (git describe of CIPHERBIRD_ROOT, "unknown"
# outside a git checkout) and writes it, with the library and its build time,
# to ${CMAKE_BINARY_DIR}/cipherbird_revision.txt. A library older than the
# checkout's last commit is a warning, or an error with
# AVIARY_REQUIRE_FRESH_CIPHERBIRD=ON (the packaging scripts set it).

option(AVIARY_REQUIRE_FRESH_CIPHERBIRD
  "Fail when the cipherbird_c library is older than its checkout's last commit" OFF)

set(AVIARY_CIPHERBIRD_REVISION "unknown")
set(_aviary_cl_commit_time "")
set(_aviary_cl_commit_date "")
find_package(Git QUIET)
if(GIT_EXECUTABLE AND EXISTS "${CIPHERBIRD_ROOT}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${CIPHERBIRD_ROOT}" describe --tags --always --dirty
    OUTPUT_VARIABLE _aviary_cl_describe
    RESULT_VARIABLE _aviary_cl_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(_aviary_cl_rc EQUAL 0 AND _aviary_cl_describe)
    set(AVIARY_CIPHERBIRD_REVISION "${_aviary_cl_describe}")
  endif()
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${CIPHERBIRD_ROOT}" log -1 --format=%ct%n%cI
    OUTPUT_VARIABLE _aviary_cl_log
    RESULT_VARIABLE _aviary_cl_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(_aviary_cl_rc EQUAL 0 AND _aviary_cl_log)
    string(REPLACE "\n" ";" _aviary_cl_log "${_aviary_cl_log}")
    list(GET _aviary_cl_log 0 _aviary_cl_commit_time)
    list(GET _aviary_cl_log 1 _aviary_cl_commit_date)
  endif()
endif()
message(STATUS "aviary: cipherbird revision  = ${AVIARY_CIPHERBIRD_REVISION} (${CIPHERBIRD_ROOT})")

set(_aviary_cl_lines
  "revision ${AVIARY_CIPHERBIRD_REVISION}"
  "root ${CIPHERBIRD_ROOT}"
  "committed ${_aviary_cl_commit_date}")
if(CIPHERBIRD_C_LIBRARY)
  list(GET CIPHERBIRD_C_LIBRARY 0 _aviary_cl_lib)
  file(TIMESTAMP "${_aviary_cl_lib}" _aviary_cl_lib_time "%s")
  file(TIMESTAMP "${_aviary_cl_lib}" _aviary_cl_lib_date "%Y-%m-%dT%H:%M:%SZ" UTC)
  list(APPEND _aviary_cl_lines "library ${_aviary_cl_lib}" "built ${_aviary_cl_lib_date}")
  message(STATUS "aviary: cipherbird_c built   = ${_aviary_cl_lib_date}")
  if(_aviary_cl_commit_time AND _aviary_cl_lib_time AND _aviary_cl_lib_time LESS _aviary_cl_commit_time)
    set(_aviary_cl_stale
      "${_aviary_cl_lib} was built ${_aviary_cl_lib_date}, before the last commit of "
      "${CIPHERBIRD_ROOT} (${_aviary_cl_commit_date}). Rebuild it from that checkout "
      "(scripts/build_selfcontained.sh, or the platform script) so the engine "
      "matches ${AVIARY_CIPHERBIRD_REVISION}.")
    string(CONCAT _aviary_cl_stale ${_aviary_cl_stale})
    if(AVIARY_REQUIRE_FRESH_CIPHERBIRD)
      message(FATAL_ERROR "aviary: ${_aviary_cl_stale}")
    endif()
    message(WARNING "aviary: ${_aviary_cl_stale}")
  endif()
endif()
string(REPLACE ";" "\n" _aviary_cl_text "${_aviary_cl_lines}")
file(WRITE "${CMAKE_BINARY_DIR}/cipherbird_revision.txt" "${_aviary_cl_text}\n")

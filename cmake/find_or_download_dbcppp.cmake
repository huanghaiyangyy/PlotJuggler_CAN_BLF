function(_pj_collect_dbcppp_include_dirs out_var)
  set(_include_dirs "")

  foreach(_hint_var IN ITEMS dbcppp_SOURCE_DIR libdbcppp_SOURCE_DIR FETCHCONTENT_SOURCE_DIR_DBCPPP)
    if(DEFINED ${_hint_var} AND EXISTS "${${_hint_var}}/include/dbcppp/Network.h")
      list(APPEND _include_dirs "${${_hint_var}}/include")
    endif()
  endforeach()

  find_path(DBCPPP_INCLUDE_DIR NAMES dbcppp/Network.h HINTS ${_include_dirs})
  if(DBCPPP_INCLUDE_DIR AND EXISTS "${DBCPPP_INCLUDE_DIR}/dbcppp/Network.h")
    list(APPEND _include_dirs "${DBCPPP_INCLUDE_DIR}")
  endif()

  foreach(_candidate_target IN ITEMS dbcppp::dbcppp libdbcppp dbcppp)
    if(TARGET ${_candidate_target})
      set(_target_for_prop ${_candidate_target})
      get_target_property(_aliased_target ${_candidate_target} ALIASED_TARGET)
      if(_aliased_target)
        set(_target_for_prop ${_aliased_target})
      endif()
      get_target_property(_target_include_dirs ${_target_for_prop} INTERFACE_INCLUDE_DIRECTORIES)
      if(_target_include_dirs AND NOT _target_include_dirs STREQUAL "_target_include_dirs-NOTFOUND")
        list(APPEND _include_dirs ${_target_include_dirs})
      endif()
    endif()
  endforeach()

  list(REMOVE_DUPLICATES _include_dirs)
  set(${out_var} "${_include_dirs}" PARENT_SCOPE)
endfunction()

function(_pj_patch_dbcppp_target_includes_if_missing target_name)
  if(NOT TARGET ${target_name})
    return()
  endif()

  set(_target_for_prop ${target_name})
  get_target_property(_aliased_target ${target_name} ALIASED_TARGET)
  if(_aliased_target)
    set(_target_for_prop ${_aliased_target})
  endif()

  get_target_property(_existing_include_dirs ${_target_for_prop} INTERFACE_INCLUDE_DIRECTORIES)
  if(_existing_include_dirs AND NOT _existing_include_dirs STREQUAL "_existing_include_dirs-NOTFOUND")
    return()
  endif()

  _pj_collect_dbcppp_include_dirs(_fallback_include_dirs)
  if(_fallback_include_dirs)
    set_property(TARGET ${_target_for_prop} PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                                     "${_fallback_include_dirs}")
  endif()
endfunction()

function(_pj_define_dbcppp_interface_target link_target)
  if(TARGET dbcppp::dbcppp)
    return()
  endif()

  add_library(dbcppp::dbcppp INTERFACE IMPORTED)
  set_target_properties(dbcppp::dbcppp PROPERTIES INTERFACE_LINK_LIBRARIES "${link_target}")

  _pj_collect_dbcppp_include_dirs(_dbcppp_include_dirs)
  if(_dbcppp_include_dirs)
    set_property(TARGET dbcppp::dbcppp PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                               "${_dbcppp_include_dirs}")
  endif()
endfunction()

function(find_or_download_dbcppp)
  if(DEFINED PJ_ENABLE_DBCPPP AND NOT PJ_ENABLE_DBCPPP)
    message(STATUS "dbcppp support disabled (PJ_ENABLE_DBCPPP=OFF)")
    set(DBCPPP_FOUND FALSE PARENT_SCOPE)
    return()
  endif()

  if(TARGET dbcppp::dbcppp)
    _pj_patch_dbcppp_target_includes_if_missing(dbcppp::dbcppp)
    message(STATUS "dbcppp target already defined")
    set(DBCPPP_FOUND TRUE PARENT_SCOPE)
    return()
  endif()

  find_package(dbcppp QUIET CONFIG)

  if(TARGET dbcppp::dbcppp)
    _pj_patch_dbcppp_target_includes_if_missing(dbcppp::dbcppp)
    message(STATUS "Found dbcppp in system")
    set(DBCPPP_FOUND TRUE PARENT_SCOPE)
    return()
  endif()

  if(TARGET dbcppp)
    get_target_property(_dbcppp_target_type dbcppp TYPE)
    if(NOT _dbcppp_target_type STREQUAL "EXECUTABLE")
      message(STATUS "Found dbcppp in system")
      _pj_define_dbcppp_interface_target(dbcppp)
      set(DBCPPP_FOUND TRUE PARENT_SCOPE)
      return()
    endif()
  endif()

  if(TARGET libdbcppp)
    message(STATUS "Found libdbcppp in system")
    _pj_define_dbcppp_interface_target(libdbcppp)
    set(DBCPPP_FOUND TRUE PARENT_SCOPE)
    return()
  endif()

  _pj_collect_dbcppp_include_dirs(_dbcppp_include_dirs)
  find_library(DBCPPP_LIBRARY NAMES dbcppp)
  if(NOT DBCPPP_INCLUDE_DIR OR DBCPPP_INCLUDE_DIR STREQUAL "DBCPPP_INCLUDE_DIR-NOTFOUND")
    find_path(DBCPPP_INCLUDE_DIR NAMES dbcppp/Network.h)
  endif()
  if(DBCPPP_LIBRARY AND DBCPPP_INCLUDE_DIR)
    message(STATUS "Found dbcppp via library/header lookup")
    add_library(dbcppp::dbcppp INTERFACE IMPORTED)
    set_target_properties(dbcppp::dbcppp PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${DBCPPP_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${DBCPPP_LIBRARY}")
    set(DBCPPP_FOUND TRUE PARENT_SCOPE)
    return()
  endif()

  message(STATUS "dbcppp not found, downloading")
  set(_dbcppp_cpm_args
    NAME dbcppp
    GIT_REPOSITORY https://github.com/xR3b0rn/dbcppp.git
    GIT_TAG 54ce78ad53442db94e03d16a4fd5f3b339dab193
    GIT_SHALLOW TRUE
    GIT_SUBMODULES
      third-party/cxxopts
      third-party/libxml2
      third-party/libxmlmm
    OPTIONS "build_tests OFF"
            "build_examples OFF"
            "LIBXML2_WITH_PROGRAMS OFF")

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.17")
    list(APPEND _dbcppp_cpm_args GIT_SUBMODULES_RECURSE TRUE)
  else()
    message(STATUS "CMake < 3.17 detected, skipping GIT_SUBMODULES_RECURSE for dbcppp")
  endif()

  cpmaddpackage(${_dbcppp_cpm_args})

  if(TARGET dbcppp::dbcppp)
    _pj_patch_dbcppp_target_includes_if_missing(dbcppp::dbcppp)
    set(DBCPPP_FOUND TRUE PARENT_SCOPE)
  elseif(TARGET libdbcppp)
    _pj_define_dbcppp_interface_target(libdbcppp)
    set(DBCPPP_FOUND TRUE PARENT_SCOPE)
  elseif(TARGET dbcppp)
    get_target_property(_dbcppp_target_type dbcppp TYPE)
    if(NOT _dbcppp_target_type STREQUAL "EXECUTABLE")
      _pj_define_dbcppp_interface_target(dbcppp)
      set(DBCPPP_FOUND TRUE PARENT_SCOPE)
    else()
      message(WARNING "dbcppp target exists but is an executable; libdbcppp target not found")
    endif()
  else()
    message(WARNING "dbcppp download completed but no known CMake target was found")
  endif()

endfunction()

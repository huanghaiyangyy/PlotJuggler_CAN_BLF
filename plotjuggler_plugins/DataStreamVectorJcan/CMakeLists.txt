include_directories(../ DataLoadBLF)

add_library(DataStreamVectorJcan SHARED
  datastream_vector_jcan.cpp
  blf_recorder.cpp
)

target_link_libraries(DataStreamVectorJcan PRIVATE
  Qt5::Widgets
  Qt5::Xml
  plotjuggler_base
  jcan_vector::jcan_vector
  blf_core
  blf_adapters
)

target_compile_definitions(DataStreamVectorJcan PRIVATE QT_PLUGIN)
if(TARGET vector_blf::vector_blf)
  target_compile_definitions(DataStreamVectorJcan PRIVATE PJ_HAS_VECTOR_BLF=1)
  target_link_libraries(DataStreamVectorJcan PRIVATE vector_blf::vector_blf)
endif()
if(TARGET dbcppp::dbcppp)
  target_compile_definitions(DataStreamVectorJcan PRIVATE PJ_HAS_DBCPPP=1)
endif()

target_compile_features(DataStreamVectorJcan PRIVATE cxx_std_17)

install(TARGETS DataStreamVectorJcan DESTINATION ${PJ_PLUGIN_INSTALL_DIRECTORY})

option(PJ_BUILD_VECTOR_JCAN_SMOKE "Build BLF write/read smoke tool" ON)
if(PJ_BUILD_VECTOR_JCAN_SMOKE AND TARGET vector_blf::vector_blf)
  add_executable(vector_jcan_blf_smoke tools/blf_write_smoke.cpp blf_recorder.cpp)
  target_include_directories(vector_jcan_blf_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../DataLoadBLF)
  target_link_libraries(vector_jcan_blf_smoke PRIVATE
    Qt5::Core
    blf_core
    blf_adapters
    vector_blf::vector_blf)
  target_compile_features(vector_jcan_blf_smoke PRIVATE cxx_std_17)
endif()

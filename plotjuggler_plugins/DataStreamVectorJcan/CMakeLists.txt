include_directories(../)

add_library(DataStreamVectorJcan SHARED
  datastream_vector_jcan.cpp
)

target_link_libraries(DataStreamVectorJcan PRIVATE
  Qt5::Widgets
  Qt5::Xml
  plotjuggler_base
  jcan_vector::jcan_vector
)

target_compile_definitions(DataStreamVectorJcan PRIVATE QT_PLUGIN)
target_compile_features(DataStreamVectorJcan PRIVATE cxx_std_17)

install(TARGETS DataStreamVectorJcan DESTINATION ${PJ_PLUGIN_INSTALL_DIRECTORY})

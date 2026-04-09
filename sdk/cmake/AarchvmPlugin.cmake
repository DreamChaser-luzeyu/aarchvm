function(aarchvm_add_plugin target)
  add_library(${target} MODULE ${ARGN})
  target_link_libraries(${target} PRIVATE aarchvm_plugin_sdk_headers)
  set_target_properties(${target} PROPERTIES
    PREFIX ""
    POSITION_INDEPENDENT_CODE ON
  )
endfunction()

aarchvm_add_plugin(aarchvm_bad_abi bad_abi.c)
aarchvm_add_plugin(aarchvm_bad_manifest bad_manifest.c)

add_library(aarchvm_no_entry MODULE no_entry.c)
set_target_properties(aarchvm_no_entry PROPERTIES
  PREFIX ""
  POSITION_INDEPENDENT_CODE ON
)

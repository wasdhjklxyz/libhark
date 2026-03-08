function(target_set_sanitizers target)
  if(HARK_ASAN)
    target_compile_options(${target} PUBLIC
      -fsanitize=address,undefined
      -fno-omit-frame-pointer
    )
    target_link_options(${target} PUBLIC
      -fsanitize=address,undefined
    )
  endif()
endfunction()

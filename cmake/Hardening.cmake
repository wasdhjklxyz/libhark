function(target_set_hardening target)
  target_compile_options(${target} PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:
      -fstack-protector-strong
      -fPIE
    >
    $<$<AND:$<C_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:
      -D_FORTIFY_SOURCE=2
    >
  )
  target_link_options(${target} PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:
      -fstack-protector-strong
    >
  )
endfunction()

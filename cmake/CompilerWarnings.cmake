function(target_set_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,GNU>:
      -Wall -Wextra -Wpedantic
      -Wshadow -Wconversion -Wdouble-promotion
      -Wformat=2 -Wformat-security
      -Wnull-dereference -Wundef
      -Wno-unused-parameter
    >
    $<$<COMPILE_LANG_AND_ID:C,Clang>:
      -Wall -Wextra -Wpedantic
      -Wshadow -Wconversion -Wdouble-promotion
      -Wformat=2 -Wundef
      -Wno-unused-parameter
    >
  )
endfunction()

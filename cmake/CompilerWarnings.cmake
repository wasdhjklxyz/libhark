function(target_set_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<C_COMPILER_ID:GNU>:
      -Wall -Wextra -Wpedantic
      -Wshadow -Wconversion -Wdouble-promotion
      -Wformat=2 -Wformat-security
      -Wnull-dereference
      -Wno-unused-parameter
    >
    $<$<C_COMPILER_ID:Clang>:
      -Wall -Wextra -Wpedantic -Werror
      -Wshadow -Wconversion -Wdouble-promotion
      -Wformat=2
      -Wno-unused-parameter
    >
  )
endfunction()

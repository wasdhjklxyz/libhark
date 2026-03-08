function(add_hark_example example_name)
  add_executable(${example_name} ${example_name}.c)
  target_link_libraries(${example_name} PRIVATE hark::hark)
endfunction()

add_hark_example(echo_server)
add_hark_example(reconnect_demo)

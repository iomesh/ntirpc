
SET(rpcping_SRCS
  rpcping.c
  )
add_executable(rpcping ${rpcping_SRCS})
target_link_libraries(rpcping ntirpc
  ${BINARY_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${LTTNG_LIBRARIES}
  -ldl)

add_executable(module_observation module_observation.c ../src/svc_observe.c)
target_compile_options(module_observation PRIVATE -UNDEBUG -Wall -Wextra -Werror)

target_link_libraries(module_observation "-Wl,--wrap=clock_gettime")

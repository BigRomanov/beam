# костыль временно
if ($ENV{USER} STREQUAL "artem")

add_executable(bbs_test bbs_test.cpp)
add_dependencies(bbs_test bbs_client)
target_link_libraries(bbs_test bbs_client)

add_executable(ds dht_sandbox.cpp)
target_include_directories(ds PRIVATE $ENV{HOME}/include)

add_dependencies(ds p2p)
target_link_libraries(ds p2p $ENV{HOME}/lib/libopendht.so)

endif()

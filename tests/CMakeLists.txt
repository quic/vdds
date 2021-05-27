add_executable(topic-test test-skell.hpp topic-test.cc)
target_link_libraries(topic-test boost_program_options vdds)
add_test(NAME topic COMMAND topic-test)

add_executable(domain-test test-skell.hpp domain-test.cc)
target_link_libraries(domain-test boost_program_options vdds)
add_test(NAME domain COMMAND domain-test)

add_executable(ping-test test-skell.hpp ping-test.cc)
target_link_libraries(ping-test boost_program_options vdds)
add_test(NAME ping COMMAND ping-test)

add_executable(shared-test test-skell.hpp shared-test.cc)
target_link_libraries(shared-test boost_program_options vdds)
add_test(NAME shared COMMAND shared-test)

add_executable(multi-test test-skell.hpp multi-test.cc)
target_link_libraries(multi-test boost_program_options vdds)
add_test(NAME multi COMMAND multi-test)

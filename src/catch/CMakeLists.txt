# Unit tests using Catch2

enable_testing()

add_executable(recoll_tests
    catchtests.cpp
    testmime.cpp
    test_smallut.cpp
    test_base64.cpp
    test_conftree.cpp
    test_pathut.cpp
)

target_include_directories(recoll_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(recoll_tests PRIVATE librecoll)

add_test(NAME recoll_tests COMMAND recoll_tests)

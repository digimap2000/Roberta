include(GoogleTest)

set(ROBERTA_GTEST_ROOT "/opt/homebrew/opt/googletest" CACHE PATH "Preferred GoogleTest installation root")

find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND AND EXISTS "${ROBERTA_GTEST_ROOT}/lib/cmake/GTest/GTestConfig.cmake")
    find_package(GTest CONFIG REQUIRED PATHS "${ROBERTA_GTEST_ROOT}/lib/cmake/GTest" NO_DEFAULT_PATH)
endif()

if(NOT GTest_FOUND)
    find_package(GTest CONFIG REQUIRED)
endif()

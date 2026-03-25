include(RobertaTesting)

add_subdirectory(unit)

if(ROBERTA_ENABLE_HIL_TESTS)
    add_subdirectory(hil)
endif()

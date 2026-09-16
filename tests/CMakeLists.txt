find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(MIDNAFX_BUILD_MOD)
    add_test(NAME package_contract COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/package_contract.py"
        "${CMAKE_BINARY_DIR}/mods/midnafx.dusk")
endif()

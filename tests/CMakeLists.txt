find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_executable(grade_math grade_math.cpp ../src/config/grade.cpp)
target_compile_features(grade_math PRIVATE cxx_std_20)
target_include_directories(grade_math PRIVATE ../src)
add_test(NAME grade_math COMMAND grade_math)
if(MIDNAFX_BUILD_MOD)
    add_test(NAME package_contract COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/package_contract.py"
        "${CMAKE_BINARY_DIR}/mods/midnafx.dusk")
endif()

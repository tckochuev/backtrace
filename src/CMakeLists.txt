set(TARGET_NAME ${PROJECT_NAME})
add_executable(${TARGET_NAME} main.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE dbghelp.lib)
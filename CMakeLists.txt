cmake_minimum_required(VERSION 3.16)
project(tchipng C)

add_library(tchipng_shared SHARED tchipng.c)
target_include_directories(tchipng_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tchipng_shared PUBLIC z)
set_target_properties(tchipng_shared PROPERTIES OUTPUT_NAME tchipng)

add_library(tchipng_static STATIC tchipng.c)
target_include_directories(tchipng_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tchipng_static PUBLIC z)
set_target_properties(tchipng_static PROPERTIES OUTPUT_NAME tchipng)

add_executable(tchipng tchipng_cli.c)
target_include_directories(tchipng PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(tchipng PRIVATE tchipng_static)
set_target_properties(tchipng PROPERTIES OUTPUT_NAME tchipng)

# pip 安装时由 scikit-build-core 定义。Android / iOS / 鸿蒙 cmake 不走这里。
if(DEFINED SKBUILD_PLATLIB_DIR)
  if(CMAKE_VERSION VERSION_LESS "3.31")
    message(FATAL_ERROR "pip install of tchipng needs CMake >= 3.31")
  endif()
  install(TARGETS tchipng_shared
    LIBRARY DESTINATION "${SKBUILD_PLATLIB_DIR}/tchipng"
    RUNTIME DESTINATION "${SKBUILD_PLATLIB_DIR}/tchipng"
  )
  install(PROGRAMS "$<TARGET_FILE:tchipng>"
    DESTINATION "${SKBUILD_PLATLIB_DIR}/tchipng"
    RENAME "_tchipng_cli"
  )
endif()

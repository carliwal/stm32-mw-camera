cmake_minimum_required(VERSION 3.22)

project(STM32_MW_Camera LANGUAGES C)
add_library(stm32_mw_camera INTERFACE)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined libraries
)


target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC 
  ./
  ./sensors
  ./sensors/vd55g0
  ./sensors/vd55g1
  ./sensors/imx335
  ./sensors/ov5640
  ./sensors/vd6g
  ./sensors/vd1943
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  ./cmw_camera.c
  ./cmw_utils.c

  ./sensors/cmw_vd55g0.c
  ./sensors/cmw_vd55g1.c
  ./sensors/vd55g0/vd55g0.c
  ./sensors/vd55g0/vd55g0_patch_cut_2.c
  ./sensors/vd55g1/vd55g1.c
  ./sensors/vd55g1/vd55g1_patch_cut_2.c
)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
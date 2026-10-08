##                                                                                                             
##        ██               ▄▄                          
## ▄█▀▀▀ ▀██▀▀ ▄███▄ ▄████ ██ ▄█▀ ███▄███▄  ▀▀█▄ ████▄ 
## ▀███▄  ██   ██ ██ ██    ████   ██ ██ ██ ▄█▀██ ██ ██ 
## ▄▄▄█▀  ██   ▀███▀ ▀████ ██ ▀█▄ ██ ██ ██ ▀█▄██ ██ ██    
## 
## Copyright 2026 doublegsoft.me
## 
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
## 
##     http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.                                                                    
##
cmake_minimum_required(VERSION 3.16)

project(stockman LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (WIN32)
  set(PLATFORM                        "mingw")
elseif (APPLE)
  set(PLATFORM                        "darwin")
elseif (UNIX)
  set(PLATFORM                        "linux")
endif()
set(BUILD                             "build/${PLATFORM}")

################################################################################
##                                                                            ##
##                                    IMGUI                                   ##
##                                                                            ##
################################################################################
set(IMGUI_DIR                         "${CMAKE_CURRENT_SOURCE_DIR}/3rd/imgui-1.92.9")
set(IMGUI_INCLUDE_DIR                 "${IMGUI_DIR}")
set(IMGUI_LIBRARY_DIR                 "${IMGUI_DIR}/${BUILD}")
set(IMGUI_LIBRARIES                   libargparse.a)

set(STOCKMAN_SOURCES
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_osx.mm
  ${IMGUI_DIR}/backends/imgui_impl_metal4.mm
)

add_executable(${PROJECT_NAME}
  src/main.mm
  ${STOCKMAN_SOURCES}
)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wno-deprecated-declarations
  -Wno-arc-bridge-casts-disallowed-in-nonarc
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
  src
  ${IMGUI_INCLUDE_DIR}
  ${IMGUI_INCLUDE_DIR}/backends
)

# Link dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
  "-framework Cocoa"
  "-framework Metal"
  "-framework MetalKit"
  "-framework QuartzCore"
  "-framework GameController"
)

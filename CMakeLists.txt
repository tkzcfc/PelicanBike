# /****************************************************************************
# Copyright (c) 2013-2014 cocos2d-x.org
# Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).
#
# https://axmol.dev/
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# ****************************************************************************/

cmake_minimum_required(VERSION 3.22...4.1)

set(APP_NAME PelicanBike)

project(${APP_NAME})

if(XCODE)
  set(CMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY TRUE)
endif()

set(_is_axmol_embed FALSE)
set(_AX_USE_PREBUILT FALSE)

set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
)

# Render backend: our 3D layer (Source/gfx) is raw OpenGL, so every platform must get a GL context.
#  - Windows: native desktop GL 3.3 core (axmol default would be ANGLE/GLES)
#  - macOS/iOS: OpenGL instead of Metal (axmol default is Metal)
#  - Android: GLES 3.0 (axmol default profile is 2.0)
#  - Linux: desktop GL 3.3 core (default); WASM: WebGL2 / GLES 3.0 (default)
if(WIN32 AND NOT WINRT)
  set(AX_USE_COMPAT_GL OFF CACHE BOOL "PelicanBike: native desktop GL" FORCE)
elseif(APPLE)
  set(AX_USE_COMPAT_GL ON CACHE BOOL "PelicanBike: OpenGL instead of Metal" FORCE)
endif()
if(ANDROID)
  set(AX_GLES_PROFILE 300 CACHE STRING "PelicanBike: GLES 3.0" FORCE)
endif()

# NOTE: The order of the cmake module "include(AXGame...)" statements matters
include(AXGameEngineOptions)
include(AXGameEngineSetup)

# The common cross-platforms source files and header files
file(GLOB_RECURSE GAME_HEADER
  Source/*.h Source/*.hpp
)

file(GLOB_RECURSE GAME_SOURCE
  Source/*.cpp Source/*.c
)

set(GAME_INC_DIRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

set(content_folder
  "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)

if(APPLE)
  ax_mark_multi_resources(common_content_files RES_TO "Resources" FOLDERS ${content_folder})
elseif(WINDOWS)
  ax_mark_multi_resources(common_content_files RES_TO "Content" FOLDERS ${content_folder})
endif()

include(AXGameSourceSetup)

# mark app complie info and libs info
set(APP_SOURCES
  ${GAME_HEADER}
  ${GAME_SOURCE}
)

if(LINUX OR MACOSX OR WINDOWS)
  option(_AX_TESTS "Enable doctest" OFF)
endif()

if(_AX_TESTS)
  set(TEST_TARGET_NAME "${APP_NAME}_test")

  file(GLOB_RECURSE TEST_HEADER CONFIGURE_DEPENDS
    Tests/*.h Tests/*.hpp
  )
  list(APPEND TEST_HEADER ${GAME_HEADER})

  file(GLOB_RECURSE TEST_SOURCE CONFIGURE_DEPENDS
    Tests/*.cpp Tests/*.c
  )
  list(APPEND TEST_SOURCE ${GAME_SOURCE})

  set(TEST_TARGET_SOURCES
    ${APP_SOURCES}
    ${TEST_HEADER}
    ${TEST_SOURCE}
  )

 set(TEST_INC_DIRS
   "${CMAKE_CURRENT_SOURCE_DIR}/Tests"
   "${CMAKE_CURRENT_SOURCE_DIR}/Source"
 )
endif()

include(AXGameTargetSetup)

if(MSVC)
  # UTF-8 sources (Chinese UI strings), M_PI
  target_compile_options(${APP_NAME} PRIVATE /utf-8 /bigobj)
  target_compile_definitions(${APP_NAME} PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# mark app resources, resource will be copy auto after mark
ax_setup_app_config(${APP_NAME})

if(_AX_TESTS)
  ax_setup_app_config(${TEST_TARGET_NAME} CONSOLE)
endif()

# Add any libraries you need to link to the project after this point

# Default Platform-specific setup
include(AXGamePlatformSetup)

# Make sure the AXGameFinalSetup is included at the end of this file
include(AXGameFinalSetup)

if(WASM)
  # no html-minifier in this emsdk install; also WebGL2 only
  target_link_options(${APP_NAME} PRIVATE -sMINIFY_HTML=0 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2)
endif()

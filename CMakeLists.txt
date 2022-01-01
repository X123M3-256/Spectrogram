cmake_minimum_required (VERSION 3.1)

if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif()

project (spectrogram C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk+-3.0)


add_executable(spectrogram src/main.c src/render.c src/fft.c src/fftsg.c)
target_include_directories(spectrogram PRIVATE ${GTK_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR}/Headers)
target_link_libraries(spectrogram PRIVATE m ${GTK_LIBRARIES})
set_property(TARGET spectrogram PROPERTY C_STANDARD 99)


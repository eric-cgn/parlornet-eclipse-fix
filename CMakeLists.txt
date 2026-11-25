# Build with the Retro68 toolchain (see build.sh):
#   cmake -S . -B build/mac -DCMAKE_TOOLCHAIN_FILE=<Retro68>/m68k-apple-macos/cmake/retro68.toolchain.cmake
#   cmake --build build/mac
cmake_minimum_required(VERSION 3.13)
project(EclipseFix C)

set(APP_NAME "Eclipse 68k Fix")
set(APP_CREATOR "EcFx")

add_executable(EclipseFix src/main.c src/patch.c)
target_compile_options(EclipseFix PRIVATE -Os -Wall -Wno-multichar -Wno-array-bounds)
set_target_properties(EclipseFix PROPERTIES OUTPUT_NAME EclipseFix.code.bin)

set(REZ_INC)
foreach(p ${REZ_INCLUDE_PATH})
	list(APPEND REZ_INC -I${p})
endforeach()

add_custom_command(
	OUTPUT EclipseFix.rsrc.bin
	COMMAND ${REZ} ${CMAKE_SOURCE_DIR}/src/EclipseFix.r ${REZ_INC} -o EclipseFix.rsrc.bin
	DEPENDS ${CMAKE_SOURCE_DIR}/src/EclipseFix.r)

# The application: MacBinary (.bin) and a small HFS disk image (.dsk) that holds it.
add_custom_command(
	OUTPUT "${APP_NAME}.bin" "${APP_NAME}.dsk"
	COMMAND ${REZ} --copy EclipseFix.code.bin EclipseFix.rsrc.bin
		-o "${APP_NAME}.bin" --cc "${APP_NAME}.dsk"
		-t APPL -c ${APP_CREATOR}
	DEPENDS EclipseFix EclipseFix.rsrc.bin)
add_custom_target(EclipseFix_APPL ALL DEPENDS "${APP_NAME}.bin")

# Emu48/SDL, the SDL3 port of Emu48 for macOS and Linux
#
# cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
# (INSTALL.TXT describes the build with a static SDL3 library)

cmake_minimum_required(VERSION 3.16)
project(Emu48 C)

# version of Emu48/SDL: Emu48 version and number of the port release, tag v<version>
set(EMU48SDL_VERSION 1.68.1)
set(EMU48SDL_URL https://github.com/FabianHassler/Emu48-SDL)

find_package(SDL3 3.2 REQUIRED CONFIG)

set(EMU48_SOURCES
	COMMANDS.C DEBUGGER.C DISASM.C DISMEM.C DISRPL.C DISPLAY.C ENGINE.C FETCH.C FILES.C
	I28F160.C KEYBOARD.C KEYMACRO.C KML.C LODEPNG.C LOWBAT.C MOPS.C MRU.C
	OPCODES.C PNGCRC.C REDEYE.C ROMCRC.C RPL.C SERIAL.C SETTINGS.C SNDENUM.C SOUND.C STACK.C
	SYMBFILE.C TIMER.C UDP.C
	SDLBAUD.C SDLDLG.C SDLGDI.C SDLHELP.C SDLMAIN.C SDLMENU.C SDLNK.C SDLOS.C SDLUSER.C SDLWAVE.C)

list(TRANSFORM EMU48_SOURCES PREPEND src/)
set_source_files_properties(${EMU48_SOURCES} PROPERTIES LANGUAGE C)

add_executable(emu48 ${EMU48_SOURCES})

# the sources include their headers in lower case or capitalized
# (io.h, Emu48.h), provide both on case sensitive file systems
file(GLOB EMU48_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src src/*.H)
foreach(HEADER ${EMU48_HEADERS})
	string(TOLOWER ${HEADER} LOWER)
	string(SUBSTRING ${LOWER} 0 1 FIRST)
	string(SUBSTRING ${LOWER} 1 -1 REST)
	string(TOUPPER ${FIRST} FIRST)
	configure_file(src/${HEADER} ${CMAKE_CURRENT_BINARY_DIR}/include/${LOWER} COPYONLY)
	configure_file(src/${HEADER} ${CMAKE_CURRENT_BINARY_DIR}/include/${FIRST}${REST} COPYONLY)
endforeach()
target_include_directories(emu48 PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/initguid.h "") # Windows SDK header of SndDef.h

# the menus, the accelerators, the dialogs, the debugger toolbar, the strings and the bitmaps
# of the Windows resources, compiled by SdlMenu.c, SdlDlg.c and SdlUser.c, the arguments of
# CONTROL statements in the order of the other controls, fixed pitch dialog fonts as FONTFIXED
file(READ src/EMU48.RC EMU48_RC)
string(REGEX MATCH "\n// Menu\n//\n\n(.*)\n+#ifdef APSTUDIO_INVOKED\n/+\n//\n// TEXTINCLUDE" EMU48_MENU "${EMU48_RC}")
string(REGEX REPLACE "(^|\n)([A-Z_0-9]+) MENU [^\n]*" "\\1MENUID(\\2)" EMU48_MENU "${CMAKE_MATCH_1}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/menu.rc "${EMU48_MENU}")
string(REGEX MATCH "\n// Dialog\n//\n\n(.*)\n/+\n//\n// Bitmap" EMU48_DLG "${EMU48_RC}")
string(REGEX REPLACE "\nFONT [^\n]*\"Courier New\"" "\nFONTFIXED" EMU48_DLG "${CMAKE_MATCH_1}")
string(REGEX REPLACE "\nFONT [^\n]*" "" EMU48_DLG "${EMU48_DLG}")
string(REGEX REPLACE "CONTROL +(\"[^\"]*\"),[ \n]*([A-Za-z_0-9]+),[ \n]*(\"[^\"]*\"),([^,]+),([^,]+),([^,]+),([^,]+),([0-9]+)"
	"CONTROL \\1,\\2,\\5,\\6,\\7,\\8,\\4,\\3" EMU48_DLG "${EMU48_DLG}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/dialog.rc "${EMU48_DLG}")
string(REGEX MATCH "\nIDR_MENU ACCELERATORS[^\n]*\nBEGIN\n(( [^\n]*\n)*)END" EMU48_ACC "${EMU48_RC}")
string(REGEX REPLACE "([^\n]+)" "{ \\1 }," EMU48_ACC "${CMAKE_MATCH_1}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/accel.rc "${EMU48_ACC}")
string(REGEX MATCH "\nIDR_DEBUG_TOOLBAR TOOLBAR [A-Z ]*([0-9]+), *([0-9]+)\nBEGIN\n(( [^\n]*\n)*)END" EMU48_TB "${EMU48_RC}")
set(EMU48_TB "${CMAKE_MATCH_1}, ${CMAKE_MATCH_2},\n${CMAKE_MATCH_3}") # button size and buttons
string(REGEX REPLACE " +BUTTON +([A-Z_0-9]+)" "\\1," EMU48_TB "${EMU48_TB}")
string(REGEX REPLACE " +SEPARATOR" "0," EMU48_TB "${EMU48_TB}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/toolbar.rc "${EMU48_TB}")
string(REGEX MATCH "\nSTRINGTABLE[^\n]*\nBEGIN\n(( [^\n]*\n)*)END" EMU48_STR "${EMU48_RC}")
string(REGEX REPLACE " +([A-Z_0-9]+) +(\"[^\n]*\")" "{ \\1, _T(\\2) }," EMU48_STR "${CMAKE_MATCH_1}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/string.rc "${EMU48_STR}")
string(REGEX MATCHALL "\n[A-Z_0-9]+ +BITMAP +[A-Z ]*\"[^\"]+\"" EMU48_BMPS "${EMU48_RC}")
set(EMU48_BMP_RC "")
set(EMU48_BMP_TABLE "")
foreach(BMP ${EMU48_BMPS})
	string(REGEX MATCH "([A-Z_0-9]+) +BITMAP +[A-Z ]*\"([^\"]+)\"" BMP "${BMP}")
	set(BMP_ID ${CMAKE_MATCH_1})
	string(TOUPPER ${CMAKE_MATCH_2} BMP_FILE)
	set(BMP_FILE src/${BMP_FILE})
	file(READ ${BMP_FILE} BMP_DATA HEX)
	string(REGEX REPLACE "(..)" "0x\\1," BMP_DATA "${BMP_DATA}")
	string(APPEND EMU48_BMP_RC "static const BYTE by${BMP_ID}[] = { ${BMP_DATA} };\n")
	string(APPEND EMU48_BMP_TABLE "{ ${BMP_ID}, by${BMP_ID} },")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BMP_FILE})
endforeach()
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/bitmap.rc "${EMU48_BMP_RC}#define BITMAP_TABLE ${EMU48_BMP_TABLE}\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS src/EMU48.RC)

# the manual doc/MANUAL.md for the help window of SdlHelp.c
file(READ doc/MANUAL.md EMU48_MANUAL HEX)
string(REGEX REPLACE "(..)" "0x\\1," EMU48_MANUAL "${EMU48_MANUAL}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/manual.rc "${EMU48_MANUAL}0x00\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS doc/MANUAL.md)

target_compile_definitions(emu48 PRIVATE
	LODEPNG_NO_COMPILE_ENCODER LODEPNG_NO_COMPILE_DISK
	LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS LODEPNG_NO_COMPILE_ERROR_TEXT
	LODEPNG_NO_COMPILE_CPP LODEPNG_NO_COMPILE_CRC
	"EMU48SDL_VERSION=\"${EMU48SDL_VERSION}\"" "EMU48SDL_URL=\"${EMU48SDL_URL}\""
	$<$<CONFIG:Debug>:_DEBUG>)

target_compile_options(emu48 PRIVATE
	-x c -fno-strict-aliasing
	-Wall -Wno-missing-braces -Wno-format -Wno-uninitialized
	-Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-local-typedefs -Wno-unknown-pragmas -Wno-nonportable-include-path)

set_source_files_properties(src/SDLNK.C PROPERTIES COMPILE_OPTIONS -w) # Nuklear library
set_source_files_properties(src/SDLDLG.C PROPERTIES COMPILE_OPTIONS $<$<C_COMPILER_ID:AppleClang,Clang>:-Wno-invalid-source-encoding>) # Latin-1 Emu48.rc

target_link_libraries(emu48 PRIVATE SDL3::SDL3)

# MkShared, the RAM card maker for port 2 of the HP48SX/GX (command line)
add_executable(mkshared src/MKSHARED/MKSHARED.C)
set_source_files_properties(src/MKSHARED/MKSHARED.C PROPERTIES LANGUAGE C)
target_compile_options(mkshared PRIVATE -x c)

# small release binaries: optimized for size, unused functions removed, no symbols,
# SDL3 is linked statically when only the static library is found
set(CMAKE_C_FLAGS_RELEASE "-Os -ffunction-sections -fdata-sections")
set_property(TARGET emu48 PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
foreach(TARGET emu48 mkshared)
	target_link_options(${TARGET} PRIVATE $<$<CONFIG:Release>:$<IF:$<PLATFORM_ID:Darwin>,-Wl$<COMMA>-dead_strip$<COMMA>-x$<COMMA>-S,-Wl$<COMMA>--gc-sections$<COMMA>-s>>)
endforeach()

# macOS: Emu48.app with the icon of Emu48.ico
if(APPLE)
	add_custom_command(OUTPUT Emu48.icns
		COMMAND sips -s format icns ${CMAKE_CURRENT_SOURCE_DIR}/src/EMU48.ICO --out Emu48.icns >/dev/null
		DEPENDS src/EMU48.ICO)
	target_sources(emu48 PRIVATE Emu48.icns)
	set_source_files_properties(Emu48.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	set_target_properties(emu48 PROPERTIES MACOSX_BUNDLE TRUE OUTPUT_NAME Emu48
		MACOSX_BUNDLE_BUNDLE_NAME Emu48 MACOSX_BUNDLE_GUI_IDENTIFIER Emu48
		MACOSX_BUNDLE_ICON_FILE Emu48.icns MACOSX_BUNDLE_SHORT_VERSION_STRING ${EMU48SDL_VERSION}
		MACOSX_BUNDLE_COPYRIGHT "GPL-2.0-or-later")
endif()

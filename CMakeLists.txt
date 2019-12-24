set(GAMBATTE_DIR "${PROJECT_SOURCE_DIR}/gambatte-libretro/libgambatte/src")
set(LIBRETRO_COMM_DIR ${GAMBATTE_DIR}/../libretro-common)

set(DEBUGGER_SRC
    ${GAMBATTE_DIR}/debugger/Breakpoint.cpp
    ${GAMBATTE_DIR}/debugger/Buffer.cpp
    ${GAMBATTE_DIR}/debugger/Debugger.cpp
    ${GAMBATTE_DIR}/debugger/GdbConnection.cpp
    ${GAMBATTE_DIR}/debugger/GdbStub.cpp
    ${GAMBATTE_DIR}/debugger/StopReason.cpp
)

SOURCE_GROUP(debugger FILES ${DEBUGGER_SRC})

set(DEBUGGER_HDR
    ${GAMBATTE_DIR}/debugger/Breakpoint.h
    ${GAMBATTE_DIR}/debugger/Buffer.h
    ${GAMBATTE_DIR}/debugger/Debugger.h
    ${GAMBATTE_DIR}/debugger/GdbConnection.h
    ${GAMBATTE_DIR}/debugger/GdbStub.h
    ${GAMBATTE_DIR}/debugger/StopReason.h
)

SOURCE_GROUP(debugger FILES ${DEBUGGER_HDR})

set(MEM_SRC
    ${GAMBATTE_DIR}/mem/cartridge.cpp 
    ${GAMBATTE_DIR}/mem/cartridge_libretro.cpp 
    ${GAMBATTE_DIR}/mem/huc3.cpp 
    ${GAMBATTE_DIR}/mem/memptrs.cpp 
    ${GAMBATTE_DIR}/mem/rtc.cpp 
)

SOURCE_GROUP(mem FILES ${MEM_SRC})

set(SOUND_SRC
    ${GAMBATTE_DIR}/sound/channel1.cpp 
    ${GAMBATTE_DIR}/sound/channel2.cpp 
    ${GAMBATTE_DIR}/sound/channel3.cpp 
    ${GAMBATTE_DIR}/sound/channel4.cpp 
    ${GAMBATTE_DIR}/sound/duty_unit.cpp 
    ${GAMBATTE_DIR}/sound/envelope_unit.cpp 
    ${GAMBATTE_DIR}/sound/length_counter.cpp 
)

SOURCE_GROUP(sound FILES ${SOUND_SRC})

set(VIDEO_SRC
    ${GAMBATTE_DIR}/video/ly_counter.cpp 
    ${GAMBATTE_DIR}/video/lyc_irq.cpp 
    ${GAMBATTE_DIR}/video/next_m0_time.cpp 
    ${GAMBATTE_DIR}/video/ppu.cpp 
    ${GAMBATTE_DIR}/video/sprite_mapper.cpp 
)

SOURCE_GROUP(video FILES ${VIDEO_SRC})

set(RETRO_SRC
    ${GAMBATTE_DIR}/../libretro/blipper.c
    ${GAMBATTE_DIR}/../libretro/libretro.cpp
    ${LIBRETRO_COMM_DIR}/streams/file_stream.c 
    ${LIBRETRO_COMM_DIR}/vfs/vfs_implementation.c 
    ${LIBRETRO_COMM_DIR}/compat/fopen_utf8.c 
    ${LIBRETRO_COMM_DIR}/compat/compat_strl.c 
    ${LIBRETRO_COMM_DIR}/encodings/encoding_utf.c
)

SOURCE_GROUP(retro FILES ${RETRO_SRC})

set(MAIN_SRC
    ${GAMBATTE_DIR}/easylogging++.cc
    ${GAMBATTE_DIR}/bootloader.cpp 
    ${GAMBATTE_DIR}/cpu.cpp 
    ${GAMBATTE_DIR}/gambatte.cpp 
    ${GAMBATTE_DIR}/initstate.cpp 
    ${GAMBATTE_DIR}/interrupter.cpp 
    ${GAMBATTE_DIR}/interruptrequester.cpp 
    ${GAMBATTE_DIR}/gambatte-memory.cpp 
    ${GAMBATTE_DIR}/sound.cpp 
    ${GAMBATTE_DIR}/statesaver.cpp 
    ${GAMBATTE_DIR}/tima.cpp 
    ${GAMBATTE_DIR}/video.cpp 
    ${GAMBATTE_DIR}/video_libretro.cpp 
)

set(MAIN_HDR
    ${GAMBATTE_DIR}/easylogging++.h
    ${GAMBATTE_DIR}/bootloader.h 
    ${GAMBATTE_DIR}/cpu.h
    ${GAMBATTE_DIR}/initstate.h 
    ${GAMBATTE_DIR}/interrupter.h 
    ${GAMBATTE_DIR}/interruptrequester.h 
    ${GAMBATTE_DIR}/gambatte-memory.h 
    ${GAMBATTE_DIR}/sound.h 
    ${GAMBATTE_DIR}/statesaver.h 
    ${GAMBATTE_DIR}/tima.h 
    ${GAMBATTE_DIR}/video.h
)

set_property(GLOBAL PROPERTY USE_FOLDERS On) 

# set_source_files_properties(${GAMBATTE_SRC} PROPERTIES LANGUAGE CXX)

message(STATUS "gambatte_srcs: ${GAMBATTE_SRC}")

add_library(gambatte_libretro SHARED ${MAIN_HDR} ${MAIN_SRC} ${DEBUGGER_HDR} ${DEBUGGER_SRC} ${MEM_SRC} ${RETRO_SRC} ${SOUND_SRC} ${VIDEO_SRC})

target_include_directories(gambatte_libretro PRIVATE ${GAMBATTE_DIR} ${GAMBATTE_DIR}/../include ${GAMBATTE_DIR}/../../common ${GAMBATTE_DIR}/../../common/resample ${GAMBATTE_DIR}/../libretro ${LIBRETRO_COMM_DIR}/include)

set(GAMBATTE_COMPILE_FLAGS
    -D__LIBRETRO__
    -DHAVE_STDINT_H
    -DHAVE_INTTYPES_H
    -DINLINE=inline
    # -DVIDEO_RGB565
)

target_compile_options(gambatte_libretro PRIVATE ${GAMBATTE_COMPILE_FLAGS})

target_compile_options(gambatte_libretro PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)

add_custom_command(TARGET gambatte_libretro POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:gambatte_libretro>"
     "${PROJECT_SOURCE_DIR}/cores/$<TARGET_FILE_NAME:gambatte_libretro>" 
  COMMENT "Copying to output directory")
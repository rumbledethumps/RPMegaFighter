cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

# Add include directory for rp6502 platform headers on my system.
# include_directories(/Users/rowe/Software/rp6502/llvm-mos/mos-platform/rp6502/include)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(RPMegaFighter C CXX ASM)

# Default to size-optimized build for embedded target unless user specified
# We can also try Release, but Minsizerel is more focused on size
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Minsizerel CACHE STRING "Choose the type of build" FORCE)
endif()

add_executable(rpmegafighter)
# Prefer size-optimized compilation for this embedded project
# target_compile_options(rpmegafighter PRIVATE -Os)

# Option to enable compile-time input test (define INPUT_TEST)
# Set this ON to compile the interactive `init_input_system_test()` into the
# `rpmegafighter` target. Default is OFF for production builds.
option(ENABLE_INPUT_TEST "Enable compilation of the input system test (define INPUT_TEST)" OFF)
if(ENABLE_INPUT_TEST)
    target_compile_definitions(rpmegafighter PRIVATE INPUT_TEST)
    message(STATUS "ENABLE_INPUT_TEST=ON — compiling input test into rpmegafighter")
else()
    message(STATUS "ENABLE_INPUT_TEST=OFF")
endif()
rp6502_asset(rpmegafighter 0x1E100 images/spaceship2.bin)
rp6502_asset(rpmegafighter 0x1E180 images/Earth.bin)
rp6502_asset(rpmegafighter 0x1E980 images/fighter.bin)
rp6502_asset(rpmegafighter 0x1E9A0 images/ebullet.bin)
rp6502_asset(rpmegafighter 0x1E9A8 images/bullet.bin)
rp6502_asset(rpmegafighter 0x1E9B0 images/sbullet.bin)
rp6502_asset(rpmegafighter 0x1EE40 images/fighter_explode.bin)
rp6502_asset(rpmegafighter 0x1EF40 images/powerup.bin)
rp6502_asset(rpmegafighter 0x1FC80 images/bomber.bin)
rp6502_asset(rpmegafighter 0x1F200 images/asteroid_L.bin)
rp6502_asset(rpmegafighter 0x1FA00 images/asteroid_M.bin)
rp6502_asset(rpmegafighter 0x1FC00 images/asteroid_S.bin)
rp6502_executable(rpmegafighter
    spaceship2.bin.rp6502
    Earth.bin.rp6502
    fighter.bin.rp6502
    ebullet.bin.rp6502
    bullet.bin.rp6502
    sbullet.bin.rp6502
    fighter_explode.bin.rp6502
    powerup.bin.rp6502
    bomber.bin.rp6502
    asteroid_L.bin.rp6502
    asteroid_M.bin.rp6502
    asteroid_S.bin.rp6502
    DATA file
    RESET file
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rpmegafighter.hlp
)
target_sources(rpmegafighter PRIVATE
    src/rpmegafighter.c
    src/highscore.c
    src/hud.c
    src/fighters.c
    src/player.c
    src/bullets.c
    src/sbullets.c
    src/sound.c
    src/music.c
    src/bkgstars.c
    src/pause.c
    src/title_screen.c
    src/splash_screen.c
    src/text.c
    src/input.c
    src/screens.c
    src/random.c
    src/powerup.c
    src/bomber.c
    src/asteroids.c
    src/explosions.c
)

# Gamepad test utility
add_executable(gamepad_test)
rp6502_executable(gamepad_test
    DATA file
    RESET file
)
target_sources(gamepad_test PRIVATE
    src/gamepad_test.c
)

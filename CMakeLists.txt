cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(RPMegaFighter C CXX ASM)

add_executable(rpmegafighter)
rp6502_asset(rpmegafighter 0x1E100 images/spaceship2.bin)
rp6502_asset(rpmegafighter 0x1E180 images/Earth.bin)
rp6502_asset(rpmegafighter 0x1E980 images/enemy1.bin)
rp6502_asset(rpmegafighter 0x1EB80 images/npbattle.bin)
rp6502_asset(rpmegafighter 0x1EC00 images/fighter.bin)
rp6502_asset(rpmegafighter 0x1EC20 images/ebullet.bin)
rp6502_asset(rpmegafighter 0x1EC28 images/bullet.bin)
rp6502_asset(rpmegafighter 0x1EC30 images/sbullet.bin)
rp6502_executable(rpmegafighter
    spaceship2.bin.rp6502
    Earth.bin.rp6502
    enemy1.bin.rp6502
    npbattle.bin.rp6502
    fighter.bin.rp6502
    ebullet.bin.rp6502
    bullet.bin.rp6502
    sbullet.bin.rp6502
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
    src/text.c
    src/input.c
    src/screens.c
    src/random.c
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

#!/usr/bin/env python3
"""Assembles dl4jc_juma/ for a pull request to the HL2IOBoard project.

    python3 make_upstream.py /path/to/HL2IOBoard

The folders in that project stand on their own: a main file, a CMakeLists, a
README and a built main.uf2. This one cannot be checked in that way here,
because it shares the status parser and the band table with the ESP32 firmware
in this repository and points at them with a relative path.

So it is generated. The canonical sources stay in one place, the folder for
upstream is assembled from them, and the generated README says where it came
from - which is the honest way round: a copy that drifts silently is worse than
a copy that admits to being one.
"""

import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
FW = os.path.join(REPO, "hl2io", "juma_pa")
SRC = os.path.join(REPO, "src")
NAME = "dl4jc_juma"

CMAKE = """cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(HL2IOBoard_dl4jc_juma C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# C++ rather than C: the status parser and the band table are shared with an
# ESP32 firmware that is C++, and they are the parts worth not duplicating.
add_executable(main main.cpp juma_status.cpp bands.cpp)
target_include_directories(main PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
target_compile_options(main PRIVATE -Wall -Wextra)

# All four are settings as well as build options: every bit of REG_JUMA_MODE can
# be written over I2C or over the USB console at any time, and the mode is kept
# in flash. These only decide what a board comes up with.
option(JUMA_HOLD_OPERATE "Hold the PA in OPERATE from power-up" ON)
option(JUMA_TELEMETRY    "A status line per second on USB from power-up" ON)
option(JUMA_PROXY        "The USB port is the PA's serial port from power-up" OFF)
option(JUMA_DEBUG        "Trace the PA traffic on the USB port" OFF)

set(JUMA_MODE_AT_BOOT 0)
if(JUMA_HOLD_OPERATE)
\tmath(EXPR JUMA_MODE_AT_BOOT "${JUMA_MODE_AT_BOOT} + 2")
endif()
if(JUMA_TELEMETRY)
\tmath(EXPR JUMA_MODE_AT_BOOT "${JUMA_MODE_AT_BOOT} + 4")
endif()
if(JUMA_PROXY)
\tmath(EXPR JUMA_MODE_AT_BOOT "${JUMA_MODE_AT_BOOT} + 8")
endif()
if(JUMA_DEBUG)
\ttarget_compile_definitions(main PRIVATE JUMA_DEBUG=1)
endif()
target_compile_definitions(main PRIVATE JUMA_MODE_AT_BOOT=${JUMA_MODE_AT_BOOT})

# The USB write blocks while a host is attached but not reading, and the default
# is half a second - long enough to eat into the PA's 5 s remote timeout.
target_compile_definitions(main PRIVATE PICO_STDIO_USB_STDOUT_TIMEOUT_US=10000)

pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)
pico_add_extra_outputs(main)

target_link_libraries(main
\tpico_stdlib
\thardware_i2c
\thardware_pwm
\thardware_uart
\thardware_adc
\thardware_flash
\tpico_i2c_slave
\t${PROJECT_SOURCE_DIR}/../n2adr_lib/build/libhl2ioboard.a)
"""

PROVENANCE = """
---

## Where this comes from

This folder is generated. `juma_status.cpp` and `bands.cpp` - the status parser
and the band table - are shared with an ESP32 firmware for the same amplifier,
where they are also covered by host tests, and `make_upstream.py` there copies
them in. Corrections belong upstream of this copy:

    https://github.com/jcmerg/esp32-juma

Generated from %s.
"""


def copy(src, dst):
    shutil.copy2(src, dst)
    print("  ", os.path.basename(dst))


def main():
    if len(sys.argv) < 2:
        raise SystemExit("give the path to a HL2IOBoard checkout")
    board = os.path.abspath(sys.argv[1])
    if not os.path.exists(os.path.join(board, "hl2ioboard.h")):
        raise SystemExit("%s does not look like HL2IOBoard" % board)

    out = os.path.join(board, NAME)
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out)
    print("assembling", out)

    for f in ("main.cpp", "juma_regs.h"):
        copy(os.path.join(FW, f), os.path.join(out, f))
    for f in ("juma_status.h", "juma_status.cpp", "bands.h", "bands.cpp"):
        copy(os.path.join(SRC, f), os.path.join(out, f))

    with open(os.path.join(out, "CMakeLists.txt"), "w") as f:
        f.write(CMAKE)
    print("   CMakeLists.txt")

    # Every folder there carries one, and the SDK ships the original.
    sdk = os.environ.get("PICO_SDK_PATH", "")
    imp = os.path.join(sdk, "external", "pico_sdk_import.cmake")
    if os.path.exists(imp):
        copy(imp, os.path.join(out, "pico_sdk_import.cmake"))
    else:
        print("   WARNING: no pico_sdk_import.cmake, set PICO_SDK_PATH")

    # The other folders in that project keep their photographs beside the
    # README, so put them there and point at them that way.
    docs = os.path.join(HERE, "docs")
    pictures = []
    if os.path.isdir(docs):
        for f in sorted(os.listdir(docs)):
            if f.lower().endswith((".png", ".jpg", ".jpeg")):
                copy(os.path.join(docs, f), os.path.join(out, f))
                pictures.append(f)

    readme = open(os.path.join(FW, "README.md")).read()
    for f in pictures:
        readme = readme.replace("../tools/docs/" + f, f)
    # Anything still pointing into the other repository would be a broken link.
    readme = "\n".join(l for l in readme.splitlines()
                       if "../tools/docs/" not in l)
    rev = "an unversioned tree"
    try:
        rev = subprocess.check_output(["git", "-C", REPO, "describe", "--always",
                                       "--dirty"], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        pass
    with open(os.path.join(out, "README.md"), "w") as f:
        f.write(readme + PROVENANCE % rev)
    print("   README.md")

    tools = os.path.join(out, "tools")
    os.makedirs(tools)
    for f in ("juma_gui.py", "juma_link.py", "juma_theme.py", "juma_config.py",
              "test_juma_link.py", "make_icon.py", "make_app.py"):
        copy(os.path.join(HERE, f), os.path.join(tools, f))

    # Build it here, so the folder carries a main.uf2 like the others do - and
    # so that "it builds on its own" is a fact rather than a hope.
    build = os.path.join(out, "build")
    os.makedirs(build, exist_ok=True)
    env = dict(os.environ)
    try:
        subprocess.check_call(["cmake", ".."], cwd=build, env=env,
                              stdout=subprocess.DEVNULL)
        subprocess.check_call(["make", "-j4"], cwd=build, env=env,
                              stdout=subprocess.DEVNULL)
    except (OSError, subprocess.CalledProcessError) as e:
        print("\ncould not build it here (%s) - do it by hand:" % e)
        print("  cd %s/build && cmake .. && make" % out)
        return

    # Keep the firmware, drop the build tree: nobody wants a CMake cache in a
    # pull request.
    uf2 = os.path.join(build, "main.uf2")
    keep = open(uf2, "rb").read()
    shutil.rmtree(build)
    os.makedirs(build)
    with open(uf2, "wb") as f:
        f.write(keep)
    print("   build/main.uf2 (%d bytes)" % len(keep))


if __name__ == "__main__":
    main()

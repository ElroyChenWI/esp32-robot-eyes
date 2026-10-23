# PlatformIO post-build script: merge bootloader + partitions + boot_app0 + app into one image that can be
# flashed at offset 0x0 (attached to GitHub releases).
Import("env")


def merge_bin(source, target, env):
    out = "$BUILD_DIR/robot-eyes-esp32s3.bin"
    images = env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])) + ["$ESP32_APP_OFFSET", "$BUILD_DIR/${PROGNAME}.bin"]
    env.Execute(" ".join([
        '"$PYTHONEXE"', '"$OBJCOPY"', "--chip", env.get("BOARD_MCU"), "merge_bin", "-o", f'"{out}"',
        "--flash_mode", "dio", "--flash_size", env.BoardConfig().get("upload.flash_size", "4MB"),
    ] + [f'"{i}"' if not str(i).startswith("0x") and not str(i).startswith("$ESP32") else str(i) for i in images]))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)

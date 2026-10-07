"""
PlatformIO pre-build script: patch the pinned JPEGDEC progressive grayscale decoder.

Problem:
  JPEGDecodeMCU_P computes pMCU = &sMCUs[iMCU & 0xffffff].  When iMCU is
  MCU_SKIP (-8), the bitmask produces index 0xFFFFF8 (16 777 208), creating a
  pointer ~33 MB past the 392-entry sMCUs array.  If the progressive JPEG's
  first scan includes AC coefficients (iScanEnd > 0), the AC decode loop writes
  through this wild pointer and crashes with a store-access fault.

Fix:
  Keep the existing safe pointer redirect, then apply the supplemental patch
  stack: guard skipped DC writes (upstream #2058), skip absent scan components
  (upstream #2925), and use single-block scan geometry for subsampled Y-only
  progressive previews. The latter preserves original image metadata.

Applied idempotently — safe to run on every build.
"""

Import("env")
import os
import subprocess


def patch_jpegdec(env):
    libdeps_dir = os.path.join(env["PROJECT_DIR"], ".pio", "libdeps")
    if not os.path.isdir(libdeps_dir):
        return
    for env_dir in os.listdir(libdeps_dir):
        jpeg_inl = os.path.join(libdeps_dir, env_dir, "JPEGDEC", "src", "jpeg.inl")
        if os.path.isfile(jpeg_inl):
            _apply_mcu_skip_pointer_fix(jpeg_inl)
            patch_dir = os.path.join(env["PROJECT_DIR"], "scripts", "jpegdec_patches")
            patches = sorted(name for name in os.listdir(patch_dir) if name.endswith(".patch"))
            if not patches:
                raise RuntimeError("JPEGDEC supplemental patches missing")
            for name in patches:
                _apply_patch(os.path.dirname(os.path.dirname(jpeg_inl)), os.path.join(patch_dir, name))


def _apply_patch(jpeg_dir, patch):
    def check(reverse=False):
        command = ["git", "apply", "--check"]
        if reverse:
            command.append("--reverse")
        return subprocess.run(command + [patch], cwd=jpeg_dir, capture_output=True, text=True)

    if check(reverse=True).returncode == 0:
        return
    result = check()
    if result.returncode != 0:
        raise RuntimeError("JPEGDEC patch cannot be applied: %s\n%s" % (patch, result.stderr))
    subprocess.run(["git", "apply", patch], cwd=jpeg_dir, check=True)
    print("Applied JPEGDEC patch: %s" % os.path.basename(patch))


def _apply_mcu_skip_pointer_fix(filepath):
    MARKER = "// CrossPoint patch: safe pMCU for MCU_SKIP"
    with open(filepath, "r") as f:
        content = f.read()

    if MARKER in content:
        return  # already patched

    # The wild-pointer line in JPEGDecodeMCU_P:
    OLD = "    signed short *pMCU = &pJPEG->sMCUs[iMCU & 0xffffff];"

    NEW = (
        "    " + MARKER + "\n"
        "    signed short *pMCU = (iMCU < 0) ? pJPEG->sMCUs\n"
        "                                     : &pJPEG->sMCUs[iMCU & 0xffffff];"
    )

    if OLD not in content:
        raise RuntimeError("JPEGDEC MCU_SKIP pointer patch target not found in %s" % filepath)

    content = content.replace(OLD, NEW, 1)
    with open(filepath, "w") as f:
        f.write(content)
    print("Patched JPEGDEC: safe pMCU for MCU_SKIP in JPEGDecodeMCU_P: %s" % filepath)


# Run immediately at script import time (before compilation).
patch_jpegdec(env)

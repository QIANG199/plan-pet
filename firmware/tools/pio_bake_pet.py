"""PlatformIO pre-build bridge: refresh pet_frames.bin via bake_pet.py.

Tries the interpreter that runs SCons first, then `python` from PATH (the
PlatformIO env often lacks Pillow). If baking is impossible but a previously
baked pet_frames.bin exists, we keep it and warn instead of failing the build.
"""

from pathlib import Path
import shutil
import subprocess
import sys

Import("env")

project = Path(env["PROJECT_DIR"]).resolve()
script = project / "tools" / "bake_pet.py"
out_bin = project / "src" / "pet_frames.bin"

candidates = [sys.executable]
path_python = shutil.which("python")
if path_python and Path(path_python) != Path(sys.executable):
    candidates.append(path_python)

baked = False
for py in candidates:
    try:
        subprocess.check_call([py, str(script)])
        baked = True
        break
    except subprocess.CalledProcessError:
        continue

if not baked:
    if out_bin.exists():
        print(f"warning: could not re-bake pet frames (Pillow missing?); "
              f"keeping existing {out_bin}")
    else:
        raise Exception("pet_frames.bin missing and baking failed; "
                        "install Pillow or set CLAWD_GIF_DIR")

env.Depends(
    str(Path(env.subst("$BUILD_DIR")) / "src" / "pet_blob.S.o"),
    str(out_bin),
)

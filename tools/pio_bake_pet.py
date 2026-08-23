Import("env")

from pathlib import Path
import subprocess
import sys

root = Path(env["PROJECT_DIR"]).resolve().parent
script = root / "tools" / "bake_pet.py"
subprocess.check_call([sys.executable, str(script)])

env.Depends(
    str(Path(env.subst("$BUILD_DIR")) / "src" / "pet_blob.S.o"),
    str(Path(env["PROJECT_DIR"]) / "src" / "pet_frames.bin"),
)

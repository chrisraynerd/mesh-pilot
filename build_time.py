Import("env")
import time
from pathlib import Path

# Changing a global compiler definition rebuilt every dependency each time.
# A generated header makes only the application depend on the build timestamp.
stamp = Path(env.subst("$BUILD_DIR")) / "build_stamp.h"
stamp.parent.mkdir(parents=True, exist_ok=True)
stamp.write_text("#pragma once\n#define BUILD_UNIX_TIME %du\n" % int(time.time()), encoding="ascii")
env.Append(CPPPATH=[str(stamp.parent)])

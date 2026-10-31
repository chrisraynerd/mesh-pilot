Import("env")
import time
env.Append(CPPDEFINES=[("BUILD_UNIX_TIME", int(time.time()))])

import os
import sys

env = SConscript("godot-cpp/SConstruct")

# ⚡ تفعيل كاش SCons السريع المدمج ⚡
cache_dir = os.environ.get("SCONS_CACHE_DIR", ".scons-cache")
if cache_dir:
    CacheDir(cache_dir)

# إعدادات C++20 و C
env.Append(CXXFLAGS=["-std=c++20"])
env.Append(CFLAGS=["-std=c11"])

if env["platform"] == "android":
    env.Append(CCFLAGS=["-fPIC", "-O3", "-pthread"])
else:
    env.Append(CCFLAGS=["-O3"])

env.Append(CPPPATH=[
    "src/",
    "src/godot_bindings/",
    "src/parser/",
    "src/decoders/",
    "src/scene/",
    "src/exporters/"
])

sources = Glob("src/*.cpp") + Glob("src/**/*.cpp") + Glob("src/**/*.c")
output_dir = "godot_project/bin/"

if env["platform"] == "android":
    lib_name = f"{output_dir}libunitystudio.android.{env['target']}.{env['arch']}.so"
elif env["platform"] == "windows":
    lib_name = f"{output_dir}libunitystudio.windows.{env['target']}.{env['arch']}.dll"
elif env["platform"] == "linux":
    lib_name = f"{output_dir}libunitystudio.linux.{env['target']}.{env['arch']}.so"

library = env.SharedLibrary(lib_name, sources)
Default(library)

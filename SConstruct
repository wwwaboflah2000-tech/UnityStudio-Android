import os
import sys

env = SConscript("godot-cpp/SConstruct")

# إعدادات المترجم C++20 وتفعيل تحسينات الأداء القصوى
if env["platform"] == "android":
    env.Append(CCFLAGS=["-std=c++20", "-fPIC", "-O3", "-pthread"])
    env.Append(CFLAGS=["-O3"])
else:
    env.Append(CCFLAGS=["-std=c++20", "-O3"])

# تضمين مسارات الكود
env.Append(CPPPATH=[
    "src/",
    "src/godot_bindings/",
    "src/parser/",
    "src/decoders/",
    "src/scene/",
    "src/exporters/"
])

# جمع كافة ملفات C و C++
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

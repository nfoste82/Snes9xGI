"""Build read-only collision differential against existing local engine objects."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument("--engine", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
engine = a.engine.resolve()
output = a.output.resolve()
flags = subprocess.check_output(["sdl2-config", "--cflags"], text=True).split()
libs = subprocess.check_output(["sdl2-config", "--libs"], text=True).split()
obj = Path(str(output) + ".o")
bridge = Path(str(output) + "-bridge.o")
subprocess.run(["clang", "-O2", "-std=gnu11", "-DSYSTEM_VOLUME_MIXER_AVAILABLE=0",
                "-I", str(engine), *flags, "-c", "tools/ALttP/alttp_transition_attributes_probe.c",
                "-o", str(obj)], check=True)
subprocess.run(["clang++", "-O2", "-std=c++17", "-c",
                "tools/ALttP/alttp_transition_attributes_bridge.cpp", "-o", str(bridge)], check=True)
objects = [v for v in (engine / "src").glob("*.o") if v.name != "main.o"]
objects += list((engine / "snes").glob("*.o"))
objects += [engine / "third_party/gl_core/gl_core_3_1.o",
            engine / "third_party/opus-1.3.1-stripped/opus_decoder_amalgam.o", obj, bridge]
subprocess.run(["clang++", *map(str, objects), *libs, "-lm", "-o", str(output)], check=True)
print(output)

"""Link current diagnostic adapter against existing verified engine objects."""
import argparse
from pathlib import Path
import subprocess
import hashlib
p=argparse.ArgumentParser();p.add_argument('--engine',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
assert a.output.is_dir()
assert hashlib.sha256((a.engine/'zelda3.sfc').read_bytes()).hexdigest()=='66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb'
flags=subprocess.check_output(['sdl2-config','--cflags'],text=True).split()
libs=subprocess.check_output(['sdl2-config','--libs'],text=True).split()
obj=a.output/'route-adapter.o';binary=a.output/'route-renderer'
subprocess.run(['clang','-O2','-std=gnu11','-DSYSTEM_VOLUME_MIXER_AVAILABLE=0','-I',str(a.engine),*flags,'-c','remaster/tools/ALttP/alttp_room_preview_adapter.c','-o',str(obj)],check=True)
objects=[v for v in (a.engine/'src').glob('*.o') if v.name!='main.o']+list((a.engine/'snes').glob('*.o'))+[a.engine/'third_party/gl_core/gl_core_3_1.o',a.engine/'third_party/opus-1.3.1-stripped/opus_decoder_amalgam.o',obj]
subprocess.run(['clang',*map(str,objects),*libs,'-lm','-o',str(binary)],check=True)
print(binary)

"""Same inputs, exact output equality for bounded performance ablations."""
import argparse
import json
from pathlib import Path
import subprocess

def main():
 p=argparse.ArgumentParser()
 for n in ('corpus','profile','rom','before','after','output'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();records=[]
 for raw in sorted(a.corpus.glob('room-*-entrance-*.raw')):
  state=json.loads(Path(str(raw)+'.state.json').read_text())
  args=[str(raw),str(raw)+'.attr',str(a.profile),*map(str,state['link_xy']),str(state['plane']),'--rom',str(a.rom)]
  before,after=[json.loads(subprocess.check_output([str(s),*args])) for s in (a.before,a.after)]
  assert before==after,raw
  records.append(dict(snapshot=str(raw),room=after['room_id'],complete_json_equal=True))
 a.output.write_text(json.dumps(dict(contexts=len(records),all_complete_json_equal=True,records=records),indent=2)+'\n')
 print('exact complete solver JSON equal',len(records),'contexts')
if __name__=='__main__':main()

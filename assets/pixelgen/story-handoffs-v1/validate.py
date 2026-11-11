#!/usr/bin/env python3
"""Run pixelgen validation/replays, retain exact JSON evidence and cost estimates."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
SCENES = ('tidewall-filters', 'deep-anchor-dispatch', 'cormorant-core', 'restoration-ready')
parser = argparse.ArgumentParser()
parser.add_argument('--pixelgen', default='pixelgen')
parser.add_argument('--replay-root', type=Path, required=True, help='New transient replay directory')
args = parser.parse_args()
args.replay_root.mkdir(parents=True, exist_ok=False)
spec=importlib.util.spec_from_file_location('story_packer', REPO/'tools/gen_story_beats.py')
packer=importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)
summary=[]
for name in SCENES:
    evidence={}
    scene=ROOT/name
    def run(label, *command):
        result=subprocess.run([args.pixelgen,*map(str,command)],capture_output=True,text=True)
        assert result.returncode==0,(label,result.stdout,result.stderr)
        data=json.loads(result.stdout)
        assert data.get('failure') is None,(label,data)
        if 'report' in data and isinstance(data['report'],dict):
            assert data['report']['hard_valid'] is True
        evidence[label]=data
        return data
    conversion=scene/'source/conversion'
    run('still-check','check','--input',conversion/'final.png')
    still_replay=args.replay_root/(name+'-still')
    data=run('still-replay','replay','--manifest',conversion/'manifest.json','--out',still_replay)
    assert data['conversion_replay']=='matched'
    run('still-replay-check','check','--input',still_replay/'final.png')
    assert (conversion/'final.png').read_bytes()==(still_replay/'final.png').read_bytes()
    manifest=scene/'bundle/manifest.json'
    run('animation-check','animate','check','--manifest',manifest)
    replay=args.replay_root/(name+'-animation')
    run('animation-replay','animate','replay','--manifest',manifest,'--out',replay)
    run('animation-replay-check','animate','check','--manifest',replay/'bundle/manifest.json')
    run('motion-check','animate','motion','check','--manifest',manifest)
    motion=args.replay_root/(name+'-motion')
    run('motion-replay','animate','motion','replay','--manifest',manifest,'--out',motion)
    run('motion-replay-check','animate','motion','check','--manifest',motion/'bundle/manifest.json')
    frames=[scene/'bundle/frames'/f'{p:03}.png' for p in range(4)]
    for f in frames:
        assert f.read_bytes()==(replay/'bundle/frames'/f.name).read_bytes()
        assert f.read_bytes()==(motion/'bundle/frames'/f.name).read_bytes()
    assert len({f.read_bytes() for f in frames})==4, 'four distinct poses required'
    costs=[len(packer.rle_encode(packer.image_frame(f))) for f in frames]
    # Count exact native changed pixels, including wrap from pose 3 back to 0.
    from PIL import Image,ImageChops
    pixels=[Image.open(f) for f in frames]
    changes=[sum(bool(p) for p in ImageChops.difference(pixels[i],pixels[(i+1)%4]).getdata()) for i in range(4)]
    assert all(changes),changes
    row={'scene':name,'frame_count':4,'duration_ms':1200,'rle_frame_bytes':costs,
         'rle_total_bytes':sum(costs),'changed_pixels_including_wrap':changes,
         'human_review':'unreviewed'}
    summary.append(row)
    (scene/'validation.json').write_text(json.dumps({'summary':row,'evidence':evidence},indent=2)+'\n')
    print(json.dumps(row),flush=True)
(ROOT/'validation-summary.json').write_text(json.dumps(summary,indent=2)+'\n')

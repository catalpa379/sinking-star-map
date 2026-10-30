"""Fetch the reference site's pinned terrain into a local cache; no save is read or sent.

Requires Python 3.10+ and Pillow. Assets stay separate from the source/binary release.
The source tileset repository does not declare a standalone asset license; see THIRD_PARTY.md.
"""
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import argparse, hashlib, json, urllib.request, urllib.error
from PIL import Image

REVISION = '818260c8b5fe704362722a9369350807ba635e68'
BASE = f'https://raw.githubusercontent.com/joric/ootss_tileset/{REVISION}/tiles'
LAYERS = {'ground':'overworld_64k','underworld':'underworld_64k','sky':'sky_64k'}

def prepare(output, cache):
    output.mkdir(parents=True,exist_ok=True)
    cache.mkdir(parents=True,exist_ok=True)
    def fetch(item):
        name,folder,x,y=item
        file=cache/name/str(y)/f'{x}.webp'
        blank=file.with_suffix('.empty')
        if file.exists(): return item,file
        if blank.exists(): return item,None
        file.parent.mkdir(parents=True,exist_ok=True)
        url=f'{BASE}/{folder}/4/{y}/{x}.webp'
        for attempt in range(3):
            try:
                with urllib.request.urlopen(url,timeout=35) as r: data=r.read(4*1024*1024)
                with Image.open(__import__('io').BytesIO(data)) as im:
                    if im.size!=(512,512): raise ValueError('unexpected tile dimensions')
                file.write_bytes(data)
                return item,file
            except urllib.error.HTTPError as e:
                if e.code==404:
                    blank.write_text('404 at pinned revision; transparent source tile',encoding='utf-8')
                    return item,None
                if attempt==2: raise
            except Exception:
                if attempt==2: raise
    items=[(name,folder,x,y) for name,folder in LAYERS.items() for y in range(1,15) for x in range(16)]
    tiles={}
    with ThreadPoolExecutor(max_workers=8) as pool:
        for i,f in enumerate(as_completed([pool.submit(fetch,item) for item in items]),1):
            item,path=f.result(); tiles[item[0],item[2],item[3]]=path
            if i%64==0: print(f'terrain tiles cached: {i}',flush=True)
    files=[]
    for name in LAYERS:
        # The complete source projection is a 190-unit square, from x=-24.5 / y=170.5.
        # All pixels outside the save's reveal mask are removed by native code before scaling.
        canvas=Image.new('RGB',(8192,8192),(55,106,147))
        for y in range(1,15):
            for x in range(16):
                path=tiles[name,x,y]
                if path:
                    with Image.open(path) as im:
                        rgba=im.convert('RGBA');canvas.paste(rgba,(x*512,y*512),rgba)
        target=output/f'{name}.jpg'
        canvas.save(target,quality=94,subsampling=0,optimize=True)
        files.append({'name':target.name,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
        print(f'prepared local terrain: {name}',flush=True)
    (output/'provenance.json').write_text(json.dumps({'repository':'https://github.com/joric/ootss_tileset','revision':REVISION,'source':'https://github.com/Jimmyzzt/OotSS_Sand/blob/master/data/map_assets.json','projection':{'left':-24.5,'top':170.5,'size':190},'files':files},ensure_ascii=False,indent=2),encoding='utf-8')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--cache',type=Path,required=True)
    a=p.parse_args();prepare(a.output,a.cache)

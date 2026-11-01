// Default mode keeps the strict save reveal boundary. The explicit opt-in mode projects all unfinished entries.
import {wallVoxels,localAxes} from './upstream/scripts/lib/voxels.ts';
import {positionOf,refOf} from './upstream/scripts/lib/fields.ts';
import {boundLayer,layerTable,layerSettings,fastTravelBoxes,groundLayer} from './upstream/scripts/lib/layers.ts';
const mod=(x,n)=>(x%n+n)%n;
const finite=a=>a.every(Number.isFinite);
const clean=s=>String(s??'').replace(/[\x00-\x1f\x7f]/g,' ').slice(0,160);
export function project(entities,status,translate=new Map(),hero=null,options={}){
 const showAllUnfinished=options.showAllUnfinished===true;
 const byType=t=>entities.filter(e=>e.type===t);
 const settings=byType('Settings')[0]?.fields;
 if(!settings)throw Error('缺少地图设置');
 const groups=new Map(byType('Group').map(e=>[e.pid,e.fields]));
 const table=layerTable(layerSettings(settings),fastTravelBoxes(byType('Camera_Marker').map(e=>e.fields)));
 const layer=e=>groundLayer(table,positionOf(e.fields)[2],boundLayer(groups,e.fields));
 const corner=[settings['per_screen_camera_corner.x'],settings['per_screen_camera_corner.y']];
 const screen=[settings.squares_per_screen_x,settings.squares_per_screen_y];
 const grid=[settings['grid.squares_x'],settings['grid.squares_y']];
 if(!finite([...corner,...screen,...grid])||screen.some(x=>x<=0)||grid.some(x=>x<=0||x>1024))throw Error('无效地图网格');
 const count=grid.map((n,i)=>Math.ceil(n/screen[i]));
 const wrap=p=>[corner[0]+mod(p[0]-corner[0],grid[0]),corner[1]+mod(p[1]-corner[1],grid[1]),p[2]];
 const present=(e,includeFuture=false)=>{
  if(e.destroyed)return false;
  const flags=e.fields['base.entity_flags']||[];
  if(flags.some(f=>['EXISTS_FOR_COSMETIC_TRANSITION_PURPOSES_ONLY','PLAYABLE_IN_PRELUDE'].includes(f)))return false;
  if(!includeFuture&&flags.includes('IN_EXISTENCE_GROUP')&&!flags.includes('CURRENTLY_EXISTS'))return false;
  let id=refOf(e.fields['base.group_id']),seen=new Set();
  while(id){
   if(seen.has(id))return false;seen.add(id);
   const g=groups.get(id);if(!g)return false;
   if(!includeFuture&&g.is_existence_group&&g.exists!==true)return false;
   id=refOf(g['base.group_id']);
  }
  return true;
 };
 const fogs=byType('Fog_Of_War');
 const primaries=new Map();
 for(const e of fogs.filter(e=>e.fields.primary)){
  const l=layer(e);if(primaries.has(l))throw Error('迷雾主层重复');
  const a=e.fields.region_states;
  primaries.set(l,Array.isArray(a)&&a.length===count[0]*count[1]&&a.every(x=>x===0||x===1||x===2)?a:null);
 }
 // Project the full footprint of an unknown box onto the map, including boxes crossing the torus seam.
 const boxes=[];
 for(const e of fogs.filter(e=>!e.fields.primary)){
  // These entities draw an overhang only; they do not conceal the map.
  if(e.fields.only_overhang_no_fog===true)continue;
  const a=e.fields.region_states;
  if(Array.isArray(a)&&a.length===1&&a[0]===2)continue;
  const p=positionOf(e.fields),ext=['x','y'].map(k=>e.fields['props.box_extents.'+k]);
  if(!finite([...p,...ext])||ext.some(x=>x<0))throw Error('无法确认局部迷雾边界');
  // Only relax point masking when the volume's vertical extent is known and upright.
  // Raster backgrounds have no height information and keep the full 2D mask.
  const ez=e.fields['props.box_extents.z'],sz=e.fields['base.scale.z']??1,axes=localAxes(e.fields);
  const upright=finite(axes.flat())&&Math.abs(Math.abs(axes[2][2])-1)<1e-6;
  const dz=Number.isFinite(ez)&&ez>=0&&Number.isFinite(sz)&&sz!==0&&upright?ez*Math.abs(sz):Infinity;
  // Actual local fog remains masked until positively cleared.
  for(const dx of [-grid[0],0,grid[0]])for(const dy of [-grid[1],0,grid[1]])boxes.push({l:layer(e),x:p[0]-ext[0]+dx,y:p[1]-ext[1]+dy,w:2*ext[0],h:2*ext[1],minZ:p[2]-dz,maxZ:p[2]+dz});
 }
 const region=(x,y)=>{
  const p=wrap([x,y,0]);return Math.floor((p[0]-corner[0])/screen[0])+count[0]*Math.floor((p[1]-corner[1])/screen[1]);
 };
 const visible=(l,x,y)=>(showAllUnfinished&&l>=0&&l<=2&&!!table[l])||(primaries.get(l)?.[region(x,y)]===2&&!boxes.some(b=>b.l===l&&x>=b.x&&x<b.x+b.w&&y>=b.y&&y<b.y+b.h));
 const pointVisible=(l,x,y,z)=>visible(l,x,y)||(primaries.get(l)?.[region(x,y)]===2&&!boxes.some(b=>b.l===l&&x>=b.x&&x<b.x+b.w&&y>=b.y&&y<b.y+b.h&&z>=b.minZ&&z<=b.maxZ));
 function subtract(r,b){
  const x=Math.max(r.x,b.x),y=Math.max(r.y,b.y),xx=Math.min(r.x+r.w,b.x+b.w),yy=Math.min(r.y+r.h,b.y+b.h);
  if(x>=xx||y>=yy)return [r];
  return [{x:r.x,y:r.y,w:r.w,h:y-r.y},{x:r.x,y:yy,w:r.w,h:r.y+r.h-yy},{x:r.x,y,w:x-r.x,h:yy-y},{x:xx,y,w:r.x+r.w-xx,h:yy-y}].filter(t=>t.w>0&&t.h>0);
 }
 const open=[];
 if(showAllUnfinished){
  for(let l=0;l<=2;l++)if(table[l])open.push({l,x:(settings['grid.corner.x']??corner[0])-.5,y:(settings['grid.corner.y']??corner[1])-.5,w:grid[0],h:grid[1]});
 }else for(const [l,a]of primaries){if(l>2||!a)continue;for(let i=0;i<a.length;i++)if(a[i]===2){
  let rs=[{l,x:corner[0]+screen[0]*(i%count[0]),y:corner[1]+screen[1]*Math.floor(i/count[0]),w:screen[0],h:screen[1]}];
  for(const b of boxes.filter(b=>b.l===l))rs=rs.flatMap(r=>subtract(r,b).map(t=>({...t,l})));
  open.push(...rs);
 }}
 const pins=new Map(byType('Area_Pin').filter(e=>(showAllUnfinished||e.fields.activated===true)&&present(e,showAllUnfinished)).map(e=>[e.pid,e]));
 const allPins=new Map(byType('Area_Pin').map(e=>[e.pid,e]));
 const allLocal=byType('Marker').filter(e=>e.fields.script_name==='local_area'&&e.fields.is_point_marker!==true);
 const local=allLocal.filter(e=>present(e,showAllUnfinished));
 const boundedPins=new Set(allLocal.map(e=>refOf(e.fields.script_target_entity_id)));
 const unboundedLayers=new Set([...allPins.values()].filter(e=>!boundedPins.has(e.pid)).map(e=>layer(e)));
 function areaMargin(f,p){
  const pos=positionOf(f),axes=localAxes(f),ext=['x','y','z'].map(k=>f['props.box_extents.'+k]),scale=['x','y','z'].map(k=>f['base.scale.'+k]??1);
  if(!finite([...pos,...axes.flat(),...ext,...scale])||ext.some(x=>x<0)||scale.some(x=>x===0))return null;
  const d=p.map((v,i)=>i<2?v-pos[i]-Math.round((v-pos[i])/grid[i])*grid[i]:v-pos[i]);
  return Math.max(...axes.map((axis,i)=>Math.abs(d.reduce((n,v,j)=>n+v*axis[j],0))/Math.abs(scale[i])-ext[i]));
 }
 function areaOf(p,l){
  let best=Infinity,name=null,insideUnconfirmed=false,nearest=null;
  for(const e of local){
   const f=e.fields,id=refOf(f.script_target_entity_id),pin=pins.get(id);
   if(!allPins.has(id))continue;
   const margin=areaMargin(f,p);if(margin===null)continue;
   const knownName=pin&&layer(pin)===l?translate.get(pin.fields.local_area_name):null;
   if(margin<=0){
    if(!knownName)insideUnconfirmed=true;
    if(pin&&Number.isFinite(f.float_parameter)&&f.float_parameter<best){best=f.float_parameter;name=knownName||null;}
   }
   // A nearby grouping is deliberately labelled approximate, and uses an already-known name on this layer only.
   if(knownName&&layer(pin)===l&&margin>0&&margin<=6&&(!nearest||margin<nearest.margin))nearest={margin,name:knownName};
  }
  if(name)return clean(name);
  // Not having acquired a region's map does not make the region unnamed.
  // Include inactive/future bounds here so lack of a revealed name is never proof of no region.
  let hasRegion=false,uncertain=unboundedLayers.has(l);
  for(const e of allLocal){const margin=areaMargin(e.fields,p);if(margin===null){if(layer(e)===l)uncertain=true;}else if(margin<=0)hasRegion=true;}
  if(insideUnconfirmed||hasRegion||uncertain)return '区域归属待确认（'+(['地面','地下','空中'][l]||'已探索范围')+'）';
  if(nearest)return clean(nearest.name+'（附近）');
  // Only confirmed points outside all author-defined area bounds become compass miscellaneous groups.
  // Never borrow an unrevealed area's real name or derive labels from hidden puzzle positions.
  const origin=[(settings['grid.corner.x']??corner[0])-.5,(settings['grid.corner.y']??corner[1])-.5];
  const u=(p[0]-origin[0])/grid[0],v=(p[1]-origin[1])/grid[1];
  const eastWest=u<1/3?'西':u>=2/3?'东':'',northSouth=v<1/3?'南':v>=2/3?'北':'';
  const direction=eastWest&&northSouth?eastWest+northSouth:(eastWest||northSouth?'正'+(eastWest||northSouth):'中部');
  return (['地面','地下','空中'][l]||'已知')+direction+'杂项';
 }
 const states=new Map(status.levels.map(e=>[e.name,e.flags]));
 const markers=[],countKeys=new Map();
 for(const e of byType('Level_Entry')){
  const f=e.fields,p=wrap(positionOf(f)),l=layer(e),flags=f['base.entity_flags']||[];
  if(l>2||!finite(p)||!present(e,showAllUnfinished)||!pointVisible(l,p[0],p[1],p[2])||f.currently_broken===true||f.currently_disfavored===true||flags.some(x=>/BROKEN|DISFAVORED/.test(x)))continue;
  const id=f.level_name;if(typeof id!=='string'||!id)continue;
  const bits=states.get(id);
  // A complete valid Status table authoritatively lists wins. An absent level is unsolved, never a parse fallback.
  const state=bits===undefined?0:(bits&1?1:0);
  if(showAllUnfinished&&state===1)continue;
  // Internal level filenames are exposed only for previously entered levels. No unseen title is guessed.
  if(!countKeys.has(id))countKeys.set(id,clean(e.id));
  markers.push({id:clean(e.id),countKey:countKeys.get(id),key:bits===undefined&&!showAllUnfinished?'':clean(id),name:bits===undefined&&!showAllUnfinished?'已发现谜题':clean(translate.get(id)||id),area:areaOf(p,l),state,l,x:p[0],y:p[1],z:p[2]});
 }
 // Coordinate map derived from installed static solids; never use an unmasked full-map image.
 const terrainMap=new Map();
 for(const e of byType('Wall_Set')){
  if(!present(e)||e.fields.kind==='OCEAN_HOLE')continue;
  for(const p0 of wallVoxels(e.fields)){
   const p=wrap(p0),l=groundLayer(table,p[2],boundLayer(groups,e.fields));
   if(l>2||![-.5,.49999].every(dx=>[-.5,.49999].every(dy=>visible(l,p[0]+dx,p[1]+dy))))continue;
   const k=[l,p[0],p[1]].join(',');const old=terrainMap.get(k);if(!old||old.z<p[2])terrainMap.set(k,{l,x:p[0],y:p[1],z:p[2]});
  }
 }
 let player=null;
 if(hero&&finite(hero)&&status.currentLevel==='overworld'){
  const p=wrap(hero),l=groundLayer(table,p[2]);if(visible(l,p[0],p[1]))player={l,x:p[0],y:p[1],z:p[2]};
 }
 const snapshot={open,markers,terrain:[...terrainMap.values()],player};
 const visiblePoints=markers.filter(m=>!visible(m.l,m.x,m.y)).map(({l,x,y,z})=>({l,x,y,z}));
 if(visiblePoints.length)snapshot.visiblePoints=visiblePoints;
 // Use the game's stored counter, not a count of filtered map entrances or win flags.
 if(Number.isInteger(status.levelsSolved)&&status.levelsSolved>=0&&status.levelsSolved<=100000)snapshot.savedSolved=status.levelsSolved;
 if(showAllUnfinished)snapshot.allUnfinished=true;
 return snapshot;
}
export function protocol(snapshot){
 const lines=['SMAP1'];
 if(snapshot.savedSolved!==undefined)lines.push(['G',snapshot.savedSolved].join('\t'));
 if(snapshot.allUnfinished===true)lines.push('M\tALL_UNFINISHED');
 for(const p of snapshot.visiblePoints??[])lines.push(['V',p.l,p.x,p.y,p.z].join('\t'));
 for(const r of snapshot.open)lines.push(['O',r.l,r.x,r.y,r.w,r.h].join('\t'));
 for(const t of snapshot.terrain)lines.push(['T',t.l,t.x,t.y,t.z].join('\t'));
 for(const m of snapshot.markers)lines.push(['P',m.l,m.x,m.y,m.z,m.state,m.id,m.key,m.name,m.area,m.countKey].join('\t'));
 if(snapshot.player){const p=snapshot.player;lines.push(['H',p.l,p.x,p.y,p.z].join('\t'));}
 lines.push('END');return lines.join('\n');
}

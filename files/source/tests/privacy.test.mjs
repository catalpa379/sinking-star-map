import test from 'node:test';
import assert from 'node:assert/strict';
import {project,protocol} from '../worker/project.mjs';
import {parsePair,applyHistory} from '../worker/main.mjs';
import {readFileSync} from 'node:fs';
const e=(pid,type,fields)=>({pid,id:'#test-'+pid,type,fields});
function fixture(){return [
 e(1,'Settings',{'per_screen_camera_corner.x':0,'per_screen_camera_corner.y':0,squares_per_screen_x:10,squares_per_screen_y:10,'grid.squares_x':20,'grid.squares_y':10,has_upper_layer:false,has_lower_layer:false}),
 e(2,'Fog_Of_War',{primary:true,region_states:[2,0],'base.position.z':0}),
 e(3,'Level_Entry',{'base.position.x':2,'base.position.y':2,'base.position.z':0,level_name:'visible'}),
 e(4,'Level_Entry',{'base.position.x':12,'base.position.y':2,'base.position.z':0,level_name:'SECRET_name'}),
 ];}
const status={currentLevel:'overworld',levels:[{name:'visible',flags:0},{name:'SECRET_name',flags:1}]};
const out=(es,s=status)=>project(es,s,new Map(),[12,2,0]);
test('hidden marker metadata, totals, layer names and player never enter the display protocol',()=>{
 const s=out(fixture());assert.equal(s.markers.length,1);assert.equal(s.player,null);assert(!protocol(s).includes('SECRET'));assert.deepEqual([...new Set(s.open.map(r=>r.l))],[0]);
});
test('adding, renaming or completing arbitrarily many hidden puzzles leaves every output byte identical',()=>{
 const base=protocol(out(fixture()));
 for(let n=0;n<80;n++){
  const es=fixture();es[3].fields.level_name='secret-'+n;for(let i=0;i<n;i++)es.push(e(100+i,'Level_Entry',{'base.position.x':11+i%8,'base.position.y':1+i%8,level_name:'hidden-'+i}));
  assert.equal(protocol(out(es,{...status,levels:[status.levels[0],{name:'secret-'+n,flags:n%2}]})),base);
 }
});
test('missing, short, invalid and partial fog arrays fail closed',()=>{
 for(const a of [undefined,[],[2],[2,9],[1,0]]){let es=fixture();es[1].fields.region_states=a;let s=out(es);assert.equal(s.markers.length,0);assert.equal(s.open.length,0);}
});
test('uncleared local fog box conceals markers, titles and its entire background footprint',()=>{
 let es=fixture();es.push(e(5,'Fog_Of_War',{'base.position.x':2,'base.position.y':2,'base.position.z':0,'props.box_extents.x':1,'props.box_extents.y':1,region_states:[0]}));
 const s=out(es);assert.equal(s.markers.length,0);assert(!s.open.some(r=>2>=r.x&&2<r.x+r.w&&2>=r.y&&2<r.y+r.h));
 es.at(-1).fields.region_states=[2];assert.equal(out(es).markers.length,1);
});
test('fog boxes cross the wrapping seam conservatively',()=>{
 let es=fixture();es[2].fields['base.position.x']=0;es.push(e(5,'Fog_Of_War',{'base.position.x':19.5,'base.position.y':2,'props.box_extents.x':2,'props.box_extents.y':2}));assert.equal(out(es).markers.length,0);
});
test('unknown, nonexisting or cyclic groups and destroyed entries are hidden',()=>{
 for(const state of ['unknown','cycle','off','destroyed']){
  let es=fixture();es[2].fields['base.group_id']=99;
  if(state==='cycle')es.push(e(99,'Group',{'base.group_id':99}));
  if(state==='off')es.push(e(99,'Group',{is_existence_group:true,exists:false}));
  if(state==='destroyed'){es[2].fields['base.group_id']=0;es[2].destroyed=true;}
  assert.equal(out(es).markers.length,0);
 }
});
test('unentered revealed pads show no internal name, while valid status controls wins',()=>{
 let s=out(fixture(),{currentLevel:'overworld',levels:[]});assert.equal(s.markers[0].name,'已发现谜题');assert.equal(s.markers[0].key,'');assert.equal(s.markers[0].state,0);
 s=out(fixture(),{...status,levels:[{name:'visible',flags:1}]});assert.equal(s.markers[0].state,1);
});
test('same puzzle with two visible entrances has one count key',()=>{
 let es=fixture();es.push(e(6,'Level_Entry',{'base.position.x':3,'base.position.y':3,level_name:'visible'}));assert.equal(new Set(out(es).markers.map(m=>m.countKey)).size,1);
});
test('switch advanced to fresh profile discards every previous region, marker and location',()=>{
 const advanced=out(fixture());const es=fixture();es[1].fields.region_states=[0,0];const fresh=out(es,{currentLevel:'overworld',levels:[]});assert.equal(advanced.markers.length,1);assert.deepEqual(fresh,{open:[],markers:[],terrain:[],player:null});
});
test('unactivated area names never leave the projector',()=>{
 const es=fixture();es.push(e(7,'Area_Pin',{local_area_name:'secret-area',activated:false}));es.push(e(8,'Marker',{script_name:'local_area',script_target_entity_id:7,'base.position.x':2,'base.position.y':2,'props.box_extents.x':4,'props.box_extents.y':4,'props.box_extents.z':4,float_parameter:0}));
 const s=project(es,status,new Map([['secret-area','秘密区域']]));assert(!protocol(s).includes('秘密'));assert.equal(s.markers[0].area,'区域归属待确认（地面）');
});
test('uncovered entries just outside a known area use an explicitly approximate nearby label',()=>{
 const es=fixture();es.push(e(7,'Area_Pin',{local_area_name:'known-area',activated:true}));es.push(e(8,'Marker',{script_name:'local_area',is_point_marker:false,script_target_entity_id:7,'base.position.x':5,'base.position.y':2,'props.box_extents.x':1,'props.box_extents.y':1,'props.box_extents.z':2,float_parameter:0}));
 const s=project(es,status,new Map([['known-area','已知区域']]));assert.equal(s.markers[0].area,'已知区域（附近）');
 es[2].fields['base.position.x']=5;assert.equal(project(es,status,new Map([['known-area','已知区域']])).markers[0].area,'已知区域');
});
test('nearby grouping never borrows names from an unactivated area or another layer',()=>{
 const es=fixture();es[0].fields.has_lower_layer=true;es[0].fields.underground_barrier=-4;
 es.push(e(7,'Area_Pin',{local_area_name:'secret-area',activated:false}));es.push(e(8,'Marker',{script_name:'local_area',script_target_entity_id:7,'base.position.x':5,'base.position.y':2,'props.box_extents.x':1,'props.box_extents.y':1,'props.box_extents.z':2,float_parameter:0}));
 assert(!protocol(project(es,status,new Map([['secret-area','秘密区域']]))).includes('秘密'));
 es.find(e=>e.pid===7).fields.activated=true;es.find(e=>e.pid===7).fields['base.position.z']=-21;
 assert(!protocol(project(es,status,new Map([['secret-area','秘密区域']]))).includes('秘密'));
});
function undergroundFixture(){
 const es=fixture();Object.assign(es[0].fields,{has_lower_layer:true,underground_barrier:-4});
 es.push(e(10,'Fog_Of_War',{primary:true,'base.position.z':-21,region_states:[0,0]}));
 es.push(e(11,'Level_Entry',{'base.position.x':2,'base.position.y':2,'base.position.z':-21,level_name:'UNDERGROUND_SECRET'}));
 es.push(e(12,'Level_Entry',{'base.position.x':12,'base.position.y':2,'base.position.z':-21,level_name:'HIDDEN_UNDERGROUND'}));
 return es;
}
test('underground data stays absent from layers, markers, search and statistics before its own fog is cleared',()=>{
 const es=undergroundFixture();const s=project(es,{...status,levels:[...status.levels,{name:'UNDERGROUND_SECRET',flags:1}]});
 assert(!s.open.some(r=>r.l===1));assert(!s.markers.some(m=>m.l===1));assert(!protocol(s).includes('UNDERGROUND'));assert.equal(s.markers.length,1);
});
test('revealing one underground screen exposes only that screen, with independent completion and region scope',()=>{
 const es=undergroundFixture();es.find(e=>e.pid===10).fields.region_states=[2,0];
 const s=project(es,{...status,levels:[...status.levels,{name:'UNDERGROUND_SECRET',flags:1},{name:'HIDDEN_UNDERGROUND',flags:0}]},new Map(),[2,2,-21]);
 const underground=s.markers.filter(m=>m.l===1);assert.equal(underground.length,1);assert.equal(underground[0].state,1);assert.equal(s.player.l,1);assert.equal(underground[0].area,'地下西南杂项');assert(!protocol(s).includes('HIDDEN_UNDERGROUND'));
 es.find(e=>e.pid===10).fields.region_states=[0,0];const fresh=project(es,status);assert(!fresh.open.some(r=>r.l===1));assert(!fresh.markers.some(m=>m.l===1));
});
test('binary parser rejects unknown, truncated and trailing state rather than treating it as unsolved',()=>{
 for(const n of [0,1,4,100])assert.throws(()=>parsePair(Buffer.alloc(n),Buffer.alloc(n)));
 const schema=JSON.parse(readFileSync(new URL('../worker/upstream/data/schema.json',import.meta.url)));
 assert.throws(()=>applyHistory(fixture(),{types:[{name:'Fog_Of_War',version:33}],actions:[{type:1,pid:2,etype:0,key:1,blob:new Uint8Array([25,0,33,0,255,255])}]},schema));
});
test('explicit all-unfinished mode bypasses fog and excludes completed puzzles',()=>{
 const es=fixture();const s=project(es,{...status,levels:[{name:'visible',flags:1},{name:'SECRET_name',flags:0}]},new Map(),null,{showAllUnfinished:true});
 assert.equal(s.allUnfinished,true);assert.deepEqual(s.markers.map(m=>m.key),['SECRET_name']);assert(s.markers.every(m=>m.state===0));assert(protocol(s).includes('M\tALL_UNFINISHED'));assert.equal(s.open[0].w,20);
});
test('all-unfinished is opt-in and turning it off restores the original projection byte-for-byte',()=>{
 const es=undergroundFixture(),copy=structuredClone(es);const baseline=protocol(project(es,status));
 const all=project(es,status,new Map(),null,{showAllUnfinished:true});assert(all.markers.some(m=>m.l===1));assert(all.open.some(r=>r.l===1));
 assert.equal(protocol(project(es,status,new Map(),null,{showAllUnfinished:false})),baseline);
 assert.equal(protocol(project(es,status,new Map(),null,{showAllUnfinished:'true'})),baseline);assert.deepEqual(es,copy);
});
test('opt-in mode ignores local fog boxes and exposes unentered IDs without exposing solutions',()=>{
 const es=fixture();es.push(e(5,'Fog_Of_War',{'base.position.x':2,'base.position.y':2,'props.box_extents.x':2,'props.box_extents.y':2,region_states:[0]}));
 assert.equal(project(es,status).markers.length,0);
 const s=project(es,{currentLevel:'overworld',levels:[]},new Map(),null,{showAllUnfinished:true});assert.equal(s.markers.length,2);assert(s.markers.every(m=>m.key&&m.name));assert(s.markers.every(m=>!('solution'in m)&&!('video'in m)));
});
test('opt-in includes future existence groups but still excludes abandoned or destroyed entries',()=>{
 const es=fixture();es.push(e(7,'Group',{is_existence_group:true,exists:false}));es[3].fields['base.group_id']=7;es[3].fields['base.entity_flags']=['IN_EXISTENCE_GROUP'];
 const s=project(es,{currentLevel:'overworld',levels:[]},new Map(),null,{showAllUnfinished:true});assert(s.markers.some(m=>m.id==='#test-4'));
 es[3].fields.currently_broken=true;assert(!project(es,status,new Map(),null,{showAllUnfinished:true}).markers.some(m=>m.id==='#test-4'));
});

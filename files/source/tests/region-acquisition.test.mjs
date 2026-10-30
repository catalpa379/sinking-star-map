import test from 'node:test';
import assert from 'node:assert/strict';
import {project,protocol} from '../worker/project.mjs';
const e=(pid,type,fields)=>({pid,id:'#test-'+pid,type,fields});
function fixture(){return [
 e(1,'Settings',{'per_screen_camera_corner.x':0,'per_screen_camera_corner.y':0,squares_per_screen_x:10,squares_per_screen_y:10,'grid.squares_x':30,'grid.squares_y':30,has_lower_layer:false,has_upper_layer:false}),
 e(2,'Fog_Of_War',{primary:true,region_states:[2,0,0,0,0,0,0,0,0]}),
 e(3,'Level_Entry',{'base.position.x':3,'base.position.y':3,level_name:'visible'}),
 e(4,'Area_Pin',{activated:false,local_area_name:'named-area','base.position.x':3,'base.position.y':3}),
 e(5,'Marker',{script_name:'local_area',script_target_entity_id:4,'base.position.x':3,'base.position.y':3,'props.box_extents.x':2,'props.box_extents.y':2,'props.box_extents.z':2,float_parameter:0}),
 ];}
const status={levels:[]},names=new Map([['named-area','真实区域名称']]);
test('unacquired named region remains unconfirmed, then becomes its real name when acquired',()=>{
 const es=fixture();let s=project(es,status,names);assert.equal(s.markers[0].area,'区域归属待确认（地面）');assert(!protocol(s).includes('真实区域名称'));assert(!protocol(s).includes('杂项'));
 es[3].fields.activated=true;s=project(es,status,names);assert.equal(s.markers[0].area,'真实区域名称');
 es[3].fields.activated=false;assert.equal(project(es,status,names).markers[0].area,'区域归属待确认（地面）');
});
test('opt-in can use a named region without permanently revealing that name in default mode',()=>{
 const es=fixture(),normal=protocol(project(es,status,names));
 assert.equal(project(es,status,names,null,{showAllUnfinished:true}).markers[0].area,'真实区域名称');
 assert.equal(protocol(project(es,status,names)),normal);
});
test('inactive region bounds, missing translations, invalid bounds and unbounded pins are never miscellaneous',()=>{
 for(const mode of ['inactive','translation','extent','priority','unbounded']){
  const es=fixture();let map=names;
  if(mode==='inactive'){es.push(e(8,'Group',{is_existence_group:true,exists:false}));es[4].fields['base.group_id']=8;}
  if(mode==='translation'){es[3].fields.activated=true;map=new Map();}
  if(mode==='extent')delete es[4].fields['props.box_extents.x'];
  if(mode==='priority'){es[3].fields.activated=true;delete es[4].fields.float_parameter;}
  if(mode==='unbounded')es.pop();
  const s=project(es,status,map);assert.equal(s.markers[0].area,'区域归属待确认（地面）',mode);assert(!protocol(s).includes('杂项'),mode);
 }
});
test('only points proven outside all area bounds become miscellaneous, even when another map is unacquired',()=>{
 const es=fixture();es[4].fields['base.position.x']=15;es[3].fields['base.position.x']=15;
 assert.equal(project(es,status,names).markers[0].area,'地面西南杂项');
});
test('hidden membership and names never produce a region placeholder before its puzzle is revealed',()=>{
 const es=fixture();es[1].fields.region_states.fill(0);assert.deepEqual(project(es,status,names),{open:[],markers:[],terrain:[],player:null});
});

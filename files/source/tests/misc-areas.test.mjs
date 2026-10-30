import test from 'node:test';
import assert from 'node:assert/strict';
import {project,protocol} from '../worker/project.mjs';
const e=(pid,type,fields)=>({pid,id:'#test-'+pid,type,fields});
function fixture(){return [e(1,'Settings',{'per_screen_camera_corner.x':0,'per_screen_camera_corner.y':0,squares_per_screen_x:10,squares_per_screen_y:10,'grid.squares_x':30,'grid.squares_y':30,has_lower_layer:true,underground_barrier:-4,has_upper_layer:true,skyground_barrier:10}),e(2,'Fog_Of_War',{primary:true,region_states:Array(9).fill(2),'base.position.z':0}),e(3,'Fog_Of_War',{primary:true,region_states:Array(9).fill(2),'base.position.z':-21}),e(4,'Fog_Of_War',{primary:true,region_states:Array(9).fill(2),'base.position.z':20})];}
test('unconfirmed areas use visible layer plus stable compass sector; explicit names are not changed',()=>{
 const es=fixture(),expected=new Map();let id=10;
 for(const [z,layer] of [[0,'地面'],[-21,'地下'],[20,'空中']])for(const [y,ns]of [[3,'南'],[13,''],[23,'北']])for(const [x,ew]of [[3,'西'],[13,''],[23,'东']]){es.push(e(id,'Level_Entry',{'base.position.x':x,'base.position.y':y,'base.position.z':z,level_name:'puzzle'+id}));const direction=ew&&ns?ew+ns:(ew||ns?'正'+(ew||ns):'中部');expected.set('#test-'+id,layer+direction+'杂项');id++;}
 const snapshot=project(es,{levels:[]});assert.equal(snapshot.markers.length,27);for(const m of snapshot.markers)assert.equal(m.area,expected.get(m.id));
 es.push(e(100,'Area_Pin',{activated:true,local_area_name:'named','base.position.x':3,'base.position.y':3,'base.position.z':0}));
 es.push(e(101,'Marker',{script_name:'local_area',script_target_entity_id:100,'base.position.x':3,'base.position.y':3,'props.box_extents.x':1,'props.box_extents.y':1,'props.box_extents.z':1,float_parameter:0}));
 assert.equal(project(es,{levels:[]},new Map([['named','原有区域名称']])).markers.find(m=>m.id==='#test-10').area,'原有区域名称');
});
test('concealed miscellaneous groups have no placeholder and cannot affect visible labels or counts',()=>{
 const es=fixture();es[1].fields.region_states=[2,0,0,0,0,0,0,0,0];es[2].fields.region_states.fill(0);es[3].fields.region_states.fill(0);
 es.push(e(10,'Level_Entry',{'base.position.x':3,'base.position.y':3,level_name:'known'}));
 const before=protocol(project(es,{levels:[]}));assert(before.includes('地面西南杂项'));assert(!before.includes('东北杂项')&&!before.includes('地下'));
 for(let i=0;i<20;i++)es.push(e(20+i,'Level_Entry',{'base.position.x':23,'base.position.y':23,level_name:'hidden'+i}));
 assert.equal(protocol(project(es,{levels:[]})),before);
});

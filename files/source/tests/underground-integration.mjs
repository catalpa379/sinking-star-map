// Exercises the real installed underground definitions using in-memory synthetic fog only.
// Never writes a game save; never prints underground names, coordinates or puzzle counts.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {Package} from '../worker/upstream/scripts/lib/package.ts';
import {positionOf,refOf} from '../worker/upstream/scripts/lib/fields.ts';
import {layerTable,layerSettings,fastTravelBoxes,groundLayer,boundLayer} from '../worker/upstream/scripts/lib/layers.ts';
import {parsePair,applyHistory} from '../worker/main.mjs';
import {project,protocol} from '../worker/project.mjs';
const [game,campaign]=process.argv.slice(2);if(!game||!campaign)throw Error('game and campaign required');
const paths=['Status.sokoban_status','overworld.history'].map(n=>campaign+'/'+n),bytes=paths.map(p=>readFileSync(p));
const hash=b=>createHash('sha256').update(b).digest('hex');
const {status,history}=parsePair(...bytes);
const pkg=new Package(game+'/data/levels.package');let es;
try{es=pkg.levelWithDefaults('overworld',new Set(['Settings','Fog_Of_War','Group','Level_Entry','Area_Pin','Marker','Camera_Marker','Wall_Set']));applyHistory(es,history,pkg.schema);}finally{pkg.close();}
const settings=es.find(e=>e.type==='Settings').fields,groups=new Map(es.filter(e=>e.type==='Group').map(e=>[e.pid,e.fields]));
const table=layerTable(layerSettings(settings),fastTravelBoxes(es.filter(e=>e.type==='Camera_Marker').map(e=>e.fields)));
const layer=e=>groundLayer(table,positionOf(e.fields)[2],boundLayer(groups,e.fields));
const original=protocol(project(es,status));
assert(!project(es,status).open.some(r=>r.l===1),'This audit expects the supplied campaign not to have uncovered underground');
const primary=es.find(e=>e.type==='Fog_Of_War'&&e.fields.primary&&layer(e)===1);assert(primary);
const target=es.find(e=>e.type==='Level_Entry'&&layer(e)===1&&!refOf(e.fields['base.group_id'])&&!e.destroyed&&!e.fields.currently_broken&&!e.fields.currently_disfavored&&!e.fields['base.entity_flags']?.some(f=>/EXISTENCE|PRELUDE/.test(f)));assert(target);
const p=positionOf(target.fields),sx=settings.squares_per_screen_x,sy=settings.squares_per_screen_y,cx=settings['per_screen_camera_corner.x'],cy=settings['per_screen_camera_corner.y'],nx=Math.ceil(settings['grid.squares_x']/sx),ny=Math.ceil(settings['grid.squares_y']/sy);
const region=Math.floor((p[0]-cx)/sx)+nx*Math.floor((p[1]-cy)/sy);assert(region>=0&&region<nx*ny);
const synthetic=structuredClone(es);
for(const e of synthetic.filter(e=>e.type==='Fog_Of_War'))e.fields.region_states=Array(e.fields.primary?nx*ny:1).fill(0);
synthetic.find(e=>e.pid===primary.pid).fields.region_states[region]=2;
const trialStatus=structuredClone(status);trialStatus.currentLevel='overworld';trialStatus.levels=trialStatus.levels.filter(e=>e.name!==target.fields.level_name);trialStatus.levels.push({name:target.fields.level_name,flags:0});
const opened=project(synthetic,trialStatus,new Map(),p);assert.deepEqual([...new Set(opened.open.map(r=>r.l))],[1]);
assert(opened.markers.some(m=>m.id===target.id&&m.l===1&&m.state===0));assert(opened.player?.l===1);
assert(opened.markers.every(m=>m.l===1&&Math.floor((m.x-cx)/sx)+nx*Math.floor((m.y-cy)/sy)===region));
trialStatus.levels.find(e=>e.name===target.fields.level_name).flags=1;assert(project(synthetic,trialStatus).markers.find(m=>m.id===target.id).state===1);
synthetic.find(e=>e.pid===primary.pid).fields.region_states.fill(0);assert.deepEqual(project(synthetic,trialStatus),{open:[],markers:[],terrain:[],player:null});
assert.equal(protocol(project(es,status)),original);
for(let i=0;i<paths.length;i++)assert.equal(hash(readFileSync(paths[i])),hash(bytes[i]),'Source file changed during audit');
console.log('PASS real underground definitions, independent fog, completion, position, reset isolation; original save hashes unchanged');

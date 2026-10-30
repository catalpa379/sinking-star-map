// Optional: real installed game + caller-owned scratch directory. Original save files are opened read-only.
import {loadSnapshot} from '../worker/main.mjs';
import {protocol} from '../worker/project.mjs';
import {readFileSync,writeFileSync,mkdirSync,copyFileSync} from 'node:fs';
import {join,resolve} from 'node:path';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
const [game,campaign,scratch]=process.argv.slice(2);
if(!game||!campaign||!scratch)throw Error('game, campaign and scratch required');
if(resolve(scratch).startsWith(resolve(campaign)))throw Error('scratch must not be in save folder');
const sha=b=>createHash('sha256').update(b).digest('hex');
const files=['Status.sokoban_status','overworld.history'];
const before=files.map(f=>sha(readFileSync(join(campaign,f))));
const a=join(scratch,'advanced'),b=join(scratch,'fresh');mkdirSync(a,{recursive:true});mkdirSync(b,{recursive:true});
for(const file of files)copyFileSync(join(campaign,file),join(a,file));
copyFileSync(join(a,files[0]),join(b,files[0]));
// A complete v73 history with one empty level-start record. It makes no claims of revealed fog.
const fresh=Buffer.alloc(118);fresh.writeUInt32LE(73,0);fresh.writeUInt32LE(7,4);fresh.writeBigInt64LE(25n,8);writeFileSync(join(b,files[1]),fresh);
const advanced=loadSnapshot(game,a);assert(advanced.open.length>0);
const allUnfinished=loadSnapshot(game,a,{showAllUnfinished:true});assert(allUnfinished.allUnfinished);assert(allUnfinished.markers.every(m=>m.state===0));assert.equal(protocol(loadSnapshot(game,a)),protocol(advanced));
const empty=loadSnapshot(game,b);assert.deepEqual(empty,{open:[],markers:[],terrain:[],player:null});
assert.equal(protocol(loadSnapshot(game,a)),protocol(advanced));
writeFileSync(join(b,files[0]),readFileSync(join(b,files[0])).subarray(0,100));assert.throws(()=>loadSnapshot(game,b));
assert.throws(()=>loadSnapshot(game,b,{showAllUnfinished:true}));
for(let i=0;i<files.length;i++)assert.equal(sha(readFileSync(join(campaign,files[i]))),before[i],'source changed during integration test');
console.log('PASS real save parse, explicit all-unfinished mode and restoration, profile isolation, malformed-save rejection in both modes; source hashes unchanged');

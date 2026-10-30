import {readFileSync,statSync} from 'node:fs';
import {join,resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {Package} from './upstream/scripts/lib/package.ts';
import {parseStatus} from './upstream/src/status-file.ts';
import {parseHistory} from './upstream/src/history-file.ts';
import {createBlobDecoder} from './upstream/src/blob.ts';
import {project,protocol} from './project.mjs';
export const compatible={exe:'0ae10a513dae25286fe93d0db4fa8a2df9a473bc07e1734e12bc1ad30166db1c',package:'408aba3676925faea0c823f8c231e30844feffe9c32ae42644d273ef31863589'};
export const compatibleCurrent={exe:'d802098b7cb85eadfc733b88d1554042fad950c572e916febea3f669a474a549',package:'d747e5d9b8f540c9c6b90e8e987bd74a8c7f8f2f0672fce189403128f1f3117c'};
const hash=b=>createHash('sha256').update(b).digest('hex');
const TYPES=new Set(['Settings','Fog_Of_War','Group','Level_Entry','Area_Pin','Marker','Camera_Marker','Wall_Set']);
function checkedRead(path,max){const s=statSync(path);if(!s.isFile()||s.size>max)throw Error('文件大小不受支持');return readFileSync(path);}
export function parsePair(statusBytes,historyBytes){
 if(statusBytes.readUInt32LE(0)!==33||historyBytes.readUInt32LE(0)!==73)throw Error('仅验证 Status 33 / History 73');
 const status=parseStatus(statusBytes),history=parseHistory(historyBytes);
 if(history.error)throw Error('历史文件不完整');
 if(new Set(status.levels.map(x=>x.name)).size!==status.levels.length)throw Error('状态表存在重复编号');
 if(status.levels.some(x=>x.flags&~15))throw Error('未知谜题状态标志');
 return {status,history};
}
export function applyHistory(entities,history,schema){
 const decode=createBlobDecoder(schema),latest=new Map(),life=new Map();
 for(const a of history.actions){
  if(a.type===3)continue;
  const type=history.types[a.etype]?.name;if(!TYPES.has(type))continue;
  const d=decode(a.blob,type,a.type===1);
  if(d.baseVersion!==25||d.typeVersion!==history.types[a.etype].version)throw Error('历史实体版本不匹配');
  if(a.type===2){const old=life.get(a.pid);if(!old||old.key<a.key)life.set(a.pid,{key:a.key,destroyed:a.destroyed});}
  for(const field of d.fields){
   const name=(field.derived?'':'base.')+field.f.n,key=a.pid+':'+name,old=latest.get(key);
   if(old&&old.key>=a.key)continue;
   // Reuse the schema-aware package value decoder on this one field through raw FieldValue conversion.
   const v=field.new;let value;
   if(v.str!==undefined)value=v.str;
   else if(v.arr!==undefined){
    if(v.raw&&field.f.e?.sz===1)value=[...v.raw];
    else if(v.raw&&field.f.e?.sz===8){const view=new DataView(v.raw.buffer,v.raw.byteOffset,v.raw.length);value=Array.from({length:v.arr},(_,i)=>view.getBigUint64(i*8,true));}
    else continue;
   }
   else if(v.raw){const raw=v.raw,view=new DataView(raw.buffer,raw.byteOffset,raw.length),t=field.f.enum_t||field.f.t;
    if(t==='bool')value=!!view.getUint8(0);
    else if(t==='f32')value=view.getFloat32(0,true);
    else if(raw.length===1)value=view.getUint8(0);
    else if(raw.length===2)value=view.getUint16(0,true);
    else if(raw.length===4)value=t==='s32'?view.getInt32(0,true):view.getUint32(0,true);
    else continue;
    if(field.f.enum){const info=schema.enums[field.f.enum];if(info?.flags)value=info.values.filter(([n,b])=>b!==0&&(Number(value)&b)===b).map(([n])=>n);else value=info?.values.find(([n,b])=>b===value)?.[0]??value;}
   }else continue;
   latest.set(key,{key:a.key,value});
  }
 }
 for(const e of entities){
  // Existence groups reset to exists_by_default on load before saved diffs apply.
  if(e.type==='Group'&&e.fields.is_existence_group)e.fields.exists=e.fields.exists_by_default===true;
  e.destroyed=life.get(e.pid)?.destroyed??false;
  for(const [key,change]of latest){const prefix=e.pid+':';if(key.startsWith(prefix))e.fields[key.slice(prefix.length)]=change.value;}
 }
 return entities;
}
function saveBusy(){return Object.assign(new Error('游戏正在保存，请重试刷新'),{code:'SAVE_BUSY'});}
function checkedSaveRead(path,max){try{return checkedRead(path,max);}catch(error){if(['EBUSY','EACCES','EPERM','ENOENT'].includes(error.code))throw saveBusy();throw error;}}
export function readWithSaveRetry(read,pause=()=>Atomics.wait(new Int32Array(new SharedArrayBuffer(4)),0,0,60)){
 for(let attempt=0;attempt<3;attempt++)try{return read();}catch(error){if(error.code!=='SAVE_BUSY'||attempt===2)throw error;pause();}
}
export function publicReadError(error){
 if(error?.code==='SAVE_BUSY')return '存档正在保存或暂时被占用；地图已清空，请稍后刷新重试。';
 const version=['游戏版本未经验证，已隐藏地图','地图资源版本未经验证','仅验证 Status 33 / History 73','历史实体版本不匹配','基础实体版本不受支持','存档世界版本与游戏不一致，需先由游戏正常加载','实体结构发生变化'];
 if(version.includes(error?.message))return '游戏或存档格式尚不兼容；地图已清空，请检查游戏版本。';
 return '存档读取或校验失败；地图已清空，请刷新重试。';
}
export function loadSnapshot(game,campaign,options={}){return readWithSaveRetry(()=>loadSnapshotOnce(game,campaign,options));}
function loadSnapshotOnce(game,campaign,options={}){
 game=resolve(game);campaign=resolve(campaign);
 const exeHash=hash(checkedRead(join(game,'sinking_star.exe'),32*1024*1024));
 const version=exeHash===compatible.exe?'26006400':exeHash===compatibleCurrent.exe?'26249670':null;
 if(!version)throw Error('游戏版本未经验证，已隐藏地图');
 const packagePath=join(game,'data','levels.package');
 if(hash(checkedRead(packagePath,512*1024*1024))!==(version==='26006400'?compatible.package:compatibleCurrent.package))throw Error('地图资源版本未经验证');
 const sp=join(campaign,'Status.sokoban_status'),hp=join(campaign,'overworld.history');
 const sb=checkedSaveRead(sp,8*1024*1024),hb=checkedSaveRead(hp,128*1024*1024);
 const {status,history}=parsePair(sb,hb);
 const pkg=new Package(packagePath);let entities,schema,header;
 try{header=pkg.rawEntities('overworld').header;entities=pkg.levelWithDefaults('overworld',TYPES);schema=pkg.schema;}finally{pkg.close();}
 if(header.base!==25||hb.readBigInt64LE(8)!==25n)throw Error('基础实体版本不受支持');
 const settings=entities.find(x=>x.type==='Settings').fields;
 if(settings.level_data_version!==hb.readUInt32LE(4))throw Error('存档世界版本与游戏不一致，需先由游戏正常加载');
 for(const t of history.types)if(TYPES.has(t.name)&&header.types.find(([n])=>n===t.name)?.[1]!==t.version)throw Error('实体结构发生变化');
 applyHistory(entities,history,schema);
 const localized=readFileSync(join(game,'data','strings','s-cn.localization'),'utf8');
 const translate=new Map();for(const m of localized.matchAll(/^`([^\r\n]+)\r?\n([^\r\n]+)/gm))translate.set(m[1],m[2]);
 const snapshot=project(entities,status,translate,history.hero,options);
 // Re-read paths, not open handles: the game atomically replaces files on its save thread.
 if(!sb.equals(checkedSaveRead(sp,8*1024*1024))||!hb.equals(checkedSaveRead(hp,128*1024*1024)))throw saveBusy();
 return snapshot;
}
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
 try{
  if(process.argv.length!==4&&process.argv.length!==5)throw Error('需要游戏目录和单个档案目录');
  const flag=process.argv[4];if(flag&&!['--all-unfinished'].includes(flag))throw Error('未知显示模式');
  const snapshot=loadSnapshot(process.argv[2],process.argv[3],{showAllUnfinished:flag==='--all-unfinished'});
  process.stdout.write(protocol(snapshot));
 }catch(error){process.stdout.write('SMAP1\nERROR\t'+publicReadError(error)+'\nEND');process.exitCode=1;}
}

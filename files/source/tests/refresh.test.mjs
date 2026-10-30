import test from 'node:test';
import assert from 'node:assert/strict';
import {readWithSaveRetry,publicReadError} from '../worker/main.mjs';
test('only a changing or temporarily locked save is retried, up to three fresh reads',()=>{
 let reads=0,pauses=0;const expected={known:true};
 assert.equal(readWithSaveRetry(()=>{if(++reads<3)throw Object.assign(new Error('busy'),{code:'SAVE_BUSY'});return expected;},()=>pauses++),expected);
 assert.equal(reads,3);assert.equal(pauses,2);
 reads=0;assert.throws(()=>readWithSaveRetry(()=>{reads++;throw Object.assign(new Error('busy'),{code:'SAVE_BUSY'});},()=>{}));assert.equal(reads,3);
});
test('corrupt or incompatible data never becomes an unfinished result and is not retried',()=>{
 let reads=0;assert.throws(()=>readWithSaveRetry(()=>{reads++;throw Error('corrupt');},()=>assert.fail('must not pause')));assert.equal(reads,1);
});
test('public errors distinguish save races from compatibility and never print raw paths or metadata',()=>{
 assert(publicReadError({code:'SAVE_BUSY'}).includes('正在保存'));
 assert(publicReadError(Error('地图资源版本未经验证')).includes('不兼容'));
 const s=publicReadError(Error('C:/Users/private/secret-puzzle'));assert(!s.includes('private')&&!s.includes('secret-puzzle'));assert(s.includes('校验失败'));
});

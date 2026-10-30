// Local path shim; original project path discovery is not bundled.
import {dirname,join} from 'node:path';
import {fileURLToPath} from 'node:url';
export function data(...parts:string[]){ return join(dirname(fileURLToPath(import.meta.url)),'../../data',...parts); }
export function game(...parts:string[]){ throw new Error('Explicit game path required'); }

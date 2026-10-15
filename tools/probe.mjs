import { createPelican, createBicycle, THREE } from './bundle.mjs';
const bike = createBicycle(); const pel = createPelican();
function walk(o,d=0){ const g=o.geometry; let s=' '.repeat(d)+o.type+' '+(o.name||'')+(g?' v='+g.attributes.position.count:'')+(o.material?' mat='+(o.material.type)+' tr='+o.material.transparent+' vc='+o.material.vertexColors+(o.material.map?' MAP':''):'')+(o.isInstancedMesh?' inst='+o.count:'')+(o.visible?'':' HIDDEN'); console.log(s); o.children.forEach(c=>walk(c,d+1)); }
walk(bike.group); walk(pel.root); console.log(Object.keys(pel));

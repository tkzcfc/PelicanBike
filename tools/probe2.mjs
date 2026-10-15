import { createWorld, THREE } from './bundle.mjs';
const scene = new THREE.Scene(); const world = createWorld(scene, { density: 1 });
scene.children.forEach((o, i) => console.log(i, o.type, o.isInstancedMesh ? 'inst ' + o.count + ' v=' + o.geometry.attributes.position.count : '', Object.keys(o.userData || {}).join(','), o.children.length, o.material ? o.material.type : ''));

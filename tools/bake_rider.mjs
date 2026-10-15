// Rider baker v2: rigid joints (bone palette) + per-pose bone matrices (16 crank x 2 trick).
// Neck tube is rebuilt at runtime on CPU (DynamicTube port), so no pose vertex data.
import { createPelican, createBicycle, THREE } from './bundle.mjs';
import fs from 'fs';
const OUT = '/workspace/PelicanBike_rename/PelicanBike/Content/data';
const toS = (c) => { c = Math.max(0, Math.min(1, c)); return Math.round(255 * (c <= 0.0031308 ? c * 12.92 : 1.055 * Math.pow(c, 1 / 2.4) - 0.055)); };
const bike = createBicycle(); const pel = createPelican();
const rider = new THREE.Group(); rider.add(bike.group, pel.root);
const gripL = new THREE.Vector3(), gripR = new THREE.Vector3();
function pose(crank, trick, extra = {}) {
  for (let w = 0; w < 180; w++) {
    bike.update({ wheelAngle: 0, crankAngle: crank, steerAngle: extra.steer || 0, speed: 7, night: 0 });
    bike.group.updateMatrixWorld(true);
    const sm = bike.steerMatrix();
    gripL.copy(bike.anchors.gripL.position).applyMatrix4(sm); gripR.copy(bike.anchors.gripR.position).applyMatrix4(sm);
    pel.update(1 / 60, { t: extra.t || 0, crankAngle: crank, pedal: 1, airborne: extra.air || 0, trick, accel: 0, look: new THREE.Vector3(6, 1.15, 0), mouthOpen: extra.mouth || 0,
      grips: { L: gripL, R: gripR }, pedals: { L: bike.anchors.pedalL.position, R: bike.anchors.pedalR.position }, helmet: true, glasses: true });
  }
  rider.updateMatrixWorld(true);
}
const all = []; rider.traverse((o) => { if (o !== rider) all.push(o); });
const snapLocal = () => all.map((o) => o.matrix.clone());
// detect controllers
pose(0, 0); const L0 = snapLocal();
pose(0, 0, { steer: 0.3 }); const Ls = snapLocal();
pose(0, 0, { mouth: 1 }); const Lm = snapLocal();
const changed = (A, B) => all.filter((o, i) => !A[i].equals(B[i]));
const steerObj = changed(L0, Ls).filter((o) => o.parent === bike.group);
const mouthObjs = changed(L0, Lm).filter((o) => { for (let q = o.parent; q; q = q.parent) if (q === pel.head) return true; return false; });
console.log('steer', steerObj.length, 'mouth', mouthObjs.length, mouthObjs.map((o) => o.rotation.z));
bike.update({ wheelAngle: 1.234, crankAngle: 0, steerAngle: 0, speed: 0, night: 0 });
const rotors = all.filter((o) => Math.abs(o.rotation.z + 1.234) < 1e-6);
// steer axis
const sq = new THREE.Quaternion(); steerObj[0].updateMatrix(); { pose(0, 0, { steer: 0.3 }); sq.copy(steerObj[0].quaternion); }
const sAxis = new THREE.Vector3(sq.x, sq.y, sq.z).normalize();
// jaw vs upper: jaw has negative z when mouth open
pose(0, 0, { mouth: 1 });
const jaw = mouthObjs.find((o) => o.rotation.z < 0), upper = mouthObjs.find((o) => o.rotation.z > 0);
const ctls = [ // name, obj, kind(0 local axis, 1 head), axis, gain
  ['steer', steerObj[0], 0, sAxis], ['rotor0', rotors[0], 0, new THREE.Vector3(0, 0, 1)], ['rotor1', rotors[1], 0, new THREE.Vector3(0, 0, 1)],
  ['head', pel.head, 1, new THREE.Vector3(0, 1, 0)], ['jaw', jaw, 0, new THREE.Vector3(0, 0, 1)], ['upper', upper, 0, new THREE.Vector3(0, 0, 1)]];
// helmet / glasses: objects whose visibility follows ctx.helmet / ctx.glasses
const visAll = () => all.map((o) => o.visible);
pose(0, 0); const v0 = visAll();
const togg = (key) => { pel.update(1 / 60, { t: 0, crankAngle: 0, pedal: 1, airborne: 0, trick: 0, accel: 0, look: new THREE.Vector3(6, 1.15, 0), grips: { L: gripL, R: gripR }, pedals: { L: bike.anchors.pedalL.position, R: bike.anchors.pedalR.position }, helmet: key !== 'helmet', glasses: key !== 'glasses' });
  const v = visAll(); const r = all.filter((o, i) => v[i] !== v0[i]); pose(0, 0); return r[0]; };
const helmetObj = togg('helmet'), glassesObj = togg('glasses');
console.log('helmet', !!helmetObj, 'glasses', !!glassesObj);
// wings (flap only matters in the spread/trick pose): objects whose world rotation changes with flap
pose(0, 1); const Wa = all.map((o) => o.matrixWorld.clone()); pose(0, 1, { t: Math.PI / 17, air: 1 }); const Wb = all.map((o) => o.matrixWorld.clone());
const rotDiff = (i) => [0, 1, 2, 4, 5, 6, 8, 9, 10].some((e) => Math.abs(Wa[i].elements[e] - Wb[i].elements[e]) > 1e-3);
const wingJ = all.filter((o, i) => o.parent === pel.root && rotDiff(i) && Math.abs(o.position.z) > 0.02);
const wingSide = wingJ.map((o) => Math.sign(o.position.z));
const shoulders = new Set([-1, 1].map((s) => { const c = wingJ.filter((o, i) => wingSide[i] === s); const d = c.map((o) => { const i = all.indexOf(o); return Math.hypot(Wa[i].elements[12] - Wb[i].elements[12], Wa[i].elements[13] + 0.025 - Wb[i].elements[13], Wa[i].elements[14] - Wb[i].elements[14]); }); return c[d.indexOf(Math.min(...d))]; }));
const lids = []; pel.head.traverse((o) => { if (o.isMesh && Math.abs(o.scale.x - 0.001) < 1e-6) lids.push(o); });
console.log('wing joints', wingJ.length, 'sides', wingSide, 'lids', lids.length, 'root rot', pel.root.matrixWorld.elements.slice(0, 11).map((x) => x.toFixed(3)).join(' '));
pose(0, 0);
const forced = new Set([...ctls.map((c) => c[1]), pel.body, pel.knot, helmetObj, glassesObj, ...wingJ, ...lids]);
// poses
const F = 16, K = 2, poses = [];
for (let k = 0; k < K; k++) for (let f = 0; f < F; f++) { pose((f / F) * Math.PI * 2, k); poses.push(all.map((o) => o.matrixWorld.clone())); }
const idx = new Map(all.map((o, i) => [o, i]));
const rel = (a, m, p) => new THREE.Matrix4().copy(p[idx.get(a)]).invert().multiply(p[idx.get(m)]);
const constRel = (a, m) => { const r0 = rel(a, m, poses[0]); return poses.every((p) => { const r = rel(a, m, p); return r.elements.every((e, i) => Math.abs(e - r0.elements[i]) < 2e-5); }); };
// rider-level: is an object constant in rider space?
const constW = (o) => poses.every((p) => p[idx.get(o)].elements.every((e, i) => Math.abs(e - poses[0][idx.get(o)].elements[i]) < 2e-5));
pose(0, 0); // visibility state
const neckMesh = all.find((o) => o.isMesh && o.frustumCulled === false && o.parent === pel.root && o.geometry.attributes.uv && o.geometry.attributes.position.count === 29 * 19);
const bones = []; const boneOf = new Map(); const ROOT = { name: 'root' }; bones.push(ROOT);
const getBone = (o) => { if (!boneOf.has(o)) { boneOf.set(o, bones.length); bones.push(o); } return boneOf.get(o); };
ctls.forEach((c) => getBone(c[1])); getBone(pel.body); getBone(pel.knot); getBone(pel.head); getBone(helmetObj); getBone(glassesObj);
const mats = new Map(); const matId = (m) => { const em = m.emissive ? Math.min(1, (m.emissiveIntensity || 0) * Math.max(m.emissive.r, m.emissive.g, m.emissive.b) / 8) : 0;
  const key = [Math.round((m.roughness ?? 0.6) * 255), Math.round((m.metalness ?? 0) * 255), Math.round(em * 255), (m.side === THREE.DoubleSide ? 1 : 0) | (m.flatShading ? 2 : 0) | (m.map ? 4 : 0) | (emisCls.get(m) || 0)].join(',');
  if (!mats.has(key)) mats.set(key, mats.size); return mats.get(key); };
// night-driven emissive materials (bike.update: lens 0.3+7n, tail 0.4+5n(+blink), reflector 0.25+0.8n)
const emisCls = new Map();
{ const mm = new Set(); rider.traverse((o) => { if (o.material) mm.add(o.material); });
  bike.update({ wheelAngle: 0, crankAngle: 0, steerAngle: 0, speed: 7, night: 0 }); const e0 = new Map([...mm].map((m) => [m, m.emissiveIntensity]));
  bike.update({ wheelAngle: 0, crankAngle: 0, steerAngle: 0, speed: 7, night: 1 });
  for (const m of mm) { const a = e0.get(m), b = m.emissiveIntensity; if (a !== b) { const cls = Math.abs(a - 0.3) < 1e-6 ? 8 : Math.abs(a - 0.4) < 1e-6 ? 16 : 32; emisCls.set(m, cls); m.emissiveIntensity = 8; console.log('emissive cls', cls, a, b, m.emissive.getHexString(), m.color.getHexString()); } } }
rider.traverse((o) => { if (o.isMesh && o.material && o.material.map && !o.material.transparent) { forced.add(o); o.geometry.computeBoundingBox(); console.log('basket bb', o.geometry.boundingBox.min.toArray(), o.geometry.boundingBox.max.toArray()); } });
const V = [], I = []; let vis = new Set(); rider.traverseVisible((o) => vis.add(o));
let skipped = 0;
for (const o of all) {
  if (!o.isMesh || !vis.has(o) || o === neckMesh) continue;
  const mat = Array.isArray(o.material) ? o.material[0] : o.material;
  if (mat.map && !mat.transparent) console.log('mapped', o.geometry.attributes.position.count, mat.color.getHexString());
  if (mat.transparent || mat.type === 'MeshBasicMaterial') { skipped++; continue; }
  let j = o;
  while (!forced.has(j) && j.parent && j.parent !== rider && constRel(j.parent, o)) j = j.parent;
  let b;
  if (!forced.has(j) && constW(o) && !ctls.some((c) => { for (let q = o; q; q = q.parent) if (q === c[1]) return true; return false; })) b = 0; else b = getBone(j);
  const Wj = b === 0 ? new THREE.Matrix4() : poses[0][idx.get(j)];
  const Mloc = new THREE.Matrix4().copy(Wj).invert().multiply(poses[0][idx.get(o)]);
  const insts = o.isInstancedMesh ? [...Array(o.count)].map((_, k) => { const im = new THREE.Matrix4(); o.getMatrixAt(k, im); return Mloc.clone().multiply(im); }) : [Mloc];
  const g = o.geometry, pos = g.attributes.position, nor = g.attributes.normal, col = g.attributes.color, mc = mat.color || new THREE.Color(1, 1, 1);
  const mid = matId(mat);
  for (const M of insts) {
    const N3 = new THREE.Matrix3().getNormalMatrix(M); const base = V.length; const p = new THREE.Vector3(), n = new THREE.Vector3();
    for (let k = 0; k < pos.count; k++) {
      p.fromBufferAttribute(pos, k).applyMatrix4(M); if (nor) n.fromBufferAttribute(nor, k).applyMatrix3(N3).normalize(); else n.set(0, 1, 0);
      let r = mc.r, gg = mc.g, bb = mc.b; if (col && mat.vertexColors) { r *= col.getX(k); gg *= col.getY(k); bb *= col.getZ(k); }
      V.push([p.x, p.y, p.z, n.x, n.y, n.z, toS(r), toS(gg), toS(bb), mid, b]);
    }
    if (g.index) for (let k = 0; k < g.index.count; k++) I.push(base + g.index.getX(k)); else for (let k = 0; k < pos.count; k++) I.push(base + k);
  }
}
// dedupe identical vertices
const vmap = new Map(), V2 = [], remap = [];
const q = (a) => [Math.round(a[0] * 8192), Math.round(a[1] * 8192), Math.round(a[2] * 8192), Math.round(a[3] * 127), Math.round(a[4] * 127), Math.round(a[5] * 127), a[6], a[7], a[8], a[9], a[10]];
V.forEach((a, k) => { const qq = q(a); const key = qq.join(','); if (!vmap.has(key)) { vmap.set(key, V2.length); V2.push(qq); } remap[k] = vmap.get(key); });
const I2 = I.map((x) => remap[x]);
console.log('bones', bones.length, 'verts', V.length, '->', V2.length, 'tris', I2.length / 3, 'mats', mats.size, 'skipped', skipped);
if (V2.length > 65535) throw new Error('too many verts');
const maxAbs = Math.max(...V2.map((a) => Math.max(Math.abs(a[0]), Math.abs(a[1]), Math.abs(a[2]))));
if (maxAbs > 32767) throw new Error('range ' + maxAbs / 8192);
// controllers: parent ctl
const ctlParent = ctls.map((c) => { for (let qq = c[1].parent; qq; qq = qq.parent) { const ci = ctls.findIndex((d) => d[1] === qq); if (ci >= 0) return ci; } return -1; });
const boneCtl = bones.map((o) => { if (o === ROOT) return -1; for (let qq = o; qq; qq = qq.parent) { const ci = ctls.findIndex((d) => d[1] === qq); if (ci >= 0) return ci; } return -1; });
// --- write ---
const out = []; const u32 = (x) => { const b = Buffer.alloc(4); b.writeUInt32LE(x); out.push(b); }; const f32s = (arr) => { const b = Buffer.alloc(arr.length * 4); arr.forEach((x, i) => b.writeFloatLE(x, i * 4)); out.push(b); };
const i32 = (x) => { const b = Buffer.alloc(4); b.writeInt32LE(x); out.push(b); };
out.push(Buffer.from('PBR2')); u32(bones.length); u32(F); u32(K); u32(ctls.length); u32(mats.size); u32(V2.length); u32(I2.length);
for (let b = 0; b < bones.length; b++) i32(boneCtl[b]);
ctls.forEach((c, i) => { i32(boneOf.get(c[1])); i32(ctlParent[i]); u32(c[2]); f32s(c[3].toArray()); });
for (const p of poses) for (const o of bones) { const e = o === ROOT ? new THREE.Matrix4().elements : p[idx.get(o)].elements; f32s([e[0], e[1], e[2], e[4], e[5], e[6], e[8], e[9], e[10], e[12], e[13], e[14]]); }
{ const b = Buffer.alloc(mats.size * 4); [...mats.keys()].forEach((k, i) => k.split(',').forEach((x, j) => b.writeUInt8(+x, i * 4 + j))); out.push(b); }
{ const b = Buffer.alloc(V2.length * 16); V2.forEach((a, k) => { const o = k * 16; for (let c = 0; c < 3; c++) b.writeInt16LE(a[c], o + c * 2); b.writeUInt8(a[10], o + 6); b.writeUInt8(a[9], o + 7);
  for (let c = 0; c < 3; c++) b.writeInt8(a[3 + c], o + 8 + c); for (let c = 0; c < 3; c++) b.writeUInt8(a[6 + c], o + 11 + c); }); out.push(b); }
{ const b = Buffer.alloc(I2.length * 2); I2.forEach((x, k) => b.writeUInt16LE(x, k * 2)); out.push(b); }
// neck: material colour + matId, body bone, head bone, knot bone, root matrix (pel.root in rider space)
{ const m = neckMesh.material; f32s([toS(m.color.r), toS(m.color.g), toS(m.color.b)]); u32(matId(m)); }
u32(boneOf.get(pel.body)); u32(boneOf.get(pel.head)); u32(boneOf.get(pel.knot)); u32(boneOf.get(helmetObj)); u32(boneOf.get(glassesObj)); f32s(pel.root.matrixWorld.elements);
// rewrite mat table (neck may have added one) -> put table at end too
{ const b = Buffer.alloc(mats.size * 4); [...mats.keys()].forEach((k, i) => k.split(',').forEach((x, j) => b.writeUInt8(+x, i * 4 + j))); u32(mats.size); out.push(b); }
u32(wingJ.length); wingJ.forEach((o, i) => { u32(boneOf.get(o)); i32(wingSide[i]); u32(shoulders.has(o) ? 1 : 0); });
u32(lids.length); lids.forEach((o) => u32(boneOf.get(o)));
fs.writeFileSync(`${OUT}/rider2.pbr`, Buffer.concat(out));
console.log('size', Buffer.concat(out).length, 'root', pel.root.matrixWorld.elements.slice(12), 'ctls', ctls.map((c) => c[0] + ':' + boneOf.get(c[1]) + '/' + c[3].toArray().map((x) => x.toFixed(3))), 'ctlParent', ctlParent);
console.log('bodyPos', pel.body.position.toArray(), 'head', pel.head.position.toArray());
{ pose(0, 0); const m = new THREE.Vector3(); pel.mouth.getWorldPosition(m); const inv = pel.head.matrixWorld.clone().invert(); console.log('mouthInHead', m.applyMatrix4(inv).toArray());
  const kp = pel.knot.position; console.log('knot', kp.toArray()); }

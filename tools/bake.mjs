// PelicanBike baker: evaluates the original JS scene-construction code and writes compact
// binary meshes (pos f32x3 | normal s8x4 | color sRGB u8x4 | mat u8x4: rough,metal,emis,flags)
import { createPelican, createBicycle, createWorld, BIKE_POINTS, THREE, fishGeometry } from './bundle.mjs';
import fs from 'fs';
const OUT = '/workspace/PelicanBike_rename/PelicanBike/Content/data';
const VS = 24;
const toS = (c) => { c = Math.max(0, Math.min(1, c)); return Math.round(255 * (c <= 0.0031308 ? c * 12.92 : 1.055 * Math.pow(c, 1 / 2.4) - 0.055)); };
class Builder {
  constructor() { this.v = []; this.i = []; this.n = 0; }
  addMesh(o, M, opts = {}) {
    if (!o.geometry || !o.material) return;
    const mat = Array.isArray(o.material) ? o.material[0] : o.material;
    if (mat.transparent && !opts.keepTransparent) return;
    if (mat.type === 'MeshBasicMaterial' && !opts.keepBasic) return;
    const mats = o.isInstancedMesh ? [...Array(o.count)].map((_, k) => { const im = new THREE.Matrix4(); o.getMatrixAt(k, im); return M.clone().multiply(im); }) : [M];
    for (const MM of mats) this._add(o.geometry, mat, MM, o.isInstancedMesh && o.instanceColor ? null : null);
  }
  _add(g, mat, M, tint) {
    const pos = g.attributes.position, nor = g.attributes.normal, col = g.attributes.color;
    const N3 = new THREE.Matrix3().getNormalMatrix(M);
    const base = this.n; const p = new THREE.Vector3(), nn = new THREE.Vector3();
    const mc = mat.color || new THREE.Color(1, 1, 1);
    const em = mat.emissive ? Math.min(1, (mat.emissiveIntensity || 0) * Math.max(mat.emissive.r, mat.emissive.g, mat.emissive.b) / 8) : 0;
    const flags = (mat.side === THREE.DoubleSide ? 1 : 0) | (mat.flatShading ? 2 : 0);
    for (let k = 0; k < pos.count; k++) {
      p.fromBufferAttribute(pos, k).applyMatrix4(M);
      if (nor) nn.fromBufferAttribute(nor, k).applyMatrix3(N3).normalize(); else nn.set(0, 1, 0);
      let r = mc.r, gg = mc.g, b = mc.b;
      if (col && mat.vertexColors) { r *= col.getX(k); gg *= col.getY(k); b *= col.getZ(k); }
      this.v.push([p.x, p.y, p.z, nn.x, nn.y, nn.z, toS(r), toS(gg), toS(b), Math.round((mat.roughness ?? 0.6) * 255), Math.round((mat.metalness ?? 0) * 255), Math.round(em * 255), flags]);
    }
    if (g.index) for (let k = 0; k < g.index.count; k++) this.i.push(base + g.index.getX(k));
    else for (let k = 0; k < pos.count; k++) this.i.push(base + k);
    this.n += pos.count;
  }
  addTree(root, skip = new Set(), opts) {
    root.updateMatrixWorld(true);
    const inv = new THREE.Matrix4().copy(root.matrixWorld).invert();
    root.traverseVisible((o) => {
      for (let q = o; q; q = q.parent) if (skip.has(q)) return;
      if (o.isMesh) this.addMesh(o, inv.clone().multiply(o.matrixWorld), opts);
    });
  }
  bytes() {
    const vb = Buffer.alloc(this.n * VS);
    this.v.forEach((a, k) => { const o = k * VS; vb.writeFloatLE(a[0], o); vb.writeFloatLE(a[1], o + 4); vb.writeFloatLE(a[2], o + 8);
      for (let c = 0; c < 3; c++) vb.writeInt8(Math.round(a[3 + c] * 127), o + 12 + c);
      vb.writeUInt8(a[12], o + 15); for (let c = 0; c < 3; c++) vb.writeUInt8(a[6 + c], o + 16 + c); vb.writeUInt8(255, o + 19); for (let c = 0; c < 3; c++) vb.writeUInt8(a[9 + c], o + 20 + c); vb.writeUInt8(255, o + 23); });
    const ib = Buffer.alloc(this.i.length * 4); this.i.forEach((x, k) => ib.writeUInt32LE(x, k * 4));
    return { vb, ib, nv: this.n, ni: this.i.length };
  }
}
// file: "PBM1", nparts, then per part: nameLen,name, nv, ni, vb, ib
function write(file, parts) {
  const chunks = [Buffer.from('PBM1'), Buffer.alloc(4)]; chunks[1].writeUInt32LE(parts.length);
  for (const [name, b] of parts) { const { vb, ib, nv, ni } = b.bytes(); const h = Buffer.alloc(12 + 32); h.write(name, 0); h.writeUInt32LE(nv, 32); h.writeUInt32LE(ni, 36); chunks.push(h, vb, ib); console.log(file, name, nv, ni); }
  fs.writeFileSync(`${OUT}/${file}`, Buffer.concat(chunks));
}
// ---------- world props ----------
const scene = new THREE.Scene(); const world = createWorld(scene, { density: 1 });
const ch = world.chunks[0]; const props = [];
for (const key of ['round', 'pine', 'palm', 'rock', 'umb']) { const m = ch.inst[key]; const b = new Builder(); b._add(m.geometry, m.material, new THREE.Matrix4()); props.push([key, b]); }
// static per-chunk: guard (staticMat) and lamps
ch.group.children.forEach((o) => { if (o.isMesh && !o.isInstancedMesh && o.name !== 'terrain') console.log('chunk child', o.geometry.attributes.position.count, o.material.type, o.material.transparent); });
const [terrain, roadMesh, guard, lamps, bulbs] = ch.group.children;
{ const b = new Builder(); b._add(guard.geometry, guard.material, new THREE.Matrix4()); b._add(lamps.geometry, lamps.material, new THREE.Matrix4()); props.push(['static', b]); }
{ const b = new Builder(); const m = bulbs.material.clone(); m.emissiveIntensity = 8; b._add(bulbs.geometry, m, new THREE.Matrix4()); props.push(['bulbs', b]); }
write('props.pbm', props);
// ---------- world extras (lighthouse, boats, birds, sign post) ----------
{
  const parts = [];
  const lh = scene.children.find((o) => o.userData && o.userData.beam);
  const [isl, grass, tower, gallery, lantern, cap, beam] = lh.children;
  lh.position.set(0, 0, 0); lh.updateMatrixWorld(true);
  { const b = new Builder(); for (const o of [isl, grass, gallery, cap]) b.addMesh(o, o.matrixWorld.clone());
    // striped tower: 6 bands (top white), CylinderGeometry(1.1,1.7,13,24)
    for (let i = 0; i < 6; i++) { const y0 = 13 / 2 - (i + 1) * 13 / 6, y1 = 13 / 2 - i * 13 / 6; const rAt = (y) => 1.1 + (1.7 - 1.1) * (13 / 2 - y) / 13;
      const g = new THREE.CylinderGeometry(rAt(y1), rAt(y0), 13 / 6, 24, 1, true); g.translate(0, (y0 + y1) / 2 + 11, 0);
      b._add(g, new THREE.MeshStandardMaterial({ color: i % 2 ? '#c9302c' : '#f4f1ea', roughness: 0.6 }), new THREE.Matrix4()); }
    parts.push(['lh_static', b]); }
  { const b = new Builder(); const m = lantern.material.clone(); m.emissiveIntensity = 8; b.addMesh(lantern, lantern.matrixWorld.clone()); b.v.forEach((v) => { v[11] = 255; }); parts.push(['lh_lantern', b]); }
  { const b = new Builder(); const g = beam.children[0].geometry; const uv = g.attributes.uv; const st = b.n;
    b._add(g, new THREE.MeshStandardMaterial({ color: '#ffffff' }), new THREE.Matrix4().makeTranslation(0, 18.5, 0));
    for (let k = 0; k < uv.count; k++) b.v[st + k][6] = Math.round(uv.getY(k) * 255);
    parts.push(['lh_beam', b]); }
  const boats = scene.children.filter((o) => o.userData && o.userData.period);
  boats.forEach((g, i) => { g.position.set(0, 0, 0); g.rotation.set(0, 0, 0); g.scale.setScalar(1); g.updateMatrixWorld(true); const b = new Builder(); b.addTree(g); parts.push([`boat${i}`, b]); console.log('boat', i, g.userData); });
  const inst = scene.children.filter((o) => o.isInstancedMesh);
  const names = ['gull_body', 'gull_wing', null, 'pel_body', 'pel_wing', null];
  inst.forEach((m, i) => { if (!names[i]) return; const b = new Builder(); b._add(m.geometry, m.material, new THREE.Matrix4()); parts.push([names[i], b]); });
  { const ch0 = world.chunks[0]; const post = ch0.sign.children[0]; const b = new Builder(); b._add(post.geometry, post.material, new THREE.Matrix4()); parts.push(['signpost', b]); }
  { const g = fishGeometry(); const b = new Builder();
    b._add(g, new THREE.MeshStandardMaterial({ vertexColors: true, metalness: 0.6, roughness: 0.25, emissive: new THREE.Color('#1f6b8f'), emissiveIntensity: 0.45 * 1.7, side: THREE.DoubleSide }), new THREE.Matrix4()); parts.push(['fish', b]);
    const b2 = new Builder(); b2._add(g, new THREE.MeshStandardMaterial({ color: '#ffcf3f', metalness: 1, roughness: 0.18, emissive: new THREE.Color('#ff9500'), emissiveIntensity: 0.9, side: THREE.DoubleSide }), new THREE.Matrix4()); parts.push(['fishgold', b2]); }
  write('world2.pbm', parts);
}

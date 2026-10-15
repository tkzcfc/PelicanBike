// Capture the JS demo's linear HDR scene (before bloom/tonemap) as float32 RGB: node js_hdr.mjs out.bin hour cam waitMs [extra]
// Writes [w,h] int32 + w*h*3 float32 rows bottom-up (GL order), plus a PNG screenshot next to it.
import { chromium } from '/var/tmp/sand-playwright-runtime/270d8c328ed1753929de28079ae8d5fe/node_modules/playwright-core/index.mjs';
import fs from 'fs';
const [out, hour = '17.35', cam = 'chase', wait = '20000', extra = ''] = process.argv.slice(2);
const browser = await chromium.launch({ executablePath: '/usr/bin/google-chrome', headless: true,
  args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-first-run', '--disable-dev-shm-usage', '--force-device-scale-factor=1', '--mute-audio'] });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
await page.goto(`http://127.0.0.1:8092/index_hdr.html?time=${hour}&freeze=1&cam=${cam}&q=high&autostart=1&dt=0.033&${extra}`, { waitUntil: 'load', timeout: 120000 });
await page.waitForTimeout(+wait);
await page.screenshot({ path: out.replace(/\.bin$/, '.png') });
const r = await page.evaluate(() => {
  const { renderer, scene, camera, THREE } = window.__pc; const w = 640, h = 360;
  const rt = new THREE.WebGLRenderTarget(w, h, { type: THREE.FloatType });
  const old = renderer.getRenderTarget(); renderer.setRenderTarget(rt); renderer.render(scene, camera); renderer.setRenderTarget(old);
  const px = new Float32Array(w * h * 4); renderer.readRenderTargetPixels(rt, 0, 0, w, h, px);
  const rgb = new Array(w * h * 3); for (let i = 0; i < w * h; i++) { rgb[i * 3] = px[i * 4]; rgb[i * 3 + 1] = px[i * 4 + 1]; rgb[i * 3 + 2] = px[i * 4 + 2]; }
  return { w, h, rgb, pr: renderer.getPixelRatio() };
});
const b = Buffer.alloc(8 + r.rgb.length * 4); b.writeInt32LE(r.w, 0); b.writeInt32LE(r.h, 4); r.rgb.forEach((v, i) => b.writeFloatLE(v, 8 + i * 4));
fs.writeFileSync(out, b); console.log('hdr', r.w, r.h, 'pr', r.pr);
await browser.close();

// time-aligned JS capture: node js_shot_t.mjs out.png hour cam simT [extra]
// env: FX="jump@1.5,confetti@1.6" VW VH TOUCH JS_NOBLOOM JS_TMO (ms, default 180000)
import { chromium } from '/var/tmp/sand-playwright-runtime/270d8c328ed1753929de28079ae8d5fe/node_modules/playwright-core/index.mjs';
const [out, hour = '17.35', cam = 'chase', simT = '4', extra = 'speed=0'] = process.argv.slice(2);
const TMO = +(process.env.JS_TMO || 180000);
setTimeout(() => { console.error('HARD TIMEOUT'); process.exit(3); }, TMO);
const browser = await chromium.launch({ executablePath: '/usr/bin/google-chrome', headless: true,
  args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-first-run', '--disable-dev-shm-usage', '--force-device-scale-factor=1', '--mute-audio'] });
const VW = +(process.env.VW || 1280), VH = +(process.env.VH || 720), touch = !!process.env.TOUCH;
const page = await browser.newPage({ viewport: { width: VW, height: VH }, hasTouch: touch, isMobile: touch });
await page.addInitScript((t) => { window.__holdAt = t; }, +simT);
await page.goto(`http://127.0.0.1:8092/index_hdr.html?time=${hour}&freeze=1&cam=${cam}&q=high&autostart=1&dt=0.033&${extra}`, { waitUntil: 'load', timeout: 90000 });
if (process.env.FX) await page.evaluate((fx) => {
  const ev = fx.split(',').filter(Boolean).map((e) => { const [k, t] = e.split('@'); return { k, t: +t, done: false }; });
  setInterval(() => {
    const pc = window.__pc; if (!pc) return;
    for (const e of ev) if (!e.done && pc.S.t >= e.t) {
      e.done = true;
      try {
        if (e.k === 'jump') pc.jump();
        else if (e.k === 'trick') pc.trick();
        else if (e.k === 'confetti') pc.confetti.burst(pc.tmpV.set(0.3, 1.9, pc.S.lane));
        else if (e.k === 'splash') pc.splashAt(pc.rider.position.x + 4, -16, 26);
        else if (e.k === 'jumper') pc.launchJumper(pc.rider.position.x + 6, -16);
      } catch (err) { console.warn('fx', e.k, err); }
    }
  }, 50);
}, process.env.FX);
const target = +simT;
const t0 = Date.now();
while (true) {
  const t = await page.evaluate(() => (window.__pc ? window.__pc.S.t : -1));
  if (t >= target) break;
  if (Date.now() - t0 > TMO - 15000) { console.error('WAIT TIMEOUT t=', t); process.exit(4); }
  if ((Date.now() - t0) % 15000 < 600) console.log('wait t=', t.toFixed(2));
  await page.waitForTimeout(400);
}
if (process.env.JS_NOBLOOM) await page.evaluate(() => { window.__pc.bloom.enabled = false; });
await page.waitForTimeout(1500);
await page.screenshot({ path: out, timeout: 30000 });
console.log('S.t', await page.evaluate(() => window.__pc.S.t));
browser.close().catch(() => {}); setTimeout(() => process.exit(0), 1500);  // close() can hang under swiftshader

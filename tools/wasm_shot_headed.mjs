// headless Chrome (swiftshader) capture of the WASM build served on :8089
import { chromium } from '/var/tmp/sand-playwright-runtime/270d8c328ed1753929de28079ae8d5fe/node_modules/playwright-core/index.mjs';
const out = process.argv[2] || '/workspace/PelicanBike2/shots/wasm_chase.png';
const wait = +(process.argv[3] || 25000);
const query = process.argv[4] || '';          // e.g. '?autostart=1&time=12'
const click = process.argv[5] || '';          // e.g. '575,525' -> click then save <out>_after.png
const touch = /touch/.test(query);
const browser = await chromium.launch({ executablePath: '/usr/bin/google-chrome', headless: false,
  args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-first-run', '--disable-dev-shm-usage', '--force-device-scale-factor=1', '--mute-audio'] });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 }, hasTouch: touch, isMobile: false });
const logs = []; page.on('response', (r) => { if (r.status() >= 400) logs.push('404 ' + r.url()); }); page.on('console', (m) => logs.push(m.text())); page.on('pageerror', (e) => logs.push('[pageerror] ' + e.message));
await page.goto('http://127.0.0.1:8089/PelicanBike.html' + query, { waitUntil: 'load', timeout: 120000 });
await page.waitForTimeout(wait);
await page.screenshot({ path: out });
if (click) { const [x, y] = click.split(',').map(Number); await page.mouse.click(x, y); await page.waitForTimeout(6000); await page.screenshot({ path: out.replace(/\.png$/, '_after.png') });
  console.log('audio', await page.evaluate(() => { try { return Object.values(AL.contexts).map((c) => c.audioCtx.state).join(','); } catch (e) { return 'n/a ' + e.message; } })); }
console.log(logs.slice(-25).join('\n'));
await browser.close();

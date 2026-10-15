// JS reference capture: node js_shot.mjs out.png hour cam [waitMs] [extra query]
import { chromium } from '/var/tmp/sand-playwright-runtime/270d8c328ed1753929de28079ae8d5fe/node_modules/playwright-core/index.mjs';
const [out, hour = '17.35', cam = 'chase', wait = '20000', extra = ''] = process.argv.slice(2);
const browser = await chromium.launch({ executablePath: '/usr/bin/google-chrome', headless: true,
  args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--no-first-run', '--disable-dev-shm-usage', '--force-device-scale-factor=1', '--mute-audio'] });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
const logs = []; page.on('console', (m) => logs.push(m.text())); page.on('pageerror', (e) => logs.push('[pageerror] ' + e.message));
await page.goto(`http://127.0.0.1:8092/index.html?time=${hour}&freeze=1&cam=${cam}&q=high&dt=0.033&${extra}`, { waitUntil: 'load', timeout: 120000 });
await page.waitForTimeout(+wait);
await page.screenshot({ path: out });
console.log(logs.filter((l) => /rror/.test(l)).slice(-8).join('\n'));
await browser.close();

import { chromium } from '/var/tmp/sand-playwright-runtime/270d8c328ed1753929de28079ae8d5fe/node_modules/playwright-core/index.mjs';
import fs from 'fs';
const src = fs.readFileSync('/workspace/claude-opus-5-5-demo/pelican-bike/src/textures.js', 'utf8').split('\n');
const grab = (start) => { const i = src.findIndex((l) => l.startsWith(start)); let j = i + 1; while (src[j] !== '}') j++; return src.slice(i, j + 1).join('\n').replace(/^export /, ''); };
const code = [grab('function makeCanvas'), grab('function drawPelicanGlyph'), grab('function roundRect'), grab('export function makeSignTexture')].join('\n').replace(/return toTexture\(c[^;]*;/g, 'return c;');
const browser = await chromium.launch({ executablePath: '/usr/bin/google-chrome', headless: true });
const page = await browser.newPage();
await page.setContent('<html><body></body></html>');
for (const k of ['crossing', 'speed', 'fish']) {
  const url = await page.evaluate(`(()=>{${code}; return makeSignTexture('${k}').toDataURL('image/png');})()`);
  fs.writeFileSync(`/workspace/PelicanBike_rename/PelicanBike/Content/tex/sign_${k}.png`, Buffer.from(url.split(',')[1], 'base64'));
}
await browser.close(); console.log('ok');

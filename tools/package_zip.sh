#!/bin/bash
# repackage /workspace/PelicanBike_web.zip from build_wasm output
set -e; cd "$(dirname "$0")/.."
rm -rf /tmp/pkg && mkdir -p /tmp/pkg/PelicanBike_web/web && cp build_wasm/bin/PelicanBike/* /tmp/pkg/PelicanBike_web/web/ && cp tools/wasm_serve.py /tmp/pkg/PelicanBike_web/
cd /tmp/pkg/PelicanBike_web
printf '@echo off\r\ncd /d "%%~dp0"\r\necho Open: http://127.0.0.1:8089/PelicanBike.html\r\npython wasm_serve.py --root web --port 8089\r\npause\r\n' > 启动.bat
printf '鹈鹕骑单车 (Axmol WASM)\r\n\r\n1. 安装 Python 3\r\n2. 双击 启动.bat（或: python wasm_serve.py --root web --port 8089）\r\n3. 浏览器打开: http://127.0.0.1:8089/PelicanBike.html\r\n\r\n注意：必须通过该服务器打开（需要 COOP/COEP 头），不能直接双击 html。\r\n' > 说明.txt
cd /tmp/pkg && rm -f /workspace/PelicanBike_web.zip && python3 -c "import shutil;shutil.make_archive('/workspace/PelicanBike_web','zip','.','PelicanBike_web')"
ls -la /workspace/PelicanBike_web.zip

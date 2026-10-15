#!/bin/bash
# Re-subset the CJK font to every glyph used in Source/ (+ printable ASCII and UI symbols).
cd "$(dirname "$0")/.." && python3 - <<'PY'
import glob
s=set(chr(c) for c in range(32,127))|set("▶◀▼▲⤒·—…×✓★☆○●←→↑↓「」『』（），。：；！？、")
for f in glob.glob('Source/**/*.[ch]*',recursive=True): s|=set(open(f,encoding='utf-8',errors='ignore').read())
open('/tmp/pb2_chars.txt','w').write(''.join(sorted(c for c in s if ord(c)>=32)))
PY
pyftsubset tools/NotoSansSC-full.ttf --text-file=/tmp/pb2_chars.txt --output-file=Content/fonts/NotoSansSC-Regular.ttf --no-hinting
ls -la Content/fonts/NotoSansSC-Regular.ttf

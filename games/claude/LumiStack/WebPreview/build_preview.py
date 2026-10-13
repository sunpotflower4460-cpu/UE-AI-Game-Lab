#!/usr/bin/env python3
"""Builds WebPreview/index.html: compiles the shared C++ core to WASM and inlines it."""
import base64
import pathlib
import subprocess

here = pathlib.Path(__file__).resolve().parent
subprocess.run([str(here / "build_wasm.sh")], check=True)
wasm = (here / "lumi.wasm").read_bytes()
template = (here / "preview.template.html").read_text(encoding="utf-8")
html = template.replace("__LUMI_WASM_BASE64__", base64.b64encode(wasm).decode("ascii"))
# The template is written as page content (the artifact host adds the document skeleton);
# the standalone file gets its own so it can be opened directly in any browser.
standalone = (
    '<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
    '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
    '<style>html,body{margin:0}</style>\n</head>\n<body>\n' + html + '\n</body>\n</html>\n'
)
(here / "index.html").write_text(standalone, encoding="utf-8")
(here / "lumi.wasm").unlink()
print(f"index.html written ({len(html) // 1024} KB, wasm {len(wasm) // 1024} KB)")

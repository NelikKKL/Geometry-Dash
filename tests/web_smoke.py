#!/usr/bin/env python3
"""Headless-browser smoke test for the WebAssembly build.

Usage: python3 tests/web_smoke.py <site_dir> <path/to/Geometry_Dash_1_0.apk> [jszip.min.js]
Needs: pip install playwright pillow && playwright install chromium
Fails if a page error occurs, assets are not imported, or the game canvas stays blank.
"""
import functools, http.server, socketserver, sys, threading, time
from playwright.sync_api import sync_playwright

site, apk = sys.argv[1], sys.argv[2]
jszip = sys.argv[3] if len(sys.argv) > 3 else None

class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a): pass

socketserver.TCPServer.allow_reuse_address = True
srv = socketserver.TCPServer(("127.0.0.1", 0), functools.partial(Quiet, directory=site))
threading.Thread(target=srv.serve_forever, daemon=True).start()
errors = []
with sync_playwright() as p:
    b = p.chromium.launch(args=["--no-sandbox", "--use-gl=swiftshader", "--enable-unsafe-swiftshader"])
    pg = b.new_page(viewport={"width": 1280, "height": 720})
    pg.on("pageerror", lambda e: errors.append(str(e)))
    if jszip:
        pg.route("**/jszip.min.js", lambda r: r.fulfill(path=jszip, content_type="application/javascript"))
    pg.goto(f"http://127.0.0.1:{srv.server_address[1]}/index.html")
    pg.wait_for_selector("#status:not(:has-text('Loading engine'))", timeout=30000)
    pg.set_input_files("#file", apk)
    pg.wait_for_function("!document.getElementById('ready').hidden", timeout=90000)
    pg.click("#start")
    time.sleep(3)
    # the menu must have drawn more than a flat colour (WebGL canvases can't be read back reliably,
    # so analyse a real screenshot instead)
    import io
    from PIL import Image
    img = Image.open(io.BytesIO(pg.screenshot())).convert("RGB").resize((64, 36))
    colours = len({(r >> 4, g >> 4, bl >> 4) for r, g, bl in img.getdata()})
    b.close()
if errors or colours < 20:
    print("FAIL", errors, "distinct colours:", colours); sys.exit(1)
print("web smoke OK, distinct colours:", colours)

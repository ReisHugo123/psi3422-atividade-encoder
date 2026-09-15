# -*- coding: utf-8 -*-
"""HTML -> PDF por Chrome headless via DevTools Protocol.

Igual ao topdf.py de PSI3482, com uma diferenca: displayHeaderFooter=True com
headerTemplate proprio, para carimbar o numero da pagina no alto a direita, que
e como estao os relatorios das atividades 1 e 2.
"""
import base64
import json
import os
import subprocess
import sys
import time
import urllib.request

import websocket

CHROME = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
PORT = 9391

src = os.path.abspath(sys.argv[1])
out = os.path.abspath(sys.argv[2])
url = "file:///" + src.replace("\\", "/")

profile = os.path.join(os.environ["TEMP"], "cdp_psi3422_a4")
proc = subprocess.Popen([
    CHROME, "--headless=old", "--disable-gpu", "--no-first-run",
    "--no-default-browser-check", "--disable-extensions",
    "--remote-debugging-port=%d" % PORT, "--remote-allow-origins=*",
    "--user-data-dir=" + profile, url,
])

ws_url = None
for _ in range(80):
    try:
        data = json.load(urllib.request.urlopen("http://127.0.0.1:%d/json" % PORT))
        for t in data:
            if t.get("type") == "page" and t.get("webSocketDebuggerUrl"):
                ws_url = t["webSocketDebuggerUrl"]
                break
        if ws_url:
            break
    except Exception:
        pass
    time.sleep(0.25)

if not ws_url:
    print("ERRO: nao conectou ao Chrome")
    proc.terminate()
    sys.exit(1)

ws = websocket.create_connection(ws_url, max_size=None)
mid = 0


def cmd(method, params=None):
    global mid
    mid += 1
    meu = mid
    ws.send(json.dumps({"id": meu, "method": method, "params": params or {}}))
    while True:
        msg = json.loads(ws.recv())
        if msg.get("id") == meu:
            if "error" in msg:
                raise RuntimeError(msg["error"])
            return msg.get("result", {})


CAB = ('<div style="width:100%;font-size:9px;font-family:Arial,sans-serif;'
       'color:#000;padding:0 2.2cm 0 0;text-align:right;">'
       '<span class="pageNumber"></span></div>')
ROD = '<div style="display:none"></div>'

time.sleep(1.2)
r = cmd("Page.printToPDF", {
    "paperWidth": 8.27, "paperHeight": 11.69,
    "marginTop": 1.102, "marginBottom": 0.866,
    "marginLeft": 1.181, "marginRight": 0.866,
    "printBackground": True,
    "displayHeaderFooter": True,
    "headerTemplate": CAB,
    "footerTemplate": ROD,
    "preferCSSPageSize": False,
})

with open(out, "wb") as f:
    f.write(base64.b64decode(r["data"]))

ws.close()
proc.terminate()
print("gerado:", out, os.path.getsize(out), "bytes")

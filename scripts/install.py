"""Install the separate Zynaps app through Booster; never erase global settings.

Requires the Atari/Sidecart to be in Booster, and inbound LAN TCP on --port.
The temporary server exposes one public UF2 to the Sidecart IP only, then stops.
"""
import argparse
import base64
import hashlib
import http.server
import json
import pathlib
import socket
import subprocess
import sys
import threading
import time
import urllib.parse
import urllib.request

parser = argparse.ArgumentParser()
parser.add_argument('--host', default='sidecart.local')
parser.add_argument('--port', type=int, default=8766)
parser.add_argument('--launch', action='store_true')
parser.add_argument('--first-install', action='store_true',
                    help='Register a new Zynaps settings slot; use only before Zynaps has settings.')
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
firmware = root / 'dist/ZYNAPS-SC.uf2'
subprocess.run([sys.executable, str(root / 'scripts/check-firmware.py'), str(firmware)], check=True)
uuid = '5a595749-4445-4000-8000-000000000001'
base = 'http://' + args.host

def get(path):
    with urllib.request.urlopen(base + path, timeout=15) as response:
        return response.read().decode('utf-8', errors='replace')

# Check a Booster-only, read-only route before starting any mutation.
try:
    initial = json.loads(get('/dlstat.shtml').strip())
    assert 'ph' in initial
except Exception:
    sys.exit('Booster not available. Return to the DevOps setup menu and press X first.')
if initial['ph'] not in (0, 5, 6):
    sys.exit('Booster is already downloading something; leaving it untouched.')
peer = socket.gethostbyname(args.host)
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route:
    route.connect((peer, 80))
    local_ip = route.getsockname()[0]
data = firmware.read_bytes()
served = threading.Event()

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.client_address[0] != peer or self.path != '/ZYNAPS-SC.uf2':
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Connection', 'close')
        self.end_headers()
        self.wfile.write(data)
        served.set()

    def log_message(self, *_):
        pass

manifest = {
    'uuid': uuid, 'name': 'Zynaps Wide GPU',
    'description': 'Zynaps Wide accelerator; original game files required on SD.',
    'image': '', 'tags': ['Development', 'GEMDRIVE'],
    'devices': ['ST', 'STE', 'MegaST', 'MegaSTE'],
    'binary': f'http://{local_ip}:{args.port}/ZYNAPS-SC.uf2',
    'md5': hashlib.md5(data).hexdigest(),
    'version':(root/'VERSION').read_text().strip()+'-zynaps-wide',
}
encoded = base64.b64encode(json.dumps(manifest, separators=(',', ':')).encode()).decode()
server = http.server.ThreadingHTTPServer((local_ip, args.port), Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    print(f'Serving verified UF2 only to {peer} on {local_ip}:{args.port}', flush=True)
    # Booster's CGI expects raw base64, as its own UI sends; it does not
    # percent-decode parameter values before decoding the JSON payload.
    # An update preserves an existing settings slot, but DOES NOT create one.
    # A new UUID must be registered once or its startup returns to Booster.
    prefix = '' if args.first_install else 'update=Y&'
    page = get('/download.cgi?' + prefix + 'json=' + encoded)
    if 'error.shtml' in page:
        raise RuntimeError('Booster rejected the app metadata')
    deadline = time.monotonic() + 180
    last = None
    while time.monotonic() < deadline:
        try:
            state = json.loads(get('/dlstat.shtml').strip())
        except (TimeoutError, OSError):
            # Flash operations may temporarily stop HTTP. Keep serving the
            # same image; do not initiate another installation on timeout.
            print('Booster busy; waiting for installation status.', flush=True)
            time.sleep(2)
            continue
        marker = (state.get('ph'), state.get('pct'))
        if marker != last:
            print(f"Booster phase={marker[0]} progress={marker[1]}%", flush=True)
            last = marker
        if state.get('ph') == 6:
            raise RuntimeError('Booster download failed: ' + state.get('msg', 'unknown'))
        if state.get('ph') == 5 and served.is_set():
            if state.get('rx') != len(data):
                raise RuntimeError('Unexpected installed binary size')
            print('Booster reports download, checksum verification and installation complete.', flush=True)
            break
        time.sleep(2)
    else:
        raise TimeoutError('Booster did not finish; firmware was not launched')
finally:
    server.shutdown()
    server.server_close()
    thread.join()
if args.launch:
    get('/mngr_launchapp.cgi?uuid=' + uuid)
    print('Zynaps microfirmware launch requested. Verify /api/v1/ping after reboot.', flush=True)

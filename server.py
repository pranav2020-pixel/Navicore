#!/usr/bin/env python3
"""
NaviCore Route Planner Server
Lightweight HTTP bridge between the web UI and the C++ routing binary.
Maps are discovered dynamically from data/maps.csv.
"""

import csv
import http.server
import json
import os
import socketserver
import subprocess
import urllib.parse

PORT = 8000
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(BASE_DIR, "data")
WEB_DIR = os.path.join(BASE_DIR, "web")
ENGINE_EXE = os.path.join(BASE_DIR, "nav_engine.exe")


def load_maps():
    maps_csv = os.path.join(DATA_DIR, "maps.csv")
    maps = []
    if os.path.isfile(maps_csv):
        with open(maps_csv, newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                map_id = (row.get("id") or "").strip()
                label = (row.get("label") or map_id).strip()
                if map_id:
                    maps.append({"id": map_id, "label": label})
    return maps


class NavigationRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def _send_json(self, payload, status=200):
        body = payload.encode("utf-8") if isinstance(payload, str) else json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path == "/api/status":
            self._send_json({
                "status": "online",
                "engine": "C++ route planner",
                "binary": os.path.basename(ENGINE_EXE),
                "binaryExists": os.path.isfile(ENGINE_EXE),
            })
            return

        if path == "/api/maps":
            self._send_json({"maps": load_maps()})
            return

        if path == "/api/graph":
            maps = load_maps()
            valid_ids = [m["id"] for m in maps]
            query = urllib.parse.parse_qs(parsed.query)
            requested = query.get("map", [valid_ids[0] if valid_ids else ""])[0]
            if requested not in valid_ids:
                self._send_json({"error": f"Unknown map '{requested}'"}, status=404)
                return

            json_file = os.path.join(WEB_DIR, f"{requested}_graph.json")
            if os.path.isfile(json_file):
                with open(json_file, "r", encoding="utf-8") as f:
                    self._send_json(f.read())
                return

            if os.path.isfile(ENGINE_EXE):
                proc = subprocess.run(
                    [ENGINE_EXE, "--map", requested, "--export-graph"],
                    cwd=BASE_DIR,
                    capture_output=True,
                    text=True,
                    timeout=5,
                )
                if proc.returncode == 0 and proc.stdout.strip():
                    self._send_json(proc.stdout.strip())
                    return

            self._send_json({"error": "Map data could not be loaded"}, status=404)
            return

        return super().do_GET()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path != "/api/route":
            self.send_error(404, "Endpoint not found")
            return

        content_length = int(self.headers.get("Content-Length", 0))
        post_data = self.rfile.read(content_length).decode("utf-8")
        try:
            params = json.loads(post_data)
        except Exception as e:
            self._send_json({"success": False, "statusMessage": f"Invalid request: {e}"}, status=400)
            return

        maps = load_maps()
        valid_ids = [m["id"] for m in maps]
        map_name = params.get("map", valid_ids[0] if valid_ids else "")
        if map_name not in valid_ids:
            self._send_json({"success": False, "statusMessage": f"Unknown map '{map_name}'."}, status=400)
            return

        start = params.get("start", "")
        target = params.get("target", "")
        algo = params.get("algo", "astar")
        metric = params.get("metric", "distance")
        blocked_roads = params.get("blockedRoads", [])

        cmd = [ENGINE_EXE, "--map", map_name, "--route", start, target, algo, metric]
        if blocked_roads:
            cmd.extend(["--block", ",".join(blocked_roads)])

        try:
            proc = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True, timeout=5)
            self._send_json(proc.stdout.strip())
        except Exception as e:
            self._send_json(
                {"success": False, "statusMessage": f"Could not run route search: {str(e)}"},
                status=500,
            )

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()


def run_server():
    os.chdir(WEB_DIR)
    socketserver.TCPServer.allow_reuse_address = True
    try:
        with socketserver.TCPServer(("", PORT), NavigationRequestHandler) as httpd:
            print("================================================================")
            print(f"[RUNNING] Route Planner Server at http://localhost:{PORT}")
            print(f"[STATIC]  Serving web files from: {WEB_DIR}")
            print(f"[ENGINE]  C++ Engine binary: {ENGINE_EXE}")
            print("================================================================")
            httpd.serve_forever()
    except OSError:
        print(f"Port {PORT} in use, trying 8080...")
        with socketserver.TCPServer(("", 8080), NavigationRequestHandler) as httpd:
            print("[RUNNING] Route Planner Server at http://localhost:8080")
            httpd.serve_forever()


if __name__ == "__main__":
    run_server()

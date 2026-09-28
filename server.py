#!/usr/bin/env python3
"""
Campus & Smart City Navigation Server
Lightweight HTTP Bridge between Web Frontend and C++ Navigation Engine
"""

import http.server
import socketserver
import json
import subprocess
import os
import sys
import urllib.parse

PORT = 8000
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
WEB_DIR = os.path.join(BASE_DIR, "web")
ENGINE_EXE = os.path.join(BASE_DIR, "nav_engine.exe")

class NavigationRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path == "/api/status":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            resp = {
                "status": "online",
                "engine": "C++ High Performance Navigation Engine",
                "binary": os.path.basename(ENGINE_EXE),
                "binaryExists": os.path.isfile(ENGINE_EXE)
            }
            self.wfile.write(json.dumps(resp).encode("utf-8"))
            return

        if path == "/api/graph":
            query = urllib.parse.parse_qs(parsed.query)
            map_name = query.get("map", ["campus"])[0]
            if map_name not in ["campus", "city"]:
                map_name = "campus"

            json_file = os.path.join(WEB_DIR, f"{map_name}_graph.json")
            if os.path.isfile(json_file):
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                with open(json_file, "rb") as f:
                    self.wfile.write(f.read())
            else:
                self.send_error(404, "Graph data not found")
            return

        # Default static file handling
        return super().do_GET()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path == "/api/route":
            content_length = int(self.headers.get("Content-Length", 0))
            post_data = self.rfile.read(content_length).decode("utf-8")
            try:
                params = json.loads(post_data)
            except Exception as e:
                self.send_error(400, f"Invalid JSON payload: {e}")
                return

            map_name = params.get("map", "campus")
            start = params.get("start", "")
            target = params.get("target", "")
            algo = params.get("algo", "astar")
            metric = params.get("metric", "distance")
            blocked_roads = params.get("blockedRoads", [])

            cmd = [
                ENGINE_EXE,
                "--map", map_name,
                "--route", start, target, algo, metric
            ]
            if blocked_roads:
                cmd.extend(["--block", ",".join(blocked_roads)])

            try:
                proc = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True, timeout=5)
                output = proc.stdout.strip()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(output.encode("utf-8"))
            except Exception as e:
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                err_resp = {"success": False, "statusMessage": f"C++ execution error: {str(e)}"}
                self.wfile.write(json.dumps(err_resp).encode("utf-8"))
            return

        self.send_error(404, "Endpoint not found")

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

def run_server():
    os.chdir(WEB_DIR)
    socketserver.TCPServer.allow_reuse_address = True
    server_address = ("", PORT)
    try:
        with socketserver.TCPServer(server_address, NavigationRequestHandler) as httpd:
            print("================================================================")
            print(f"[RUNNING] Navigation Engine Web Server at http://localhost:{PORT}")
            print(f"[STATIC]  Serving web files from: {WEB_DIR}")
            print(f"[ENGINE]  C++ Engine binary: {ENGINE_EXE}")
            print("================================================================")
            httpd.serve_forever()
    except OSError as e:
        print(f"Port {PORT} in use, trying 8080...")
        with socketserver.TCPServer(("", 8080), NavigationRequestHandler) as httpd:
            print(f"[RUNNING] Navigation Engine Web Server at http://localhost:8080")
            httpd.serve_forever()

if __name__ == "__main__":
    run_server()

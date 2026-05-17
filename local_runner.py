from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote
import hashlib
import json
import os
import secrets
import subprocess
import sys
import time

ROOT = Path(__file__).parent
DATA = ROOT / "data"
SUBMISSIONS = ROOT / "submissions"
FRONTEND = ROOT / "frontend"
USERS_FILE = DATA / "users.txt"
SUBMISSIONS_FILE = DATA / "submissions.txt"
PROBLEMS_FILE = DATA / "problems.json"
GPP = Path("C:/MinGW/bin/g++.exe")
GPP_COMMAND = str(GPP) if GPP.exists() else "g++"

sessions = {}


def safe_log(message):
    if sys.stdout:
        print(message)


def ensure_files():
    DATA.mkdir(exist_ok=True)
    SUBMISSIONS.mkdir(exist_ok=True)
    USERS_FILE.touch(exist_ok=True)
    SUBMISSIONS_FILE.touch(exist_ok=True)


def password_hash(password):
    return hashlib.sha256(("codearena:" + password).encode()).hexdigest()


def load_users():
    ensure_files()
    users = []
    for line in USERS_FILE.read_text(encoding="utf-8").splitlines():
        parts = line.split("|")
        if len(parts) >= 3:
            users.append({"username": parts[0], "password_hash": parts[1], "score": int(parts[2])})
    return users


def save_users(users):
    USERS_FILE.write_text(
        "".join(f"{u['username']}|{u['password_hash']}|{u['score']}\n" for u in users),
        encoding="utf-8",
    )


def load_submissions():
    ensure_files()
    rows = []
    for line in SUBMISSIONS_FILE.read_text(encoding="utf-8").splitlines():
        parts = line.split("|")
        if len(parts) >= 6:
            rows.append(
                {
                    "id": parts[0],
                    "username": parts[1],
                    "problemId": int(parts[2]),
                    "status": parts[3],
                    "score": int(parts[4]),
                    "timestamp": parts[5],
                }
            )
    return rows


def append_submission(row):
    with SUBMISSIONS_FILE.open("a", encoding="utf-8") as handle:
        handle.write(
            f"{row['id']}|{row['username']}|{row['problemId']}|{row['status']}|{row['score']}|{row['timestamp']}\n"
        )


def timestamp():
    return time.strftime("%Y-%m-%d %H:%M:%S")


def problem_score(problem_id):
    return {1: 100, 2: 150, 3: 200}.get(problem_id, 50)


def test_count(problem_id):
    folder = ROOT / "samples" / f"problem{problem_id}"
    count = 0
    while (folder / f"input{count + 1}.txt").exists():
        count += 1
    return count


def normalize(text):
    return text.replace("\r", "").rstrip(" \t\n")


def judge_cpp(username, problem_id, code):
    submission_id = str(time.time_ns())
    base = SUBMISSIONS / submission_id
    base.mkdir(parents=True, exist_ok=True)
    source = base / "main.cpp"
    exe = base / "main.exe"
    source.write_text(code, encoding="utf-8")

    compile_result = subprocess.run(
        [GPP_COMMAND, str(source), "-std=c++17", "-O2", "-o", str(exe)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=20,
    )
    if compile_result.returncode != 0:
        (base / "compile.log").write_text(compile_result.stderr or compile_result.stdout or "Compilation failed", encoding="utf-8")
        return {
            "id": submission_id,
            "username": username,
            "problemId": problem_id,
            "status": "Compilation Error",
            "score": 0,
            "timestamp": timestamp(),
        }

    total = test_count(problem_id)
    passed = 0
    for idx in range(1, total + 1):
        folder = ROOT / "samples" / f"problem{problem_id}"
        input_file = folder / f"input{idx}.txt"
        expected_file = folder / f"output{idx}.txt"
        actual_file = base / f"actual{idx}.txt"
        try:
            with input_file.open("r", encoding="utf-8") as stdin, actual_file.open("w", encoding="utf-8") as stdout:
                result = subprocess.run([str(exe)], stdin=stdin, stdout=stdout, stderr=subprocess.DEVNULL, timeout=3)
            if result.returncode == 0 and normalize(actual_file.read_text(encoding="utf-8")) == normalize(
                expected_file.read_text(encoding="utf-8")
            ):
                passed += 1
        except subprocess.TimeoutExpired:
            pass

    score = 0 if total == 0 else problem_score(problem_id) * passed // total
    status = "No Tests Found" if total == 0 else "Accepted" if passed == total else "Wrong Answer"
    return {
        "id": submission_id,
        "username": username,
        "problemId": problem_id,
        "status": status,
        "score": score,
        "timestamp": timestamp(),
    }


def add_score_if_best(username, problem_id, score):
    old_best = 0
    for row in load_submissions():
        if row["username"] == username and row["problemId"] == problem_id:
            old_best = max(old_best, row["score"])
    delta = max(0, score - old_best)
    if delta == 0:
        return
    users = load_users()
    for user in users:
        if user["username"] == username:
            user["score"] += delta
    save_users(users)


class Handler(BaseHTTPRequestHandler):
    def send_json(self, data, status=200):
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()
        self.wfile.write(body)

    def current_user(self):
        auth = self.headers.get("Authorization", "")
        if not auth.startswith("Bearer "):
            return ""
        return sessions.get(auth.removeprefix("Bearer "), "")

    def read_body(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length == 0:
            return {}
        return json.loads(self.rfile.read(length).decode("utf-8"))

    def do_OPTIONS(self):
        self.send_json({})

    def do_GET(self):
        path = unquote(self.path.split("?", 1)[0])
        if path == "/api/problems":
            self.send_json(json.loads(PROBLEMS_FILE.read_text(encoding="utf-8")))
            return
        if path == "/api/leaderboard":
            rows = sorted(load_users(), key=lambda user: (-user["score"], user["username"]))
            self.send_json(
                [{"rank": idx + 1, "username": row["username"], "score": row["score"]} for idx, row in enumerate(rows)]
            )
            return
        if path == "/api/submissions":
            username = self.current_user()
            if not username:
                self.send_json({"error": "Login required"}, 401)
                return
            rows = [row for row in reversed(load_submissions()) if row["username"] == username]
            self.send_json(rows)
            return
        self.serve_static(path)

    def do_POST(self):
        path = self.path.split("?", 1)[0]
        try:
            body = self.read_body()
            if path == "/api/register":
                username = body.get("username", "").strip()
                password = body.get("password", "")
                if len(username) < 3 or len(password) < 4:
                    self.send_json({"error": "Username or password too short"}, 400)
                    return
                users = load_users()
                if any(user["username"] == username for user in users):
                    self.send_json({"error": "User already exists"}, 409)
                    return
                users.append({"username": username, "password_hash": password_hash(password), "score": 0})
                save_users(users)
                self.send_json({"message": "Registered successfully"}, 201)
                return
            if path == "/api/login":
                username = body.get("username", "").strip()
                hashed = password_hash(body.get("password", ""))
                for user in load_users():
                    if user["username"] == username and user["password_hash"] == hashed:
                        token = secrets.token_hex(24)
                        sessions[token] = username
                        self.send_json({"token": token, "username": username})
                        return
                self.send_json({"error": "Invalid username or password"}, 401)
                return
            if path == "/api/submit":
                username = self.current_user()
                if not username:
                    self.send_json({"error": "Login required"}, 401)
                    return
                result = judge_cpp(username, int(body.get("problemId", 0)), body.get("code", ""))
                add_score_if_best(username, result["problemId"], result["score"])
                append_submission(result)
                self.send_json(result)
                return
            self.send_json({"error": "Not found"}, 404)
        except Exception as exc:
            self.send_json({"error": str(exc)}, 500)

    def serve_static(self, path):
        if path in ("", "/"):
            target = FRONTEND / "index.html"
        else:
            clean = path.lstrip("/")
            if clean.startswith("frontend/"):
                clean = clean[len("frontend/") :]
            target = FRONTEND / clean

        if not target.exists() or not target.is_file():
            self.send_error(404)
            return

        content_type = "text/html"
        if target.suffix == ".css":
            content_type = "text/css"
        elif target.suffix == ".js":
            content_type = "application/javascript"

        body = target.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        safe_log("%s - %s" % (self.address_string(), format % args))


if __name__ == "__main__":
    try:
        ensure_files()
        (ROOT / "server.pid").write_text(str(os.getpid()), encoding="utf-8")
        server = ThreadingHTTPServer(("localhost", 18080), Handler)
        safe_log("CodeArena local runner is live at http://localhost:18080")
        server.serve_forever()
    except Exception as exc:
        (ROOT / "server.err").write_text(str(exc), encoding="utf-8")
        raise

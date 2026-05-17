# CodeArena - Online Coding Judge System

CodeArena is a full-stack coding judge platform inspired by LeetCode and HackerRank. It includes authentication, automated C++ answer evaluation, score management, ranking, submission history, and a responsive frontend.

## Tech Stack

- C++17
- Crow Framework
- REST API
- HTML, CSS, JavaScript
- File handling for users, submissions, scores, and problem data
- g++ compiler for submitted solutions

## Features

- User registration and login
- Token-based session handling
- Problem list with statements and sample I/O
- C++ code submission from the browser
- Automated compile and run against hidden sample test files
- Accepted / Wrong Answer / Compilation Error results
- Best-score based leaderboard
- Personal submission history
- Responsive frontend layout

## Project Structure

```text
CodeArena/
  backend/
    main.cpp
    CMakeLists.txt
    crow.h              # Crow single-header dependency
  data/
    problems.json
    users.txt           # Auto-created/updated
    submissions.txt     # Auto-created/updated
  frontend/
    index.html
    styles.css
    app.js
  samples/
    problem1/
    problem2/
    problem3/
  submissions/          # Auto-created judged code/output files
```

## Setup

## Quick Run On This Laptop

This repository includes a no-install local runner so you can test the full frontend, authentication, judging, scoring, and leaderboard immediately:

```powershell
cd C:\Users\Dell\Documents\Codex\2026-05-16\i-want-to-make-this-project
.\RUN_CODEARENA.ps1
```

Then open:

```text
http://localhost:18080
```

To stop the local runner:

```powershell
.\STOP_CODEARENA.ps1
```

The local runner uses Python only to host the demo API because this laptop currently does not have CMake or Crow installed. Submitted solutions are still compiled and judged with `g++`.

## C++ Crow Build

1. Install a C++ compiler.

   On Windows, install MinGW-w64 or MSYS2 and make sure `g++` is available in your terminal.

2. Crow is already included as `backend/crow.h`.

3. Build the backend from the project root.

   ```powershell
   cmake -S backend -B backend/build
   cmake --build backend/build
   ```

4. Run the backend from the project root.

   ```powershell
   .\backend\build\Debug\codearena.exe
   ```

   If your compiler creates a Release build, run:

   ```powershell
   .\backend\build\Release\codearena.exe
   ```

5. Open the frontend.

   Open this file in your browser:

   ```text
   frontend/index.html
   ```

The API runs on `http://localhost:18080`.

## API Endpoints

| Method | Endpoint | Purpose |
|---|---|---|
| POST | `/api/register` | Create a new user |
| POST | `/api/login` | Login and receive token |
| GET | `/api/problems` | List coding problems |
| POST | `/api/submit` | Submit C++ code for judging |
| GET | `/api/leaderboard` | View ranked users |
| GET | `/api/submissions` | View logged-in user's submissions |

## Example Accepted Solution: Two Sum

```cpp
#include <bits/stdc++.h>
using namespace std;

int main() {
    int n, target;
    cin >> n >> target;
    vector<int> a(n);
    for (int &x : a) cin >> x;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (a[i] + a[j] == target) {
                cout << i + 1 << " " << j + 1 << "\n";
                return 0;
            }
        }
    }
    cout << -1 << "\n";
    return 0;
}
```

## Important Notes

- This is an educational project. Running arbitrary code on a real public server requires sandboxing, time limits, memory limits, Docker/isolation, and stronger security.
- Password hashing here is intentionally lightweight for demonstration. Use a real password hashing library such as bcrypt/Argon2 in production.
- The judge compares exact output. Extra spaces or missing newlines may affect results.

## Future Enhancements

- Docker-based sandbox for secure code execution
- Admin panel for adding problems and test cases
- Multi-language support
- Time and memory limit tracking
- Database storage with MySQL/PostgreSQL
- Contest mode with timed rankings

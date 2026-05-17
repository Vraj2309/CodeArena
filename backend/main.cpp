#include "crow.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <io.h>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

const std::string DATA_DIR = "data";
const std::string SUBMISSION_DIR = "submissions";
const std::string USERS_FILE = DATA_DIR + "/users.txt";
const std::string SUBMISSIONS_FILE = DATA_DIR + "/submissions.txt";
const std::string PROBLEMS_FILE = DATA_DIR + "/problems.json";

struct User {
    std::string username;
    std::string password_hash;
    int score = 0;
};

struct Submission {
    std::string id;
    std::string username;
    int problem_id = 0;
    std::string status;
    int score = 0;
    std::string timestamp;
};

std::unordered_map<std::string, std::string> sessions;

std::string now() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
    std::tm* local = std::localtime(&t);
    if (local) tm = *local;
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string hash_password(const std::string& password) {
    return std::to_string(std::hash<std::string>{}("codearena:" + password));
}

std::string make_token() {
    static std::mt19937_64 rng(std::random_device{}());
    std::ostringstream out;
    out << std::hex << rng() << rng();
    return out.str();
}

std::string escape_json(const std::string& text) {
    std::ostringstream out;
    for (char c : text) {
        if (c == '"') out << "\\\"";
        else if (c == '\\') out << "\\\\";
        else if (c == '\n') out << "\\n";
        else if (c == '\r') out << "\\r";
        else out << c;
    }
    return out.str();
}

std::vector<std::string> split(const std::string& text, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, delim)) parts.push_back(item);
    return parts;
}

bool path_exists(const std::string& path) {
    return _access(path.c_str(), 0) == 0;
}

void make_dir(const std::string& path) {
    if (!path_exists(path)) _mkdir(path.c_str());
}

void ensure_files() {
    make_dir(DATA_DIR);
    make_dir(SUBMISSION_DIR);
    if (!path_exists(USERS_FILE)) std::ofstream(USERS_FILE).close();
    if (!path_exists(SUBMISSIONS_FILE)) std::ofstream(SUBMISSIONS_FILE).close();
}

std::vector<User> load_users() {
    ensure_files();
    std::vector<User> users;
    std::ifstream in(USERS_FILE);
    std::string line;
    while (std::getline(in, line)) {
        auto p = split(line, '|');
        if (p.size() >= 3) users.push_back({p[0], p[1], std::stoi(p[2])});
    }
    return users;
}

void save_users(const std::vector<User>& users) {
    std::ofstream out(USERS_FILE, std::ios::trunc);
    for (const auto& u : users) {
        out << u.username << "|" << u.password_hash << "|" << u.score << "\n";
    }
}

std::vector<Submission> load_submissions() {
    ensure_files();
    std::vector<Submission> submissions;
    std::ifstream in(SUBMISSIONS_FILE);
    std::string line;
    while (std::getline(in, line)) {
        auto p = split(line, '|');
        if (p.size() >= 6) {
            submissions.push_back({p[0], p[1], std::stoi(p[2]), p[3], std::stoi(p[4]), p[5]});
        }
    }
    return submissions;
}

void append_submission(const Submission& s) {
    std::ofstream out(SUBMISSIONS_FILE, std::ios::app);
    out << s.id << "|" << s.username << "|" << s.problem_id << "|" << s.status << "|" << s.score << "|" << s.timestamp << "\n";
}

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string normalize_output(std::string text) {
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    while (!text.empty() && (text.back() == '\n' || text.back() == ' ' || text.back() == '\t')) {
        text.pop_back();
    }
    return text;
}

bool user_exists(const std::string& username) {
    auto users = load_users();
    return std::any_of(users.begin(), users.end(), [&](const User& u) { return u.username == username; });
}

std::string username_from_request(const crow::request& req) {
    auto auth = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (auth.rfind(prefix, 0) != 0) return "";
    auto token = auth.substr(prefix.size());
    auto it = sessions.find(token);
    return it == sessions.end() ? "" : it->second;
}

int problem_score(int problem_id) {
    if (problem_id == 1) return 100;
    if (problem_id == 2) return 150;
    if (problem_id == 3) return 200;
    return 50;
}

int test_count(int problem_id) {
    std::string dir = "samples/problem" + std::to_string(problem_id);
    int count = 0;
    while (path_exists(dir + "/input" + std::to_string(count + 1) + ".txt")) count++;
    return count;
}

std::string run_command(const std::string& command) {
    return std::system(command.c_str()) == 0 ? "ok" : "fail";
}

Submission judge_cpp(const std::string& username, int problem_id, const std::string& code) {
    ensure_files();
    std::string id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string base = SUBMISSION_DIR + "/" + id;
    make_dir(base);
    std::string source = base + "/main.cpp";
    std::string exe = base + "/main.exe";
    {
        std::ofstream out(source);
        out << code;
    }

    std::string compile = "g++ \"" + source + "\" -std=c++17 -O2 -o \"" + exe + "\"";
    if (run_command(compile) != "ok") {
        return {id, username, problem_id, "Compilation Error", 0, now()};
    }

    int total = test_count(problem_id);
    int passed = 0;
    for (int i = 1; i <= total; i++) {
        std::string test_dir = "samples/problem" + std::to_string(problem_id);
        std::string input = test_dir + "/input" + std::to_string(i) + ".txt";
        std::string expected = test_dir + "/output" + std::to_string(i) + ".txt";
        std::string actual = base + "/actual" + std::to_string(i) + ".txt";

#ifdef _WIN32
        std::string command = "\"" + exe + "\" < \"" + input + "\" > \"" + actual + "\"";
#else
        std::string command = "timeout 3s \"" + exe + "\" < \"" + input + "\" > \"" + actual + "\"";
#endif
        if (run_command(command) == "ok" && normalize_output(read_file(actual)) == normalize_output(read_file(expected))) {
            passed++;
        }
    }

    int score = total == 0 ? 0 : (problem_score(problem_id) * passed / total);
    std::string status = passed == total ? "Accepted" : "Wrong Answer";
    if (total == 0) status = "No Tests Found";
    return {id, username, problem_id, status, score, now()};
}

void add_score_if_best(const std::string& username, int problem_id, int new_score) {
    auto submissions = load_submissions();
    int old_best = 0;
    for (const auto& s : submissions) {
        if (s.username == username && s.problem_id == problem_id) old_best = std::max(old_best, s.score);
    }
    int delta = std::max(0, new_score - old_best);
    if (delta == 0) return;

    auto users = load_users();
    for (auto& u : users) {
        if (u.username == username) {
            u.score += delta;
            break;
        }
    }
    save_users(users);
}

crow::response json_response(const std::string& body, int status = 200) {
    crow::response res(status, body);
    res.set_header("Content-Type", "application/json");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    return res;
}

int main() {
    ensure_files();
    crow::SimpleApp app;

    CROW_ROUTE(app, "/api/register").methods("POST"_method, "OPTIONS"_method)([](const crow::request& req) {
        if (req.method == "OPTIONS"_method) return json_response("{}");
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password")) return json_response("{\"error\":\"Invalid request\"}", 400);
        std::string username = body["username"].s();
        std::string password = body["password"].s();
        if (username.size() < 3 || password.size() < 4) return json_response("{\"error\":\"Username or password too short\"}", 400);
        if (user_exists(username)) return json_response("{\"error\":\"User already exists\"}", 409);
        auto users = load_users();
        users.push_back({username, hash_password(password), 0});
        save_users(users);
        return json_response("{\"message\":\"Registered successfully\"}", 201);
    });

    CROW_ROUTE(app, "/api/login").methods("POST"_method, "OPTIONS"_method)([](const crow::request& req) {
        if (req.method == "OPTIONS"_method) return json_response("{}");
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password")) return json_response("{\"error\":\"Invalid request\"}", 400);
        std::string username = body["username"].s();
        std::string password_hash = hash_password(body["password"].s());
        for (const auto& u : load_users()) {
            if (u.username == username && u.password_hash == password_hash) {
                std::string token = make_token();
                sessions[token] = username;
                return json_response("{\"token\":\"" + token + "\",\"username\":\"" + escape_json(username) + "\"}");
            }
        }
        return json_response("{\"error\":\"Invalid username or password\"}", 401);
    });

    CROW_ROUTE(app, "/api/problems").methods("GET"_method)([] {
        return json_response(read_file(PROBLEMS_FILE));
    });

    CROW_ROUTE(app, "/api/submit").methods("POST"_method, "OPTIONS"_method)([](const crow::request& req) {
        if (req.method == "OPTIONS"_method) return json_response("{}");
        std::string username = username_from_request(req);
        if (username.empty()) return json_response("{\"error\":\"Login required\"}", 401);
        auto body = crow::json::load(req.body);
        if (!body || !body.has("problemId") || !body.has("code")) return json_response("{\"error\":\"Invalid submission\"}", 400);

        int problem_id = body["problemId"].i();
        std::string code = body["code"].s();
        Submission result = judge_cpp(username, problem_id, code);
        add_score_if_best(username, problem_id, result.score);
        append_submission(result);

        std::ostringstream out;
        out << "{\"id\":\"" << result.id << "\",\"status\":\"" << result.status << "\",\"score\":" << result.score
            << ",\"timestamp\":\"" << result.timestamp << "\"}";
        return json_response(out.str());
    });

    CROW_ROUTE(app, "/api/leaderboard").methods("GET"_method)([] {
        auto users = load_users();
        std::sort(users.begin(), users.end(), [](const User& a, const User& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.username < b.username;
        });
        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < users.size(); i++) {
            if (i) out << ",";
            out << "{\"rank\":" << (i + 1) << ",\"username\":\"" << escape_json(users[i].username)
                << "\",\"score\":" << users[i].score << "}";
        }
        out << "]";
        return json_response(out.str());
    });

    CROW_ROUTE(app, "/api/submissions").methods("GET"_method)([](const crow::request& req) {
        std::string username = username_from_request(req);
        if (username.empty()) return json_response("{\"error\":\"Login required\"}", 401);
        auto submissions = load_submissions();
        std::reverse(submissions.begin(), submissions.end());
        std::ostringstream out;
        out << "[";
        bool first = true;
        for (const auto& s : submissions) {
            if (s.username != username) continue;
            if (!first) out << ",";
            first = false;
            out << "{\"id\":\"" << s.id << "\",\"problemId\":" << s.problem_id << ",\"status\":\"" << s.status
                << "\",\"score\":" << s.score << ",\"timestamp\":\"" << s.timestamp << "\"}";
        }
        out << "]";
        return json_response(out.str());
    });

    CROW_ROUTE(app, "/")([] {
        return "CodeArena API is running. Open frontend/index.html to use the app.";
    });

    app.port(18080).multithreaded().run();
}

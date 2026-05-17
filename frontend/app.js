const API = "http://localhost:18080/api";

const state = {
    token: localStorage.getItem("codearena_token") || "",
    username: localStorage.getItem("codearena_user") || "",
    authMode: "login",
    problems: [],
    selected: null
};

const $ = (id) => document.getElementById(id);

function setStatus(el, message, tone = "") {
    el.textContent = message;
    el.className = `status ${tone}`;
}

async function request(path, options = {}) {
    const headers = { "Content-Type": "application/json", ...(options.headers || {}) };
    if (state.token) headers.Authorization = `Bearer ${state.token}`;
    const res = await fetch(`${API}${path}`, { ...options, headers });
    const text = await res.text();
    let data = {};
    try {
        data = text ? JSON.parse(text) : {};
    } catch {
        data = { error: text };
    }
    if (!res.ok) throw new Error(data.error || "Request failed");
    return data;
}

function updateSessionUI() {
    $("activeUser").textContent = state.username ? `Logged in as ${state.username}` : "Guest";
    $("logoutBtn").classList.toggle("hidden", !state.token);
}

function problemTemplate(problem) {
    return `#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Write your solution for: ${problem.title}
    return 0;
}
`;
}

function renderProblems() {
    const wrap = $("problems");
    wrap.innerHTML = "";
    state.problems.forEach((problem) => {
        const card = document.createElement("div");
        card.className = `problem-card ${state.selected?.id === problem.id ? "active" : ""}`;
        card.innerHTML = `
            <h3>${problem.title}</h3>
            <p>${problem.short}</p>
            <div class="meta">
                <span class="badge">${problem.difficulty}</span>
                <span class="badge">${problem.score} pts</span>
            </div>
        `;
        card.addEventListener("click", () => selectProblem(problem.id));
        wrap.appendChild(card);
    });
}

function selectProblem(id) {
    state.selected = state.problems.find((p) => p.id === id);
    if (!state.selected) return;
    $("selectedTitle").textContent = state.selected.title;
    $("selectedMeta").textContent = `${state.selected.difficulty} difficulty`;
    $("selectedScore").textContent = `${state.selected.score} pts`;
    $("statement").innerHTML = `
        <p>${state.selected.statement}</p>
        <p><strong>Input:</strong> ${state.selected.input}</p>
        <p><strong>Output:</strong> ${state.selected.output}</p>
        <pre>${state.selected.sample}</pre>
    `;
    $("code").value = problemTemplate(state.selected);
    renderProblems();
}

async function loadProblems() {
    state.problems = await request("/problems");
    if (!state.selected && state.problems.length) state.selected = state.problems[0];
    renderProblems();
    if (state.selected) selectProblem(state.selected.id);
}

async function loadLeaderboard() {
    const rows = await request("/leaderboard");
    $("leaderboard").innerHTML = rows.length
        ? rows.map((row) => `
            <div class="rank-row">
                <span class="rank">#${row.rank}</span>
                <strong>${row.username}</strong>
                <span>${row.score}</span>
            </div>
        `).join("")
        : "<p class='status'>No scores yet.</p>";
}

async function loadSubmissions() {
    if (!state.token) {
        $("submissions").innerHTML = "<p class='status'>Login to view submissions.</p>";
        return;
    }
    const rows = await request("/submissions");
    $("submissions").innerHTML = rows.length
        ? rows.map((row) => `
            <div class="submission-row">
                <strong>P${row.problemId}</strong>
                <span class="${row.status === "Accepted" ? "ok" : "bad"}">${row.status}</span>
                <span>${row.score}</span>
            </div>
        `).join("")
        : "<p class='status'>No submissions yet.</p>";
}

async function refreshAll() {
    await loadProblems();
    await loadLeaderboard();
    await loadSubmissions();
}

document.querySelectorAll(".tab").forEach((tab) => {
    tab.addEventListener("click", () => {
        state.authMode = tab.dataset.mode;
        document.querySelectorAll(".tab").forEach((t) => t.classList.toggle("active", t === tab));
        $("authSubmit").textContent = state.authMode === "login" ? "Login" : "Register";
        setStatus($("authStatus"), "");
    });
});

$("authForm").addEventListener("submit", async (event) => {
    event.preventDefault();
    const username = $("username").value.trim();
    const password = $("password").value;
    try {
        const data = await request(`/${state.authMode}`, {
            method: "POST",
            body: JSON.stringify({ username, password })
        });
        if (state.authMode === "register") {
            setStatus($("authStatus"), "Registered. You can log in now.", "ok");
            return;
        }
        state.token = data.token;
        state.username = data.username;
        localStorage.setItem("codearena_token", state.token);
        localStorage.setItem("codearena_user", state.username);
        updateSessionUI();
        setStatus($("authStatus"), "Login successful.", "ok");
        await loadSubmissions();
    } catch (err) {
        setStatus($("authStatus"), err.message, "bad");
    }
});

$("logoutBtn").addEventListener("click", () => {
    state.token = "";
    state.username = "";
    localStorage.removeItem("codearena_token");
    localStorage.removeItem("codearena_user");
    updateSessionUI();
    loadSubmissions();
});

$("refreshBtn").addEventListener("click", refreshAll);

$("loadTemplateBtn").addEventListener("click", () => {
    if (state.selected) $("code").value = problemTemplate(state.selected);
});

$("submitBtn").addEventListener("click", async () => {
    if (!state.selected) {
        setStatus($("submitStatus"), "Select a problem first.", "warn");
        return;
    }
    if (!state.token) {
        setStatus($("submitStatus"), "Login before submitting.", "warn");
        return;
    }
    $("submitBtn").disabled = true;
    setStatus($("submitStatus"), "Judging your code...");
    try {
        const result = await request("/submit", {
            method: "POST",
            body: JSON.stringify({ problemId: state.selected.id, code: $("code").value })
        });
        setStatus($("submitStatus"), `${result.status} - Score: ${result.score}`, result.status === "Accepted" ? "ok" : "bad");
        await loadLeaderboard();
        await loadSubmissions();
    } catch (err) {
        setStatus($("submitStatus"), err.message, "bad");
    } finally {
        $("submitBtn").disabled = false;
    }
});

updateSessionUI();
refreshAll().catch((err) => {
    $("problems").innerHTML = `<p class="status bad">${err.message}. Start the C++ backend first.</p>`;
});

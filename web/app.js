const API_BASE = window.__API_BASE__ || "/api";

const form = document.getElementById("calc-form");
const resultEl = document.getElementById("result");
const historyEl = document.getElementById("history");
const refreshBtn = document.getElementById("refresh-history");

async function apiFetch(path, options = {}) {
  const response = await fetch(`${API_BASE}${path}`, {
    headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    ...options,
  });

  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(payload.error || `Request failed (${response.status})`);
  }
  return payload;
}

function getIdentity() {
  const userId = document.getElementById("user-id").value.trim();
  const sessionId = document.getElementById("session-id").value.trim();
  return { userId, sessionId };
}

async function loadHistory() {
  const { userId, sessionId } = getIdentity();
  if (!userId || !sessionId) return;

  const query = new URLSearchParams({ userId, sessionId, limit: "20" });
  const data = await apiFetch(`/history?${query.toString()}`);

  historyEl.innerHTML = "";
  for (const item of data.items || []) {
    const li = document.createElement("li");
    li.textContent = `${item.timestamp}: ${item.expression} = ${item.result}`;
    historyEl.appendChild(li);
  }
}

form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const expression = document.getElementById("expression").value.trim();
  const { userId, sessionId } = getIdentity();

  try {
    const data = await apiFetch("/calculate", {
      method: "POST",
      body: JSON.stringify({ expression, userId, sessionId }),
    });
    resultEl.textContent = JSON.stringify(data, null, 2);
    await loadHistory();
  } catch (error) {
    resultEl.textContent = error.message;
  }
});

refreshBtn.addEventListener("click", async () => {
  try {
    await loadHistory();
  } catch (error) {
    resultEl.textContent = error.message;
  }
});

loadHistory().catch(() => {});

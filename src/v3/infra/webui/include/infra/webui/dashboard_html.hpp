// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file dashboard_html.hpp
/// Embedded HTML dashboard served by EmbeddedWebServer.
/// Single-file dashboard with vanilla HTML/CSS/JS, no build step.

#include <string_view>

namespace pvpgn::infra::webui {

/// Embedded HTML dashboard with auto-refresh.
static constexpr std::string_view DASHBOARD_HTML = R"html(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>pvpgn server dashboard</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Courier New', monospace;
      background: linear-gradient(135deg, #0f0f23 0%, #1a1a3f 100%);
      color: #e0e0e0;
      padding: 2em;
      min-height: 100vh;
    }
    h1 {
      color: #00d9ff;
      margin-bottom: 1em;
      font-size: 2em;
      text-shadow: 0 0 10px rgba(0, 217, 255, 0.5);
    }
    h2 {
      color: #00d9ff;
      margin: 1em 0 0.5em 0;
      font-size: 1.3em;
    }
    .container { max-width: 1200px; margin: 0 auto; }
    .card {
      background: rgba(22, 33, 62, 0.9);
      border: 1px solid rgba(0, 217, 255, 0.3);
      border-radius: 8px;
      padding: 1.5em;
      margin-bottom: 1.5em;
      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 1em;
    }
    th, td {
      padding: 0.75em;
      text-align: left;
      border-bottom: 1px solid rgba(0, 217, 255, 0.2);
    }
    th {
      background: rgba(0, 217, 255, 0.1);
      color: #00d9ff;
      font-weight: bold;
    }
    tr:hover { background: rgba(0, 217, 255, 0.05); }
    .status-item {
      display: grid;
      grid-template-columns: 1fr 2fr;
      gap: 1em;
      margin: 0.5em 0;
      padding: 0.5em;
      border-left: 3px solid #00d9ff;
      padding-left: 1em;
    }
    .label { color: #888; font-weight: bold; }
    .value { color: #00ff88; }
    .status-ok { color: #00ff88; }
    .status-warning { color: #ffaa00; }
    .status-error { color: #ff4444; }
    #refresh-timer {
      color: #888;
      font-size: 0.9em;
      margin-top: 0.5em;
    }
    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 1.5em; }
    @media (max-width: 768px) {
      .grid-2 { grid-template-columns: 1fr; }
      body { padding: 1em; }
      h1 { font-size: 1.5em; }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>⚔️ pvpgn server dashboard</h1>
    
    <div id="status-card" class="card">
      <h2>Server Status</h2>
      <div id="status-content">Loading...</div>
      <div id="refresh-timer"></div>
    </div>

    <div class="grid-2">
      <div class="card">
        <h2>Online Players</h2>
        <div id="players-content">Loading...</div>
      </div>
      <div class="card">
        <h2>Active Channels</h2>
        <div id="channels-content">Loading...</div>
      </div>
    </div>

    <div class="card">
      <h2>Active Games</h2>
      <div id="games-content">Loading...</div>
    </div>
  </div>

  <script>
    let lastRefresh = Date.now();
    let refreshInterval = 5000; // 5 seconds

    async function fetchJson(url) {
      try {
        const response = await fetch(url);
        if (!response.ok) return null;
        return await response.json();
      } catch (e) {
        console.error('Fetch error:', e);
        return null;
      }
    }

    function formatUptime(seconds) {
      const d = Math.floor(seconds / 86400);
      const h = Math.floor((seconds % 86400) / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = seconds % 60;
      if (d > 0) return `${d}d ${h}h ${m}m`;
      if (h > 0) return `${h}h ${m}m ${s}s`;
      return `${m}m ${s}s`;
    }

    function renderStatus(data) {
      if (!data) return '<p class="status-error">Error loading status</p>';
      return `
        <div class="status-item">
          <span class="label">Version:</span>
          <span class="value">${data.version || 'unknown'}</span>
        </div>
        <div class="status-item">
          <span class="label">Uptime:</span>
          <span class="value">${formatUptime(data.uptime_seconds || 0)}</span>
        </div>
        <div class="status-item">
          <span class="label">Active Connections:</span>
          <span class="value status-ok">${data.active_connections || 0}</span>
        </div>
      `;
    }

    function renderPlayers(data) {
      if (!data || data.length === 0) {
        return '<p style="color: #888;">No players online</p>';
      }
      let html = '<table><tr><th>Name</th><th>Protocol</th><th>Channel</th></tr>';
      for (const p of data) {
        html += `<tr><td>${p.name || '?'}</td><td>${p.protocol || '?'}</td><td>${p.channel || '?'}</td></tr>`;
      }
      html += '</table>';
      return html;
    }

    function renderChannels(data) {
      if (!data || data.length === 0) {
        return '<p style="color: #888;">No active channels</p>';
      }
      let html = '<table><tr><th>Channel</th><th>Members</th><th>Topic</th></tr>';
      for (const c of data) {
        html += `<tr><td>${c.name || '?'}</td><td>${c.members || 0}</td><td>${c.topic || ''}</td></tr>`;
      }
      html += '</table>';
      return html;
    }

    function renderGames(data) {
      if (!data || data.length === 0) {
        return '<p style="color: #888;">No active games</p>';
      }
      let html = '<table><tr><th>Name</th><th>Host</th><th>Players</th><th>Type</th></tr>';
      for (const g of data) {
        const ratio = `${g.players || 0}/${g.max_players || 0}`;
        html += `<tr><td>${g.name || '?'}</td><td>${g.host || '?'}</td><td>${ratio}</td><td>${g.type || '?'}</td></tr>`;
      }
      html += '</table>';
      return html;
    }

    async function refresh() {
      const [status, players, channels, games] = await Promise.all([
        fetchJson('/api/v1/status'),
        fetchJson('/api/v1/players'),
        fetchJson('/api/v1/channels'),
        fetchJson('/api/v1/games')
      ]);

      document.getElementById('status-content').innerHTML = renderStatus(status);
      document.getElementById('players-content').innerHTML = renderPlayers(players);
      document.getElementById('channels-content').innerHTML = renderChannels(channels);
      document.getElementById('games-content').innerHTML = renderGames(games);

      lastRefresh = Date.now();
      updateRefreshTimer();
    }

    function updateRefreshTimer() {
      const elapsed = Math.floor((Date.now() - lastRefresh) / 1000);
      const remaining = Math.floor(refreshInterval / 1000) - elapsed;
      document.getElementById('refresh-timer').textContent = 
        `Last refreshed: ${elapsed}s ago (next in ${Math.max(0, remaining)}s)`;
    }

    // Initial refresh
    refresh();

    // Auto-refresh every 5 seconds
    setInterval(refresh, refreshInterval);
    setInterval(updateRefreshTimer, 1000);
  </script>
</body>
</html>)html";

}  // namespace pvpgn::infra::webui

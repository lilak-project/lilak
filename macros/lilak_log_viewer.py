#!/usr/bin/env python3

import argparse
import json
import os
import re
import socket
import sys
import urllib.parse
import webbrowser
from datetime import datetime
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


TIMESTAMP_LOG_RE = re.compile(r"^(init|run|eor)_(\d{8}_\d{6})\.log$")
SAFE_LOG_NAME_RE = re.compile(r"^(init|run|eor)_\d{8}_\d{6}\.log$")
MAX_LOG_SIZE = 2_000_000


def lilak_root_path():
    configured = os.environ.get("LILAK_PATH", "")
    if configured:
        return Path(configured).expanduser().resolve()
    return Path(__file__).resolve().parent.parent


def parse_log(path):
    parsed = {}
    try:
        content = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return parsed

    for raw_line in content.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        parts = line.split(None, 1)
        key = parts[0]
        value = parts[1].strip() if len(parts) > 1 else ""
        parsed[key] = value
    return parsed


def parse_datetime(value, fallback_stamp=""):
    cleaned = re.sub(r"\s*\[[^]]*]\s*$", "", value or "").strip()
    if cleaned.isdigit():
        try:
            return datetime.fromtimestamp(int(cleaned))
        except (OSError, OverflowError, ValueError):
            pass

    for date_format in (
        "%Y.%m.%d %H:%M:%S",
        "%Y-%m-%d %H:%M:%S",
        "%Y/%m/%d %H:%M:%S",
        "%Y%m%d_%H%M%S",
    ):
        try:
            return datetime.strptime(cleaned, date_format)
        except ValueError:
            continue

    if fallback_stamp:
        try:
            return datetime.strptime(fallback_stamp, "%Y%m%d_%H%M%S")
        except ValueError:
            pass
    return None


def format_datetime(value, fallback_stamp=""):
    parsed = parse_datetime(value, fallback_stamp)
    return parsed.strftime("%Y-%m-%d %H:%M:%S") if parsed else ""


def parse_duration_seconds(value, start_value="", end_value=""):
    cleaned = re.sub(r"\s*\[[^]]*]\s*$", "", value or "").strip()
    try:
        return float(cleaned)
    except ValueError:
        start = parse_datetime(start_value)
        end = parse_datetime(end_value)
        if start is None or end is None:
            return None
        return (end - start).total_seconds()


def format_duration(value, start_value="", end_value=""):
    seconds = parse_duration_seconds(value, start_value, end_value)
    if seconds is None:
        return ""

    if seconds < 60:
        return f"{seconds:g} s"
    if seconds < 3600:
        minutes, remainder = divmod(int(seconds), 60)
        return f"{minutes}m {remainder:02d}s"
    hours, remainder = divmod(int(seconds), 3600)
    minutes, secs = divmod(remainder, 60)
    return f"{hours}h {minutes:02d}m {secs:02d}s"


def build_run_rows(log_path):
    grouped = {}
    if not log_path.is_dir():
        return []

    for path in log_path.iterdir():
        if path.is_symlink() or not path.is_file():
            continue
        match = TIMESTAMP_LOG_RE.match(path.name)
        if match is None:
            continue
        stage, stamp = match.groups()
        group = grouped.setdefault(stamp, {"stamp": stamp})
        if stage == "init":
            group["init_path"] = path
        else:
            existing = group.get("run_path")
            if existing is None or stage == "run":
                group["run_path"] = path

    rows = []
    for stamp, group in grouped.items():
        init_path = group.get("init_path")
        run_path = group.get("run_path")
        init_data = parse_log(init_path) if init_path else {}
        run_data = parse_log(run_path) if run_path else {}
        data = dict(init_data)
        data.update(run_data)

        start_raw = run_data.get("start_time", "") or init_data.get("start_time", "")
        end_raw = run_data.get("end_time", "")
        start_text = format_datetime(start_raw, stamp)
        end_text = format_datetime(end_raw)
        duration_raw = run_data.get("run_time", "")
        duration_seconds = parse_duration_seconds(duration_raw, start_raw, end_raw)
        duration = format_duration(duration_raw, start_raw, end_raw)

        rows.append(
            {
                "run_name": data.get("RunName", ""),
                "run_number": data.get("RunID", ""),
                "start_datetime": start_text,
                "end_datetime": end_text,
                "time_took": duration,
                "time_took_seconds": duration_seconds,
                "init_log": init_path.name if init_path else "",
                "init_log_path": str(init_path.resolve()) if init_path else "",
                "run_log": run_path.name if run_path else "",
                "run_log_path": str(run_path.resolve()) if run_path else "",
                "output_file": data.get("OutputFile", ""),
                "completed": run_path is not None,
                "sort_key": start_text or stamp,
            }
        )

    rows.sort(key=lambda row: row["sort_key"], reverse=True)
    for row in rows:
        row.pop("sort_key", None)
    return rows


HTML = r"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LILAK Run Logs</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #0b1017;
      --panel: #111925;
      --panel-2: #172231;
      --border: #263548;
      --text: #e8edf4;
      --muted: #8fa0b5;
      --cyan: #67d4e7;
      --green: #78d6a3;
      --yellow: #e8c56b;
      --glow: rgba(62, 127, 150, .22);
      --table-bg: rgba(17, 25, 37, .9);
      --header-bg: #162130;
      --filter-bg: #111a27;
      --header-text: #9fb0c4;
      --row-border: rgba(38, 53, 72, .64);
      --row-hover: rgba(52, 75, 99, .22);
      --strong: #ffffff;
      --output: #c8d4e2;
      --pre-text: #d7e1ec;
      --backdrop: rgba(2, 6, 11, .76);
      --error: #ff9f9f;
      --hover-border: #49617b;
      --shadow: 0 18px 55px rgba(0, 0, 0, .38);
    }
    :root[data-theme="bright"] {
      color-scheme: light;
      --bg: #f3f6fa;
      --panel: #ffffff;
      --panel-2: #edf2f7;
      --border: #c8d3df;
      --text: #172231;
      --muted: #65758a;
      --cyan: #087e96;
      --green: #247b50;
      --yellow: #9a7115;
      --glow: rgba(58, 143, 170, .13);
      --table-bg: rgba(255, 255, 255, .96);
      --header-bg: #e6edf5;
      --filter-bg: #f4f7fa;
      --header-text: #53657b;
      --row-border: rgba(190, 203, 216, .8);
      --row-hover: rgba(28, 116, 145, .08);
      --strong: #0e1927;
      --output: #34485f;
      --pre-text: #28394c;
      --backdrop: rgba(24, 36, 50, .42);
      --error: #b42318;
      --hover-border: #7c91a8;
      --shadow: 0 18px 55px rgba(42, 61, 80, .17);
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background:
        radial-gradient(circle at 10% -10%, var(--glow), transparent 35rem),
        var(--bg);
      color: var(--text);
      font: 14px/1.45 ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    }
    header {
      display: flex;
      align-items: flex-end;
      justify-content: space-between;
      gap: 24px;
      padding: 34px 38px 20px;
    }
    h1 {
      margin: 0;
      font: 700 29px/1.1 system-ui, sans-serif;
      letter-spacing: -.02em;
    }
    .eyebrow {
      margin-bottom: 7px;
      color: var(--cyan);
      font-size: 11px;
      font-weight: 700;
      letter-spacing: .16em;
      text-transform: uppercase;
    }
    .summary { margin-top: 8px; color: var(--muted); }
    .controls { display: flex; justify-content: flex-end; flex-wrap: wrap; gap: 9px; }
    input, select, button {
      border: 1px solid var(--border);
      border-radius: 8px;
      background: var(--panel);
      color: var(--text);
      font: inherit;
    }
    input { width: min(360px, 38vw); padding: 10px 12px; outline: none; }
    select { padding: 10px 30px 10px 11px; cursor: pointer; }
    input:focus { border-color: var(--cyan); box-shadow: 0 0 0 3px rgba(103, 212, 231, .1); }
    button { padding: 9px 12px; cursor: pointer; }
    button:hover { border-color: var(--hover-border); background: var(--panel-2); }
    main { padding: 0 38px 38px; }
    .table-shell {
      overflow: auto;
      height: calc(100vh - 150px);
      min-height: 360px;
      border: 1px solid var(--border);
      border-radius: 11px;
      background: var(--table-bg);
      box-shadow: var(--shadow);
    }
    table { width: 100%; min-width: 1420px; border-collapse: collapse; }
    table.brief-mode { min-width: 850px; }
    table.brief-mode .detail-column { display: none; }
    th {
      position: sticky;
      top: 0;
      z-index: 1;
      padding: 12px 14px;
      background: var(--header-bg);
      color: var(--header-text);
      border-bottom: 1px solid var(--border);
      text-align: left;
      font-size: 10px;
      letter-spacing: .1em;
      text-transform: uppercase;
      white-space: nowrap;
    }
    thead tr:first-child th { top: 0; z-index: 3; }
    .filter-row th {
      top: 41px;
      z-index: 2;
      padding: 7px 10px 9px;
      background: var(--filter-bg);
      font-size: 10px;
      font-weight: 400;
      letter-spacing: normal;
      text-transform: none;
    }
    .header-actions { display: flex; align-items: center; gap: 6px; }
    .sort-button {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 2px 0;
      border: 0;
      border-radius: 0;
      background: transparent;
      color: inherit;
      font-size: inherit;
      font-weight: 700;
      letter-spacing: inherit;
      text-transform: inherit;
    }
    .sort-button:hover { background: transparent; color: var(--text); }
    .sort-indicator { min-width: 1em; color: var(--cyan); font-size: 12px; }
    .filter-trigger {
      padding: 5px 8px;
      border: 1px solid var(--border);
      border-radius: 6px;
      background: var(--panel);
      color: var(--muted);
      cursor: pointer;
      font-size: 10px;
      letter-spacing: .04em;
      text-transform: none;
    }
    .filter-trigger:hover,
    .filter-trigger[aria-expanded="true"] { border-color: var(--cyan); color: var(--cyan); }
    .filter-menu {
      position: fixed;
      z-index: 1000;
      width: 240px;
      max-height: calc(100vh - 24px);
      padding: 10px;
      border: 1px solid var(--border);
      border-radius: 9px;
      background: var(--filter-bg);
      box-shadow: var(--shadow);
      color: var(--text);
      font-size: 12px;
      font-weight: 400;
      letter-spacing: normal;
      text-transform: none;
    }
    .filter-menu.hidden { display: none; }
    .filter-menu-head { display: flex; align-items: center; gap: 6px; padding-bottom: 8px; }
    .filter-menu-head strong { margin-right: auto; }
    .filter-menu-head button { padding: 3px 6px; font-size: 10px; }
    .filter-options { max-height: 260px; overflow-y: auto; border-top: 1px solid var(--border); }
    .filter-option {
      display: flex;
      align-items: center;
      gap: 8px;
      padding: 7px 4px;
      cursor: pointer;
    }
    .filter-option:hover { color: var(--cyan); }
    .filter-option input { width: auto; margin: 0; accent-color: var(--cyan); }
    .range-filter { display: flex; align-items: center; gap: 5px; }
    .range-filter input {
      width: 78px;
      min-width: 0;
      padding: 6px 7px;
      border-radius: 6px;
      font-size: 10px;
    }
    .range-separator { color: var(--muted); font-size: 9px; }
    td { padding: 11px 14px; border-bottom: 1px solid var(--row-border); white-space: nowrap; }
    tbody tr:hover { background: var(--row-hover); }
    tbody tr:last-child td { border-bottom: 0; }
    .run-name { color: var(--strong); font-weight: 700; }
    .muted, .empty { color: var(--muted); }
    .empty { opacity: .58; }
    .output-wrap { display: flex; align-items: center; gap: 8px; }
    .output { color: var(--output); }
    .copy-actions { display: inline-flex; gap: 4px; }
    .copy-button { padding: 4px 7px; color: var(--muted); font-size: 10px; }
    .copy-button:hover { color: var(--text); }
    .log-button {
      padding: 5px 8px;
      border-color: rgba(103, 212, 231, .3);
      background: rgba(103, 212, 231, .08);
      color: var(--cyan);
      font-size: 12px;
    }
    .log-button.run {
      border-color: rgba(120, 214, 163, .3);
      background: rgba(120, 214, 163, .08);
      color: var(--green);
    }
    dialog {
      width: min(1080px, calc(100vw - 42px));
      height: min(760px, calc(100vh - 42px));
      padding: 0;
      border: 1px solid var(--border);
      border-radius: 12px;
      background: var(--panel);
      color: var(--text);
      box-shadow: var(--shadow);
    }
    dialog::backdrop { background: var(--backdrop); backdrop-filter: blur(3px); }
    .dialog-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 13px 16px;
      border-bottom: 1px solid var(--border);
      background: var(--panel-2);
    }
    .dialog-head strong { color: var(--cyan); }
    .dialog-head button { padding: 5px 9px; }
    .dialog-actions { display: flex; gap: 6px; }
    pre {
      height: calc(100% - 54px);
      margin: 0;
      padding: 18px;
      overflow: auto;
      color: var(--pre-text);
      font: 12px/1.55 ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      white-space: pre;
    }
    .error { padding: 30px; color: var(--error); }
    @media (max-width: 760px) {
      header { align-items: stretch; flex-direction: column; padding: 24px 18px 16px; }
      .controls { width: 100%; }
      input { width: 100%; min-width: 0; }
      main { padding: 0 18px 24px; }
      .table-shell { height: calc(100vh - 190px); min-height: 320px; }
    }
  </style>
</head>
<body>
  <header>
    <div>
      <div class="eyebrow">LILAK / data/log</div>
      <h1>Run log viewer</h1>
      <div class="summary" id="summary">Loading run logs…</div>
    </div>
    <div class="controls">
      <select id="status-filter" aria-label="Filter by completion status">
        <option value="all">All statuses</option>
        <option value="complete">Complete</option>
        <option value="incomplete">Incomplete</option>
      </select>
      <input id="search" type="search" placeholder="Filter run, date, or output file" aria-label="Filter run logs">
      <button id="show-log-list" type="button">Log list</button>
      <button id="show-output-list" type="button">Output list</button>
      <button id="brief-toggle" type="button" aria-pressed="false">Brief mode</button>
      <button id="theme-toggle" type="button">Bright theme</button>
      <button id="refresh" type="button">Refresh</button>
    </div>
  </header>
  <main>
    <div class="table-shell">
      <table id="run-table">
        <thead>
          <tr>
            <th><button class="sort-button" type="button" data-sort="run_name">Run name <span class="sort-indicator"></span></button></th>
            <th><button class="sort-button" type="button" data-sort="run_number">Run number <span class="sort-indicator"></span></button></th>
            <th class="detail-column"><button class="sort-button" type="button" data-sort="start_datetime">Start date time <span class="sort-indicator"></span></button></th>
            <th class="detail-column"><button class="sort-button" type="button" data-sort="end_datetime">End date time <span class="sort-indicator"></span></button></th>
            <th class="detail-column">Time took</th>
            <th class="detail-column">Init log</th>
            <th class="detail-column">Run log</th>
            <th>Output file name</th>
          </tr>
          <tr class="filter-row">
            <th>
              <button class="filter-trigger" id="run-name-filter-label" type="button" aria-expanded="false">All run names</button>
            </th>
            <th>
              <div class="range-filter">
                <input id="run-number-min" type="number" placeholder="Min" aria-label="Minimum run number">
                <span class="range-separator">to</span>
                <input id="run-number-max" type="number" placeholder="Max" aria-label="Maximum run number">
              </div>
            </th>
            <th class="detail-column"></th>
            <th class="detail-column"></th>
            <th class="detail-column">
              <div class="range-filter">
                <input id="duration-min" type="number" min="0" step="any" placeholder="Min s" aria-label="Minimum duration in seconds">
                <span class="range-separator">to</span>
                <input id="duration-max" type="number" min="0" step="any" placeholder="Max s" aria-label="Maximum duration in seconds">
              </div>
            </th>
            <th class="detail-column"></th>
            <th class="detail-column"></th>
            <th></th>
          </tr>
        </thead>
        <tbody id="rows"></tbody>
      </table>
    </div>
  </main>
  <div class="filter-menu hidden" id="run-name-filter-menu">
    <div class="filter-menu-head">
      <strong>Run names</strong>
      <button type="button" data-filter-action="all">All</button>
      <button type="button" data-filter-action="none">None</button>
    </div>
    <div class="filter-options" id="run-name-options"></div>
  </div>
  <dialog id="viewer">
    <div class="dialog-head">
      <strong id="viewer-title"></strong>
      <div class="dialog-actions">
        <button id="copy-popup" type="button">Copy</button>
        <button id="close" type="button">Close</button>
      </div>
    </div>
    <pre id="content"></pre>
  </dialog>
  <script>
    const state = {
      runs: [],
      allRunNames: [],
      selectedRunNames: new Set(),
      runNameFilterActive: false,
      statusFilter: 'all',
      briefMode: false,
      sortKey: 'start_datetime',
      sortDirection: 'desc',
    };
    const rows = document.querySelector('#rows');
    const search = document.querySelector('#search');
    const summary = document.querySelector('#summary');
    const themeToggle = document.querySelector('#theme-toggle');
    const briefToggle = document.querySelector('#brief-toggle');
    const runTable = document.querySelector('#run-table');
    const viewer = document.querySelector('#viewer');
    const content = document.querySelector('#content');
    const viewerTitle = document.querySelector('#viewer-title');
    const runNameFilterButton = document.querySelector('#run-name-filter-label');
    const runNameFilterMenu = document.querySelector('#run-name-filter-menu');
    const rangeFilters = {
      runNumberMin: document.querySelector('#run-number-min'),
      runNumberMax: document.querySelector('#run-number-max'),
      durationMin: document.querySelector('#duration-min'),
      durationMax: document.querySelector('#duration-max'),
    };

    function applyTheme(theme, save = false) {
      const selected = theme === 'bright' ? 'bright' : 'dark';
      document.documentElement.dataset.theme = selected;
      themeToggle.textContent = selected === 'bright' ? 'Dark theme' : 'Bright theme';
      themeToggle.title = selected === 'bright' ? 'Switch to dark theme' : 'Switch to bright theme';
      themeToggle.setAttribute('aria-pressed', selected === 'bright' ? 'true' : 'false');
      if (save) {
        try { localStorage.setItem('lilak-log-theme', selected); } catch (error) {}
      }
    }

    function initialTheme() {
      try {
        const saved = localStorage.getItem('lilak-log-theme');
        if (saved === 'bright' || saved === 'dark') return saved;
      } catch (error) {}
      return window.matchMedia && window.matchMedia('(prefers-color-scheme: light)').matches ? 'bright' : 'dark';
    }

    applyTheme(initialTheme());

    function cell(value, className = '') {
      const td = document.createElement('td');
      td.textContent = value || '—';
      td.className = className;
      if (!value) td.classList.add('empty');
      return td;
    }

    function logCell(name, kind) {
      const td = document.createElement('td');
      td.className = 'detail-column';
      if (!name) {
        td.textContent = '—';
        td.classList.add('empty');
        return td;
      }
      const button = document.createElement('button');
      button.type = 'button';
      button.className = `log-button ${kind}`;
      button.textContent = name;
      button.addEventListener('click', () => openLog(name));
      td.appendChild(button);
      return td;
    }

    function fileName(path) {
      return path ? path.split('/').pop() : '';
    }

    async function copyText(value, button) {
      if (!value) return;
      try {
        if (navigator.clipboard && window.isSecureContext) {
          await navigator.clipboard.writeText(value);
        } else {
          const input = document.createElement('textarea');
          input.value = value;
          input.style.position = 'fixed';
          input.style.opacity = '0';
          document.body.appendChild(input);
          input.select();
          document.execCommand('copy');
          input.remove();
        }
        const original = button.textContent;
        button.textContent = 'Copied';
        window.setTimeout(() => { button.textContent = original; }, 900);
      } catch (error) {
        const original = button.textContent;
        button.textContent = 'Failed';
        window.setTimeout(() => { button.textContent = original; }, 1200);
      }
    }

    function outputCell(path) {
      const td = document.createElement('td');
      if (!path) {
        td.textContent = '—';
        td.className = 'empty';
        return td;
      }
      const wrap = document.createElement('div');
      wrap.className = 'output-wrap';
      const name = document.createElement('span');
      name.className = 'output';
      name.textContent = fileName(path);
      name.title = path;
      const actions = document.createElement('span');
      actions.className = 'copy-actions';
      const copyName = document.createElement('button');
      copyName.type = 'button';
      copyName.className = 'copy-button';
      copyName.textContent = 'Copy name';
      copyName.title = 'Copy file name';
      copyName.addEventListener('click', () => copyText(fileName(path), copyName));
      const copyPath = document.createElement('button');
      copyPath.type = 'button';
      copyPath.className = 'copy-button';
      copyPath.textContent = 'Copy path';
      copyPath.title = 'Copy full path';
      copyPath.addEventListener('click', () => copyText(path, copyPath));
      actions.append(copyName, copyPath);
      wrap.append(name, actions);
      td.appendChild(wrap);
      return td;
    }

    function updateSortIndicators() {
      document.querySelectorAll('[data-sort]').forEach(button => {
        const indicator = button.querySelector('.sort-indicator');
        const active = button.dataset.sort === state.sortKey;
        indicator.textContent = active ? (state.sortDirection === 'asc' ? '↑' : '↓') : '↕';
        button.setAttribute('aria-sort', active ? (state.sortDirection === 'asc' ? 'ascending' : 'descending') : 'none');
      });
    }

    function compareRuns(left, right) {
      const leftValue = left[state.sortKey];
      const rightValue = right[state.sortKey];
      const leftEmpty = leftValue === '' || leftValue === null || leftValue === undefined;
      const rightEmpty = rightValue === '' || rightValue === null || rightValue === undefined;
      if (leftEmpty || rightEmpty) {
        if (leftEmpty === rightEmpty) return 0;
        return leftEmpty ? 1 : -1;
      }

      let comparison;
      if (state.sortKey === 'run_number') {
        const leftNumber = Number(leftValue);
        const rightNumber = Number(rightValue);
        comparison = Number.isFinite(leftNumber) && Number.isFinite(rightNumber)
          ? leftNumber - rightNumber
          : String(leftValue).localeCompare(String(rightValue), undefined, { numeric: true });
      } else {
        comparison = String(leftValue).localeCompare(String(rightValue), undefined, { numeric: true, sensitivity: 'base' });
      }
      return state.sortDirection === 'asc' ? comparison : -comparison;
    }

    function updateRunNameFilterLabel() {
      const label = document.querySelector('#run-name-filter-label');
      label.textContent = state.runNameFilterActive
        ? `${state.selectedRunNames.size}/${state.allRunNames.length} selected`
        : 'All run names';
    }

    function buildRunNameOptions() {
      const options = document.querySelector('#run-name-options');
      options.replaceChildren();
      for (const runName of state.allRunNames) {
        const label = document.createElement('label');
        label.className = 'filter-option';
        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.checked = !state.runNameFilterActive || state.selectedRunNames.has(runName);
        checkbox.addEventListener('change', () => {
          if (!state.runNameFilterActive) {
            state.selectedRunNames = new Set(state.allRunNames);
            state.runNameFilterActive = true;
          }
          if (checkbox.checked) state.selectedRunNames.add(runName);
          else state.selectedRunNames.delete(runName);
          if (state.selectedRunNames.size === state.allRunNames.length) {
            state.runNameFilterActive = false;
          }
          buildRunNameOptions();
          updateRunNameFilterLabel();
          render();
        });
        const text = document.createElement('span');
        text.textContent = runName || '(unnamed)';
        label.append(checkbox, text);
        options.appendChild(label);
      }
      updateRunNameFilterLabel();
    }

    function numberInRange(value, minimumInput, maximumInput) {
      const hasMinimum = minimumInput.value !== '';
      const hasMaximum = maximumInput.value !== '';
      if (!hasMinimum && !hasMaximum) return true;
      if (value === '' || value === null || value === undefined) return false;
      const number = Number(value);
      if (!Number.isFinite(number)) return false;
      if (hasMinimum && number < Number(minimumInput.value)) return false;
      if (hasMaximum && number > Number(maximumInput.value)) return false;
      return true;
    }

    function passesColumnFilters(run) {
      return numberInRange(run.run_number, rangeFilters.runNumberMin, rangeFilters.runNumberMax)
        && numberInRange(run.time_took_seconds, rangeFilters.durationMin, rangeFilters.durationMax);
    }

    function filteredRuns() {
      const query = search.value.trim().toLowerCase();
      return state.runs
        .filter(run => !state.runNameFilterActive || state.selectedRunNames.has(run.run_name))
        .filter(run => state.statusFilter === 'all' || (state.statusFilter === 'complete' ? run.completed : !run.completed))
        .filter(passesColumnFilters)
        .filter(run => Object.values(run).join(' ').toLowerCase().includes(query))
        .sort(compareRuns);
    }

    function closeRunNameFilter() {
      runNameFilterMenu.classList.add('hidden');
      runNameFilterButton.setAttribute('aria-expanded', 'false');
    }

    function openRunNameFilter() {
      runNameFilterMenu.classList.remove('hidden');
      runNameFilterButton.setAttribute('aria-expanded', 'true');
      const buttonRect = runNameFilterButton.getBoundingClientRect();
      const menuRect = runNameFilterMenu.getBoundingClientRect();
      let left = buttonRect.left;
      let top = buttonRect.bottom + 6;
      if (left + menuRect.width > window.innerWidth - 12) {
        left = Math.max(12, window.innerWidth - menuRect.width - 12);
      }
      if (top + menuRect.height > window.innerHeight - 12) {
        top = Math.max(12, buttonRect.top - menuRect.height - 6);
      }
      runNameFilterMenu.style.left = `${left}px`;
      runNameFilterMenu.style.top = `${top}px`;
    }

    function render() {
      const visible = filteredRuns();
      rows.replaceChildren();
      for (const run of visible) {
        const tr = document.createElement('tr');
        tr.appendChild(cell(run.run_name, 'run-name'));
        tr.appendChild(cell(run.run_number));
        tr.appendChild(cell(run.start_datetime, 'detail-column'));
        tr.appendChild(cell(run.end_datetime, 'muted detail-column'));
        tr.appendChild(cell(run.time_took, 'detail-column'));
        tr.appendChild(logCell(run.init_log, 'init'));
        tr.appendChild(logCell(run.run_log, 'run'));
        tr.appendChild(outputCell(run.output_file));
        rows.appendChild(tr);
      }
      const completed = state.runs.filter(run => run.completed).length;
      summary.textContent = `${visible.length} shown / ${state.runs.length} runs · ${completed} completed`;
    }

    function openTextPopup(title, text) {
      viewerTitle.textContent = title;
      content.textContent = text;
      if (!viewer.open) viewer.showModal();
    }

    function showLogList() {
      const paths = [];
      for (const run of filteredRuns()) {
        if (run.init_log_path) paths.push(run.init_log_path);
        if (run.run_log_path) paths.push(run.run_log_path);
      }
      openTextPopup(`Log files (${paths.length})`, paths.join('\n') || '(no matching log files)');
    }

    function showOutputList() {
      const paths = filteredRuns().map(run => run.output_file).filter(Boolean);
      openTextPopup(`Output files (${paths.length})`, paths.join('\n') || '(no matching output files)');
    }

    function applyBriefMode() {
      runTable.classList.toggle('brief-mode', state.briefMode);
      briefToggle.textContent = state.briefMode ? 'Full mode' : 'Brief mode';
      briefToggle.setAttribute('aria-pressed', state.briefMode ? 'true' : 'false');
    }

    async function loadRuns() {
      summary.textContent = 'Loading run logs…';
      try {
        const response = await fetch('/api/runs', { cache: 'no-store' });
        if (!response.ok) throw new Error(await response.text());
        state.runs = (await response.json()).runs;
        state.allRunNames = [...new Set(state.runs.map(run => run.run_name))]
          .sort((left, right) => left.localeCompare(right, undefined, { numeric: true, sensitivity: 'base' }));
        if (state.runNameFilterActive) {
          state.selectedRunNames = new Set(
            [...state.selectedRunNames].filter(runName => state.allRunNames.includes(runName))
          );
        } else {
          state.selectedRunNames = new Set(state.allRunNames);
        }
        buildRunNameOptions();
        updateSortIndicators();
        render();
      } catch (error) {
        summary.textContent = 'Could not load run logs';
        rows.innerHTML = `<tr><td class="error" colspan="8"></td></tr>`;
        rows.querySelector('.error').textContent = error.message;
      }
    }

    async function openLog(name) {
      openTextPopup(name, 'Loading…');
      try {
        const response = await fetch('/api/log?name=' + encodeURIComponent(name), { cache: 'no-store' });
        const payload = await response.json();
        if (!response.ok) throw new Error(payload.error || 'Could not read log');
        content.textContent = payload.content;
      } catch (error) {
        content.textContent = error.message;
      }
    }

    search.addEventListener('input', render);
    document.querySelector('#show-log-list').addEventListener('click', showLogList);
    document.querySelector('#show-output-list').addEventListener('click', showOutputList);
    briefToggle.addEventListener('click', () => {
      state.briefMode = !state.briefMode;
      applyBriefMode();
    });
    themeToggle.addEventListener('click', () => {
      const next = document.documentElement.dataset.theme === 'bright' ? 'dark' : 'bright';
      applyTheme(next, true);
    });
    Object.values(rangeFilters).forEach(input => input.addEventListener('input', render));
    document.querySelector('#status-filter').addEventListener('change', event => {
      state.statusFilter = event.target.value;
      render();
    });
    document.querySelector('#refresh').addEventListener('click', loadRuns);
    document.querySelectorAll('[data-sort]').forEach(button => {
      button.addEventListener('click', () => {
        const key = button.dataset.sort;
        if (state.sortKey === key) state.sortDirection = state.sortDirection === 'asc' ? 'desc' : 'asc';
        else {
          state.sortKey = key;
          state.sortDirection = 'asc';
        }
        updateSortIndicators();
        render();
      });
    });
    document.querySelectorAll('[data-filter-action]').forEach(button => {
      button.addEventListener('click', () => {
        if (button.dataset.filterAction === 'all') {
          state.runNameFilterActive = false;
          state.selectedRunNames = new Set(state.allRunNames);
        } else {
          state.runNameFilterActive = true;
          state.selectedRunNames.clear();
        }
        buildRunNameOptions();
        render();
      });
    });
    runNameFilterButton.addEventListener('click', event => {
      event.stopPropagation();
      if (runNameFilterMenu.classList.contains('hidden')) openRunNameFilter();
      else closeRunNameFilter();
    });
    document.addEventListener('click', event => {
      if (!runNameFilterMenu.classList.contains('hidden') && !runNameFilterMenu.contains(event.target)) {
        closeRunNameFilter();
      }
    });
    window.addEventListener('resize', closeRunNameFilter);
    document.querySelector('.table-shell').addEventListener('scroll', closeRunNameFilter);
    document.querySelector('#copy-popup').addEventListener('click', event => copyText(content.textContent, event.currentTarget));
    document.querySelector('#close').addEventListener('click', () => viewer.close());
    viewer.addEventListener('click', event => {
      if (event.target === viewer) viewer.close();
    });
    applyBriefMode();
    loadRuns();
  </script>
</body>
</html>
"""


class LogViewerHandler(BaseHTTPRequestHandler):
    server_version = "LILAKLogViewer/1.0"

    def log_message(self, message_format, *args):
        sys.stderr.write(f"[{self.log_date_time_string()}] {message_format % args}\n")

    def send_bytes(self, content, content_type, status=HTTPStatus.OK):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(content)

    def send_json(self, payload, status=HTTPStatus.OK):
        content = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_bytes(content, "application/json; charset=utf-8", status)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/":
            self.send_bytes(HTML.encode("utf-8"), "text/html; charset=utf-8")
            return

        if parsed.path == "/api/runs":
            self.send_json({"runs": build_run_rows(self.server.log_path)})
            return

        if parsed.path == "/api/log":
            query = urllib.parse.parse_qs(parsed.query)
            name = query.get("name", [""])[0]
            if not SAFE_LOG_NAME_RE.fullmatch(name):
                self.send_json({"error": "Invalid log name"}, HTTPStatus.BAD_REQUEST)
                return
            path = self.server.log_path / name
            if path.is_symlink() or not path.is_file():
                self.send_json({"error": f"Log not found: {name}"}, HTTPStatus.NOT_FOUND)
                return
            try:
                size = path.stat().st_size
                with path.open("rb") as stream:
                    if size > MAX_LOG_SIZE:
                        stream.seek(size - MAX_LOG_SIZE)
                    text = stream.read().decode("utf-8", "replace")
                if size > MAX_LOG_SIZE:
                    text = "[Showing the last 2 MB]\n\n" + text
                self.send_json({"name": name, "content": text})
            except OSError as error:
                self.send_json({"error": str(error)}, HTTPStatus.INTERNAL_SERVER_ERROR)
            return

        self.send_json({"error": "Not found"}, HTTPStatus.NOT_FOUND)


class LogViewerServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, log_path):
        super().__init__(address, LogViewerHandler)
        self.log_path = log_path


def pick_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def main():
    parser = argparse.ArgumentParser(description="Open the LILAK run log viewer.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()

    log_path = lilak_root_path() / "data" / "log"
    if not log_path.is_dir():
        print(f"Log directory not found: {log_path}", file=sys.stderr)
        return 1

    port = args.port or pick_port()
    server = LogViewerServer((args.host, port), log_path)
    url = f"http://{args.host}:{port}/"
    print(f"LILAK log viewer: {url}")
    print(f"Log directory: {log_path}")
    print("Press Ctrl-C to stop the server.")

    if not args.no_browser:
        webbrowser.open(url)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping log viewer.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

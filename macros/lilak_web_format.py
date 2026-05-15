#!/usr/bin/env python3

import html
import re


SHARED_CSS = r"""
    :root {
      --bg: #dfeaf7;
      --panel: #f7fbff;
      --line: #b6cbe0;
      --text: #2b2f36;
      --muted: #666f7b;
      --accent: #2e6f95;
      --accent-2: #5fa8d3;
      --ok: #3b82a6;
      --danger: #b24a5a;
      --shadow: 0 14px 30px rgba(20, 54, 84, 0.12);
      --radius: 16px;
      --mono: "SFMono-Regular", Menlo, Consolas, monospace;
      --sans: "Avenir Next", "Helvetica Neue", Helvetica, sans-serif;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: var(--sans);
      color: var(--text);
      background: var(--bg);
      min-height: 100vh;
    }
    button, select, input, textarea { font: inherit; }
    .app {
      display: grid;
      grid-template-rows: auto auto 1fr;
      min-height: 100vh;
    }
    .topbar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      padding: 18px 22px 10px;
    }
    .headline h1 {
      margin: 0;
      font-size: 26px;
      letter-spacing: 0.02em;
    }
    .headline p {
      margin: 4px 0 0;
      color: var(--muted);
      font-size: 14px;
    }
    .toolbar, .subtoolbar {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
    }
    .subtoolbar.block {
      display: grid;
      gap: 8px;
      align-items: start;
    }
    .subtoolbar-title {
      font-size: 12px;
      color: #2b2f36;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      margin-right: 4px;
      font-weight: 700;
    }
    .toolbar button, .subtoolbar button {
      border: 0;
      border-radius: 999px;
      padding: 3px 8px;
      box-shadow: var(--shadow);
      background: var(--panel);
      color: var(--text);
      cursor: pointer;
      transition: transform 0.15s ease, background 0.2s ease;
      font-size: 11px;
    }
    .toolbar button.primary, .subtoolbar button.primary {
      background: var(--accent);
      color: white;
    }
    .toolbar button:hover, .subtoolbar button:hover { transform: translateY(-1px); }
    .tabs-wrap {
      padding: 0 22px 16px;
      display: grid;
      gap: 10px;
    }
    .tabs {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
    }
    .tabs > button:not(.tab), .button-group > button {
      border: 1px solid var(--line);
      background: rgba(247,251,255,0.98);
      border-radius: 8px;
      padding: 3px 10px;
      min-height: 26px;
      color: var(--text);
      cursor: pointer;
      box-shadow: var(--shadow);
      font-size: 11px;
      transition: transform 0.15s ease, background 0.2s ease, border-color 0.2s ease;
    }
    .tabs > button:not(.tab):hover, .button-group > button:hover { transform: translateY(-1px); }
    .toolbar button.active-toggle, .tabs > button.active-toggle, .button-group > button.active-toggle {
      border-width: 3px;
      border-color: #8b0000;
      box-shadow: inset 0 0 0 1px rgba(139,0,0,0.22), 0 0 0 1px rgba(139,0,0,0.28), var(--shadow);
    }
    .tab {
      border: 1px solid var(--line);
      background: #edf5fc;
      border-radius: 999px;
      padding: 4px 10px;
      cursor: pointer;
      color: #495662;
      font-size: 12px;
    }
    .tab.active {
      background: #edf5fc;
      color: #2b2f36;
      border-color: #2b2f36;
      border-width: 2px;
      font-weight: 600;
    }
    .tab.error {
      border-color: #b10018;
      border-width: 3px;
      box-shadow: inset 0 0 0 1px rgba(177,0,24,0.18);
    }
    .tab-wrap {
      display: inline-flex;
      align-items: center;
      gap: 4px;
    }
    .tab-close {
      width: 22px;
      height: 22px;
      border: 1px solid rgba(33,66,90,0.24);
      border-radius: 999px;
      background: rgba(247,251,255,0.98);
      color: var(--muted);
      cursor: pointer;
      font-size: 14px;
      line-height: 1;
      padding: 0;
      display: inline-flex;
      align-items: center;
      justify-content: center;
    }
    .tab-close:hover { color: #7f2030; border-color: #7f2030; }
    .toolbar-separator {
      width: 1px;
      height: 18px;
      background: rgba(33,66,90,0.18);
      margin: 0 2px;
      display: inline-block;
    }
    .workspace {
      display: grid;
      grid-template-columns: 300px 1fr;
      gap: 18px;
      padding: 0 22px 22px;
      min-height: 0;
      height: 1520px;
      align-items: stretch;
    }
    .panel {
      background: rgba(247,251,255,0.94);
      border: 1px solid var(--line);
      border-radius: var(--radius);
      box-shadow: var(--shadow);
      overflow: hidden;
      display: flex;
      flex-direction: column;
      min-height: 0;
    }
    .panel h2 {
      margin: 0;
      font-size: 15px;
      text-transform: uppercase;
      letter-spacing: 0.08em;
    }
    .panel-header {
      padding: 14px 16px;
      border-bottom: 1px solid rgba(182,203,224,0.85);
      background: rgba(231,242,252,0.9);
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      flex-wrap: wrap;
    }
    .panel-body {
      padding: 14px 16px;
      overflow: auto;
      flex: 1 1 auto;
      min-height: 0;
    }
    .summary-panel .panel-body, .canvas-panel .panel-body { overflow: hidden; }
    .inspector-panel {
      grid-column: 1 / -1;
      max-height: 360px;
      min-height: 260px;
    }
    .inspector-panel .panel-body { overflow: auto; }
    .stack { display: grid; gap: 12px; }
    .summary-stack {
      display: flex;
      flex-direction: column;
      gap: 12px;
      height: 100%;
      min-height: 0;
    }
    .group-summary-scroll {
      flex: 1 1 auto;
      min-height: 0;
      overflow: auto;
      padding-right: 2px;
    }
    .group-card, .summary-card {
      border: 1px solid rgba(182,203,224,0.85);
      border-radius: 14px;
      padding: 12px;
      background: rgba(255,255,255,0.78);
      min-width: 0;
      width: 100%;
      overflow: hidden;
    }
    .summary-list {
      display: grid;
      gap: 6px;
      margin-top: 10px;
    }
    .summary-item {
      border: 1px solid rgba(182,203,224,0.85);
      border-radius: 10px;
      padding: 8px 10px;
      background: rgba(255,255,255,0.9);
      cursor: pointer;
      font-size: 12px;
      min-width: 0;
      width: 100%;
      overflow: hidden;
    }
    .summary-item.active {
      border-color: var(--accent);
      box-shadow: inset 0 0 0 1px rgba(46,111,149,0.18);
    }
    .summary-item-row {
      display: flex;
      align-items: start;
      justify-content: space-between;
      gap: 8px;
    }
    .summary-item-row > div {
      min-width: 0;
      overflow-wrap: anywhere;
      word-break: break-word;
    }
    .summary-delete {
      border: 0;
      background: transparent;
      color: #7f2030;
      cursor: pointer;
      font-size: 12px;
      line-height: 1;
      padding: 0;
    }
    .message-log {
      font-family: var(--mono);
      font-size: 12px;
      white-space: pre-wrap;
      overflow-wrap: anywhere;
      word-break: break-word;
      background: rgba(255,255,255,0.98);
      color: var(--text);
      padding: 12px;
      border-radius: 12px;
      border: 1px solid rgba(182,203,224,0.85);
      min-height: 96px;
    }
    .message-log.error {
      background: #fff0f1;
      color: #8f1d2c;
      border-color: #d58b95;
    }
    .canvas-shell {
      position: relative;
      border-radius: 18px;
      background:
        linear-gradient(0deg, rgba(247,251,255,0.9), rgba(247,251,255,0.9)),
        repeating-linear-gradient(0deg, rgba(99,140,173,0.09) 0px, rgba(99,140,173,0.09) 1px, transparent 1px, transparent 32px),
        repeating-linear-gradient(90deg, rgba(99,140,173,0.09) 0px, rgba(99,140,173,0.09) 1px, transparent 1px, transparent 32px);
      min-height: 1520px;
      overflow: hidden;
    }
    .canvas-shell svg {
      position: absolute;
      inset: 0;
      width: 100%;
      height: 100%;
      pointer-events: none;
    }
    .wire-layer-back { z-index: 1; }
    .wire-layer-front { z-index: 4; }
    .canvas-nodes {
      position: relative;
      min-height: 1520px;
      z-index: 2;
    }
    .node {
      position: absolute;
      width: 190px;
      border-radius: 16px;
      border: 1px solid rgba(71,112,145,0.16);
      background: rgba(255,255,255,0.98);
      box-shadow: 0 12px 24px rgba(37,72,103,0.12);
      overflow: hidden;
      user-select: none;
    }
    .node.selected { outline: 3px solid rgba(46,111,149,0.32); }
    .node.error { outline: 3px solid rgba(177,0,24,0.54); border-color: #b10018; }
    .node-header {
      padding: 10px 12px;
      font-weight: 700;
      cursor: move;
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 8px;
      color: white;
    }
    .node-title {
      flex: 1 1 auto;
      min-width: 0;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .node-settings {
      width: 24px;
      height: 24px;
      border: 1px solid rgba(33,66,90,0.26);
      border-radius: 999px;
      background: rgba(255,255,255,0.72);
      color: #1f2f3f;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      padding: 0;
      line-height: 1;
      flex: 0 0 24px;
      min-width: 24px;
      max-width: 24px;
      box-sizing: border-box;
      overflow: hidden;
      font-size: 15px;
      font-weight: 700;
    }
    .node-settings:hover {
      background: rgba(255,255,255,0.96);
      border-color: rgba(33,66,90,0.48);
    }
    .node-body { padding: 10px 12px 12px; font-size: 13px; }
    .node-inline-tools {
      display: grid;
      grid-template-columns: 1fr;
      gap: 2px;
      margin-top: 2px;
    }
    .node-inline-tools.single { grid-template-columns: 1fr; }
    .node-inline-tools.single input, .node-inline-tools input { width: 52%; min-width: 60px; }
    .node-inline-tools select, .node-inline-tools input, .node-inline-tools button {
      min-height: 20px;
      border-radius: 8px;
      border: 1px solid rgba(122,106,78,0.28);
      background: rgba(255,255,255,0.9);
      padding: 1px 5px;
      font-size: 11px;
    }
    .node-input-row {
      display: grid;
      grid-template-columns: auto 1fr;
      gap: 3px;
      align-items: center;
      margin-top: 4px;
      font-size: 12px;
      color: var(--muted);
    }
    .port-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin: 8px 0 0;
      gap: 10px;
      font-size: 12px;
    }
    .port-label { color: var(--muted); }
    .port {
      width: 14px;
      height: 14px;
      border-radius: 50%;
      border: 2px solid white;
      box-shadow: 0 0 0 1px rgba(47,38,25,0.28);
      cursor: crosshair;
      flex: 0 0 auto;
      background: #f4d35e;
    }
    .port.triangle-right, .port.triangle-left {
      width: 16px;
      height: 14px;
      border-radius: 0;
      border: 0;
      box-shadow: none;
      background: transparent;
    }
    .port.triangle-right {
      clip-path: polygon(0 0, 100% 50%, 0 100%);
      background: #b8b8b8;
    }
    .port.triangle-left {
      clip-path: polygon(100% 0, 0 50%, 100% 100%);
      background: #f4d35e;
    }
    .port.output { background: #b8b8b8; }
    .port.connected.input { background: var(--accent-2); }
    .port.connected.output { background: var(--accent); }
    .port.connected.triangle-right { background: var(--accent); }
    .port.connected.triangle-left { background: var(--accent-2); }
    .port.disabled { background: #111 !important; cursor: not-allowed; }
    .port.active-connect {
      box-shadow: 0 0 0 4px rgba(242,143,59,0.28);
      transform: scale(1.1);
    }
    .port-cell {
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .port-disable {
      width: 11px;
      height: 11px;
      border: 1px solid #111;
      background: transparent;
      cursor: pointer;
      user-select: none;
      line-height: 1;
      border-radius: 2px;
      flex: 0 0 auto;
    }
    .port-disable.disabled { background: #111; }
    .port-disable.enabled { background: transparent; }
    .port-disable:hover { box-shadow: 0 0 0 1px rgba(17,17,17,0.12); }
    .port-disable-label { font-size: 0; color: transparent; cursor: pointer; }
    .form-grid { display: grid; gap: 10px; }
    .field { display: grid; gap: 5px; }
    .field label {
      font-size: 12px;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: 0.04em;
    }
    .field input, .field select, .field textarea {
      width: 100%;
      border-radius: 10px;
      border: 1px solid rgba(122,106,78,0.28);
      background: rgba(255,255,255,0.88);
      padding: 9px 10px;
      min-height: 38px;
    }
    .form-grid.two { grid-template-columns: 1fr 1fr; }
    .form-grid.four { grid-template-columns: repeat(4, 1fr); }
    .status {
      font-family: var(--mono);
      font-size: 12px;
      white-space: pre-wrap;
      background: rgba(47,38,25,0.92);
      color: #fdf7ee;
      padding: 12px;
      border-radius: 14px;
      min-height: 180px;
    }
    .pill {
      display: inline-block;
      border-radius: 999px;
      padding: 3px 8px;
      font-size: 11px;
      background: rgba(79,119,45,0.12);
      color: var(--ok);
      margin-left: 6px;
    }
    .table {
      width: 100%;
      border-collapse: collapse;
      font-size: 12px;
    }
    .table th, .table td {
      border-bottom: 1px solid rgba(182,203,224,0.85);
      padding: 8px 6px;
      text-align: left;
    }
    .table input {
      width: 100%;
      min-width: 60px;
      border: 1px solid rgba(182,203,224,0.92);
      border-radius: 8px;
      padding: 6px 7px;
      background: rgba(255,255,255,0.92);
    }
    .parameter-sheet-wrap {
      overflow-x: auto;
      overflow-y: auto;
      max-height: min(62vh, 680px);
      background: #edf3f8;
      border: 1px solid rgba(182,203,224,0.92);
      border-radius: 14px;
    }
    .parameter-sheet {
      width: 100%;
      border-collapse: collapse;
      min-width: 920px;
      table-layout: fixed;
      font-size: 14px;
    }
    .parameter-sheet thead th {
      position: sticky;
      top: 0;
      z-index: 5;
      background: #4f7faa;
      border-bottom: 1px solid #3d678c;
      color: #ffffff;
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.1em;
      padding: 4px 10px;
    }
    .parameter-sheet th,
    .parameter-sheet td {
      padding: 4px 10px;
      border-bottom: 1px solid rgba(120, 99, 67, 0.08);
      vertical-align: middle;
    }
    .parameter-sheet th:nth-child(1), .parameter-sheet td:nth-child(1) { width: 54px; text-align: right; }
    .parameter-sheet th:nth-child(2), .parameter-sheet td:nth-child(2) { width: 190px; }
    .parameter-sheet th:nth-child(3), .parameter-sheet td:nth-child(3) { width: 190px; }
    .parameter-sheet th:nth-child(4), .parameter-sheet td:nth-child(4) { width: 260px; }
    .parameter-sheet th:nth-child(5), .parameter-sheet td:nth-child(5) { width: 90px; }
    .parameter-sheet td.readonly {
      color: #315a80;
      overflow-wrap: anywhere;
    }
    .parameter-sheet input[type="text"] {
      width: 100%;
      border: 1px solid transparent;
      background: rgba(255,255,255,0.96);
      border-radius: 10px;
      padding: 5px 10px;
      font: inherit;
      color: var(--text);
    }
    .parameter-sheet input[type="text"]:focus {
      outline: none;
      border-color: #4f7faa;
      box-shadow: 0 0 0 3px rgba(79,127,170,0.16);
    }
    .parameter-sheet .comment-row td {
      background: #e1edf8;
      color: #315a80;
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.12em;
      text-align: left;
    }
    .parameter-sheet .group-header td {
      background: #e1edf8;
      color: #315a80;
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.12em;
      text-align: left;
      font-weight: 700;
    }
    .parameter-sheet .commented-parameter-row td,
    .parameter-sheet .commented-parameter-row input[type="text"] {
      background: #eceff1;
      color: #7a7f85;
    }
    .parameter-sheet input[disabled] {
      cursor: not-allowed;
      opacity: 0.72;
    }
    .shortcut-sheet {
      width: min(620px, 100%);
      min-width: 560px;
      border-collapse: collapse;
      font-size: 13px;
    }
    .shortcut-sheet th,
    .shortcut-sheet td {
      padding: 7px 9px;
      border-bottom: 1px solid rgba(182,203,224,0.75);
      text-align: left;
      vertical-align: top;
    }
    .shortcut-sheet th:first-child,
    .shortcut-sheet td:first-child {
      width: 140px;
      white-space: nowrap;
      font-family: var(--mono);
    }
    .modal {
      position: fixed;
      inset: 0;
      background: rgba(22,17,10,0.42);
      display: none;
      align-items: center;
      justify-content: center;
      padding: 24px;
      z-index: 100;
    }
    .modal.open { display: flex; }
    #navigatorModal.open {
      align-items: flex-start;
      justify-content: center;
      padding-top: 18px;
    }
    .modal-card {
      width: min(1100px, 100%);
      max-height: calc(100vh - 48px);
      overflow: auto;
      background: #eef5ff;
      border: 1px solid rgba(182,203,224,0.92);
      border-radius: 18px;
      box-shadow: 0 24px 50px rgba(20,54,84,0.2);
      padding: 18px;
    }
    .modal-card h3 { margin: 0; }
    .modal-card > div:first-child {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      margin-bottom: 12px;
    }
    .modal-card button {
      border: 1px solid rgba(33,66,90,0.24);
      background: rgba(247,251,255,0.98);
      color: var(--text);
      border-radius: 8px;
      padding: 4px 10px;
      cursor: pointer;
      box-shadow: var(--shadow);
      font-size: 11px;
    }
    .navigator-card {
      width: min(980px, 100%);
      max-height: calc(100vh - 36px);
      background: #eef5ff;
      padding: 16px 18px 18px;
    }
    .navigator-controls { display: grid; gap: 10px; }
    .navigator-row { display: grid; gap: 8px; align-items: center; }
    .navigator-row.path-row { grid-template-columns: fit-content(88px) fit-content(88px) fit-content(68px) minmax(0, 1fr) fit-content(68px); }
    .navigator-row.file-row { grid-template-columns: fit-content(48px) minmax(0, 1fr) fit-content(72px); }
    .navigator-row input {
      border: 1px solid rgba(182,203,224,0.92);
      border-radius: 10px;
      padding: 8px 10px;
      background: rgba(255,255,255,0.96);
      min-width: 0;
      font-size: 13px;
    }
    .navigator-row button {
      border: 1px solid rgba(33,66,90,0.24);
      background: rgba(247,251,255,0.98);
      color: var(--text);
      border-radius: 8px;
      padding: 5px 10px;
      min-height: 28px;
      box-shadow: var(--shadow);
      font-size: 11px;
      cursor: pointer;
      justify-self: start;
      width: auto;
      white-space: nowrap;
    }
    .navigator-list {
      margin-top: 12px;
      border: 1px solid rgba(182,203,224,0.92);
      border-radius: 14px;
      background: rgba(255,255,255,0.96);
      padding: 8px;
      min-height: 340px;
      max-height: calc(100vh - 220px);
      overflow: auto;
    }
    .navigator-entry {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      padding: 8px 10px;
      border-radius: 10px;
      cursor: pointer;
      font-size: 18px;
    }
    .navigator-entry:hover { background: rgba(231,242,252,0.9); }
    .navigator-entry .name {
      flex: 1 1 auto;
      min-width: 0;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .navigator-entry .detail {
      display: block;
      margin-top: 2px;
      font-size: 12px;
      line-height: 1.25;
      color: var(--muted);
      white-space: normal;
      overflow-wrap: anywhere;
    }
    .navigator-entry.dir-entry .name { color: #2f6fa6; font-weight: 600; }
    .navigator-entry.conf-entry .name { color: #7b4ca6; font-weight: 600; }
    .navigator-entry.file-entry .name { color: #808898; }
    .workspace.summary-collapsed {
      grid-template-columns: 52px 1fr;
    }
    .summary-toggle {
      width: 28px;
      height: 28px;
      border: 1px solid rgba(33,66,90,0.24);
      border-radius: 8px;
      background: rgba(247,251,255,0.98);
      color: var(--text);
      cursor: pointer;
      font-size: 16px;
      line-height: 1;
      padding: 0;
      display: inline-flex;
      align-items: center;
      justify-content: center;
    }
    .workspace.summary-collapsed .summary-panel .panel-header {
      justify-content: center;
      padding-left: 8px;
      padding-right: 8px;
    }
    .workspace.summary-collapsed .summary-panel h2,
    .workspace.summary-collapsed .summary-panel .panel-body {
      display: none;
    }
    @media (max-width: 1180px) {
      .workspace { grid-template-columns: 280px 1fr; }
      .workspace > .panel:last-child { grid-column: 1 / -1; }
    }
    @media (max-width: 900px) {
      .workspace { grid-template-columns: 1fr; height: auto; }
      .workspace > .panel:nth-child(1) { order: 2; }
      .workspace > .panel:nth-child(2) { order: 1; }
      .workspace > .panel:nth-child(3) { order: 3; }
      .summary-panel .panel-body, .canvas-panel .panel-body, .inspector-panel .panel-body { overflow: auto; }
      .canvas-shell, .canvas-nodes { min-height: 900px; }
    }
"""


SHARED_JS = r"""
window.lilakWebFormat = window.lilakWebFormat || {};
window.lilakWebFormat.toggleModal = function(modalId, forceOpen) {
  const modal = document.getElementById(modalId);
  if (!modal) return false;
  if (forceOpen === true) modal.classList.add("open");
  else if (forceOpen === false) modal.classList.remove("open");
  else modal.classList.toggle("open");
  return modal.classList.contains("open");
};
window.lilakWebFormat.toggleShortcutsModal = function(modalId) {
  return window.lilakWebFormat.toggleModal(modalId || "shortcutsModal");
};
window.lilakWebFormat.closeShortcutsModal = function(modalId) {
  return window.lilakWebFormat.toggleModal(modalId || "shortcutsModal", false);
};
window.lilakWebFormat.bindShortcutsButton = function(buttonId, modalId) {
  const button = document.getElementById(buttonId || "shortcutsBtn");
  if (!button) return;
  button.addEventListener("click", () => window.lilakWebFormat.toggleShortcutsModal(modalId || "shortcutsModal"));
};
window.lilakWebFormat.nodeEditButton = function(nodeId, escapeHtml, options) {
  options = options || {};
  const esc = escapeHtml || (value => String(value ?? ""));
  const action = options.action || "open-edit";
  const title = options.title || "Edit";
  const label = options.label || "✎";
  return `<button class="node-settings" data-action="${esc(action)}" data-node-id="${esc(nodeId)}" title="${esc(title)}" aria-label="${esc(title)}">${esc(label)}</button>`;
};
window.lilakWebFormat.isParameterFile = function(name) {
  const lower = String(name || "").toLowerCase();
  return lower.endsWith(".mac") || lower.endsWith(".conf") || lower.endsWith(".par");
};
window.lilakWebFormat.bindNavigatorEntry = function(element, entry, handlers) {
  element.addEventListener("click", () => {
    if (entry && entry.is_dir) {
      if (handlers && handlers.clickDir) handlers.clickDir(entry);
      return;
    }
    if (handlers && handlers.clickFile) handlers.clickFile(entry);
  });
  element.addEventListener("dblclick", () => {
    if (entry && entry.is_dir) {
      if (handlers && handlers.openDir) handlers.openDir(entry);
      return;
    }
    if (handlers && handlers.openFile) handlers.openFile(entry);
  });
};
window.lilakWebFormat.toggleCollapsiblePanel = function(options) {
  options = options || {};
  const workspace = typeof options.workspace === "string" ? document.getElementById(options.workspace) : options.workspace;
  const button = typeof options.button === "string" ? document.getElementById(options.button) : options.button;
  const className = options.className || "summary-collapsed";
  if (!workspace) return false;
  const collapsed = !workspace.classList.contains(className);
  workspace.classList.toggle(className, collapsed);
  if (button) button.classList.toggle("active-toggle", collapsed);
  if (typeof options.afterToggle === "function") requestAnimationFrame(options.afterToggle);
  return collapsed;
};
window.lilakWebFormat.renderNavigatorEntries = function(options) {
  const list = document.getElementById(options.listId || "navList");
  if (!list) return;
  const entries = options.entries || [];
  const escapeHtml = options.escapeHtml || (value => String(value ?? ""));
  const parameterPredicate = options.parameterPredicate || window.lilakWebFormat.isParameterFile;
  list.innerHTML = entries.map((entry, index) => {
    const isParameter = !entry.is_dir && parameterPredicate(entry.name || "");
    const detail = entry.detail ? `<span class="detail">${escapeHtml(entry.detail)}</span>` : "";
    const kindLabel = entry.kind_label || (entry.is_dir ? "dir" : "file");
    const classes = [
      "navigator-entry",
      entry.is_dir ? "dir-entry" : (isParameter ? "conf-entry" : "file-entry"),
    ].join(" ");
    return `
      <div class="${classes}" data-entry-index="${index}">
        <span class="name">${escapeHtml(entry.name || "")}${detail}</span>
        <span>${escapeHtml(kindLabel)}</span>
      </div>
    `;
  }).join("") || '<div class="help">No entries</div>';
  list.querySelectorAll(".navigator-entry").forEach((element) => {
    const entry = entries[Number(element.dataset.entryIndex)];
    window.lilakWebFormat.bindNavigatorEntry(element, entry, options.handlers || {});
  });
};
window.lilakWebFormat.parameterValueSheetHtml = function(rows, options) {
  options = options || {};
  const escapeHtml = options.escapeHtml || (value => String(value ?? ""));
  const ownerId = options.ownerId || "sheet";
  const extra = options.extra ? "1" : "0";
  const sourceRows = (rows || []).map((row, index) => ({ row, index }));
  const orderedRows = options.groupView === false
    ? sourceRows
    : (() => {
        const groups = [];
        const byGroup = new Map();
        for (const item of sourceRows) {
          const group = item.row && item.row.kind === "parameter" ? (item.row.group || "") : "";
          if (!byGroup.has(group)) {
            byGroup.set(group, []);
            groups.push(group);
          }
          byGroup.get(group).push(item);
        }
        groups.sort((a, b) => {
          if (a === "lilak") return -1;
          if (b === "lilak") return 1;
          if (a === "LKRun") return b === "lilak" ? 1 : -1;
          if (b === "LKRun") return a === "lilak" ? -1 : 1;
          return String(a).localeCompare(String(b));
        });
        return groups.flatMap(group => [{ groupHeader: group || "(no group)" }, ...byGroup.get(group)]);
      })();
  const body = orderedRows.map((item) => {
    if (item.groupHeader != null) {
      return `<tr class="group-header"><td colspan="6">${escapeHtml(item.groupHeader)}</td></tr>`;
    }
    const row = item.row;
    const index = item.index;
    if (row && row.kind === "comment") {
      return `
        <tr class="comment-row">
          <td>${index + 1}</td>
          <td colspan="5">${escapeHtml(row.comment || "")}</td>
        </tr>
      `;
    }
    const commented = row.unit === "#" || row.enabled === false;
    return `
      <tr class="${commented ? "commented-parameter-row" : ""}" data-sheet-row-index="${index}">
        <td>${index + 1}</td>
        <td class="readonly">${escapeHtml(row.group || "")}</td>
        <td class="readonly">${escapeHtml(row.name || "")}</td>
        <td><input type="text" value="${escapeHtml(row.value || "")}" data-kind="value" data-owner="${escapeHtml(ownerId)}" data-index="${index}" data-extra="${extra}" ${commented ? "disabled" : ""}></td>
        <td><input type="text" value="${escapeHtml(row.unit || "")}" data-kind="unit" data-owner="${escapeHtml(ownerId)}" data-index="${index}" data-extra="${extra}"></td>
        <td class="readonly">${escapeHtml(row.comment || "")}</td>
      </tr>
    `;
  }).join("") || '<tr><td colspan="6">No rows</td></tr>';
  return `
    <div class="parameter-sheet-wrap">
      <table class="parameter-sheet">
        <thead>
          <tr>
            <th>#</th>
            <th>group</th>
            <th>name</th>
            <th>value</th>
            <th>unit</th>
            <th>comment</th>
          </tr>
        </thead>
        <tbody>${body}</tbody>
      </table>
    </div>
  `;
};
"""


def shortcuts_button_html(button_id: str = "shortcutsBtn", label: str = "Shortcuts") -> str:
    return f'<button id="{html.escape(button_id)}" onclick="window.lilakWebFormat.toggleShortcutsModal()">{html.escape(label)}</button>'


def shortcuts_modal_html(rows, modal_id: str = "shortcutsModal", title: str = "Shortcuts", close_label: str = "Close") -> str:
    body = "\n".join(
        f"          <tr><td>{html.escape(str(shortcut))}</td><td>{html.escape(str(description))}</td></tr>"
        for shortcut, description in rows
    )
    return f"""  <div class="modal" id="{html.escape(modal_id)}">
    <div class="modal-card">
      <div class="modal-head">
        <h3>{html.escape(title)}</h3>
        <div class="toolbar">
          <button onclick="window.lilakWebFormat.closeShortcutsModal('{html.escape(modal_id)}')">{html.escape(close_label)}</button>
        </div>
      </div>
      <div class="parameter-sheet-wrap">
        <table class="parameter-sheet shortcut-sheet">
          <thead>
            <tr><th>Shortcut</th><th>Description</th></tr>
          </thead>
          <tbody>
{body}
          </tbody>
        </table>
      </div>
    </div>
  </div>"""


def build_lilak_web_page(title: str, body_html: str, script_js: str, extra_style: str = "") -> str:
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title}</title>
  <style>
{SHARED_CSS}
{extra_style}
  </style>
</head>
<body>
{body_html}
<script>
{SHARED_JS}
{script_js}
</script>
</body>
</html>
"""


def rebuild_legacy_page(legacy_html: str, title: str | None = None, extra_style: str = "") -> str:
    body_match = re.search(r"<body>(.*)</body>", legacy_html, re.S | re.I)
    script_match = re.search(r"<script>(.*)</script>\s*</body>", legacy_html, re.S | re.I)
    title_match = re.search(r"<title>(.*?)</title>", legacy_html, re.S | re.I)
    style_matches = re.findall(r"<style>(.*?)</style>", legacy_html, re.S | re.I)
    body_html = body_match.group(1).strip() if body_match else legacy_html
    script_js = script_match.group(1).strip() if script_match else ""
    page_title = title or (title_match.group(1).strip() if title_match else "LILAK")
    merged_extra_style = "\n".join(style_matches)
    if extra_style:
      merged_extra_style = f"{merged_extra_style}\n{extra_style}" if merged_extra_style else extra_style
    if script_match:
      body_html = re.sub(r"<script>.*</script>\s*$", "", body_html, flags=re.S | re.I).strip()
    return build_lilak_web_page(page_title, body_html, script_js, extra_style=merged_extra_style)

#!/usr/bin/env python3
"""
Host-side AI Serial Bridge for AetherOS.
Connects to QEMU's COM1 TCP serial socket (127.0.0.1:4444), listens for
[AI_REQ]<prompt> lines, and responds with:
  - Optional [AI_CMD]<shell_command> (executed directly by AetherOS!)
  - [AI_RES]<answer> (displayed in the AI Assistant window)
"""

from __future__ import annotations

import json
import os
import re
import socket
import time
import urllib.request

HOST = "127.0.0.1"
PORT = 4444

SYSTEM_PROMPT = (
    "You are AetherOS's kernel AI agent. Available shell commands: "
    "ls, cat <file>, touch <file>, write <file> <text>, rm <file>, sync, "
    "ps, spawn <name>, kill <pid>, mem, uptime, uname, calc <a op b>, hexdump <0xADDR>. "
    "If the user wants an OS action, respond with TWO lines:\n"
    "CMD: <exact shell command>\n"
    "MSG: <short plain-text explanation under 160 chars>\n"
    "Otherwise respond with one line:\n"
    "MSG: <short plain-text answer under 220 chars>"
)


def ask_gemini(prompt: str, api_key: str) -> tuple[str | None, str]:
    url = (
        "https://generativelanguage.googleapis.com/v1beta/models/"
        f"gemini-2.5-flash:generateContent?key={api_key}"
    )
    payload = {
        "contents": [{"parts": [{"text": f"{SYSTEM_PROMPT}\n\nUser: {prompt}"}]}]
    }
    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=15) as resp:
        data = json.loads(resp.read().decode("utf-8"))
        text = data["candidates"][0]["content"]["parts"][0]["text"].strip()
        cmd = None
        msg = text
        for line in text.splitlines():
            if line.startswith("CMD:"):
                cmd = line[4:].strip()
            elif line.startswith("MSG:"):
                msg = line[4:].strip()
        return cmd, " ".join(msg.split())[:300]


def local_intent_agent(prompt: str) -> tuple[str | None, str]:
    p = prompt.strip()
    low = p.lower()

    m = re.match(r"create\s+(?:a\s+)?(?:file\s+)?(\S+)\s+with\s+(.+)", p, re.I)
    if m:
        fname, content = m.group(1), m.group(2)
        return f"write {fname} {content}", f"Created '{fname}' and wrote your text."

    m = re.match(r"(?:read|open|show)\s+(?:file\s+)?(\S+)", p, re.I)
    if m and not any(w in low for w in ["memory", "task", "process", "file list", "files"]):
        fname = m.group(1)
        return f"cat {fname}", f"Displayed contents of {fname} in Terminal."

    if any(w in low for w in ["list file", "show file", "files", "directory"]):
        return "ls", "Listed all files in Terminal."
    if any(w in low for w in ["save", "persist", "disk", "sync"]):
        return "sync", "Synced filesystem to ATA disk (LBA 200)."
    if any(w in low for w in ["process", "task", "running"]):
        return "ps", "Listed active kernel tasks in Terminal."
    if any(w in low for w in ["memory", "ram", "heap"]):
        return "mem", "Checked live kernel heap usage."
    if "uptime" in low:
        return "uptime", "Checked OS uptime."

    m = re.search(r"spawn\s+(\S+)", p, re.I)
    if m:
        return f"spawn {m.group(1)}", f"Spawned kernel task '{m.group(1)}'."

    m = re.search(r"(\d+\s*[\+\-\*/%]\s*\d+)", p)
    if m:
        expr = re.sub(r"(\d+)\s*([\+\-\*/%])\s*(\d+)", r"\1 \2 \3", m.group(1))
        return f"calc {expr}", f"Evaluated '{expr}' in Terminal."

    return None, f"Received '{p}'. Try 'ai create todo.txt with test' or set GEMINI_API_KEY."


def handle_prompt(prompt: str) -> tuple[str | None, str]:
    api_key = os.environ.get("GEMINI_API_KEY", "").strip()
    if api_key:
        try:
            return ask_gemini(prompt, api_key)
        except Exception as exc:
            cmd, fallback = local_intent_agent(prompt)
            return cmd, f"API err ({exc}); {fallback}"
    return local_intent_agent(prompt)


def main():
    print(f"[AI Bridge] Waiting for QEMU serial on {HOST}:{PORT}...")
    while True:
        try:
            with socket.create_connection((HOST, PORT), timeout=5) as sock:
                sock.settimeout(None)
                print("[AI Bridge] Connected to AetherOS COM1!")
                buf = ""
                while True:
                    chunk = sock.recv(1024)
                    if not chunk:
                        break
                    buf += chunk.decode("utf-8", errors="replace")
                    while "\n" in buf:
                        line, buf = buf.split("\n", 1)
                        line = line.strip("\r")
                        if line:
                            print(f"[OS] {line}")
                        if line.startswith("[AI_REQ]"):
                            prompt = line[len("[AI_REQ]") :].strip()
                            cmd, reply = handle_prompt(prompt)
                            if cmd:
                                sock.sendall(f"[AI_CMD]{cmd}\n".encode("utf-8"))
                                print(f"[Bridge -> OS CMD] {cmd}")
                            sock.sendall(f"[AI_RES]{reply}\n".encode("utf-8"))
                            print(f"[Bridge -> OS RES] {reply}")
        except (ConnectionRefusedError, TimeoutError, OSError):
            time.sleep(1.0)


if __name__ == "__main__":
    main()

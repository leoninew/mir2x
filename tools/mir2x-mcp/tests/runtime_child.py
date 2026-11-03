"""Isolated fake game executable: a real listener and a closable Windows window."""

import argparse
import ctypes
import hashlib
import os
import socket
import threading
import time
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--client-port", type=int)
parser.add_argument("--auto-login")
parser.add_argument("--expect-login-sha256")
parser.add_argument("--ai-bridge-dir")
parser.add_argument("--expect-bridge-dir")
args, _ = parser.parse_known_args()
if args.expect_login_sha256:
    assert hashlib.sha256((args.auto_login or "").encode()).hexdigest() == args.expect_login_sha256
    print("synthetic login argument verified", flush=True)
if args.expect_bridge_dir:
    assert Path(args.ai_bridge_dir).resolve() == Path(args.expect_bridge_dir).resolve()
    print("synthetic bridge argument verified", flush=True)
listener = None
if args.client_port:
    listener = socket.socket()
    listener.bind(("127.0.0.1", args.client_port))
    listener.listen()

    def accept():
        while True:
            connection, _ = listener.accept()
            connection.close()

    threading.Thread(target=accept, daemon=True).start()
print("runtime child ready", flush=True)

if os.name == "nt":
    from ctypes import wintypes

    user32 = ctypes.WinDLL("user32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    callback = ctypes.WINFUNCTYPE(
        ctypes.c_ssize_t, wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM
    )

    class WindowClass(ctypes.Structure):
        _fields_ = [
            ("style", wintypes.UINT),
            ("proc", callback),
            ("class_extra", ctypes.c_int),
            ("window_extra", ctypes.c_int),
            ("instance", wintypes.HINSTANCE),
            ("icon", wintypes.HICON),
            ("cursor", wintypes.HANDLE),
            ("brush", wintypes.HBRUSH),
            ("menu", wintypes.LPCWSTR),
            ("name", wintypes.LPCWSTR),
        ]

    user32.DefWindowProcW.argtypes = [
        wintypes.HWND,
        wintypes.UINT,
        wintypes.WPARAM,
        wintypes.LPARAM,
    ]
    user32.DefWindowProcW.restype = ctypes.c_ssize_t
    user32.DestroyWindow.argtypes = [wintypes.HWND]

    @callback
    def procedure(window, message, wparam, lparam):
        if message == 0x10:
            user32.DestroyWindow(window)
            return 0
        if message == 2:
            user32.PostQuitMessage(0)
            return 0
        return user32.DefWindowProcW(window, message, wparam, lparam)

    kernel32.GetModuleHandleW.restype = wintypes.HMODULE
    instance = kernel32.GetModuleHandleW(None)
    kind = WindowClass(instance=instance, proc=procedure, name="Mir2xRuntimeTest")
    user32.RegisterClassW.argtypes = [ctypes.POINTER(WindowClass)]
    assert user32.RegisterClassW(ctypes.byref(kind))
    user32.CreateWindowExW.argtypes = [
        wintypes.DWORD,
        wintypes.LPCWSTR,
        wintypes.LPCWSTR,
        wintypes.DWORD,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        wintypes.HWND,
        wintypes.HMENU,
        wintypes.HINSTANCE,
        wintypes.LPVOID,
    ]
    user32.CreateWindowExW.restype = wintypes.HWND
    assert user32.CreateWindowExW(
        0, kind.name, "Isolated runtime test", 0, 0, 0, 1, 1, None, None, instance, None
    )
    message = wintypes.MSG()
    while user32.GetMessageW(ctypes.byref(message), None, 0, 0) > 0:
        user32.TranslateMessage(ctypes.byref(message))
        user32.DispatchMessageW(ctypes.byref(message))
else:
    while True:
        time.sleep(1)

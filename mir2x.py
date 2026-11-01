#!/usr/bin/env python3
"""Source-tree entry; the installed distribution also provides mir2x.exe."""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools" / "mir2x-mcp" / "src"))

if __name__ == "__main__":
    from mir2x_mcp.runtime_cli import main

    main(["--root", str(ROOT), *sys.argv[1:]])

from pathlib import Path
import re
import subprocess

Import("env")

ROOT = Path(env.subst("$PROJECT_DIR"))
CONFIG = ROOT / "src" / "config.h"
OUT = ROOT / "include" / "version_generated.h"


def git(*args):
    try:
        return subprocess.check_output(
            ["git", *args],
            cwd=ROOT,
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except Exception:
        return ""


def version_base():
    text = CONFIG.read_text(encoding="utf-8")
    match = re.search(r'#define\s+APP_VERSION_BASE\s+"([^"]+)"', text)
    if not match:
        raise RuntimeError("APP_VERSION_BASE fehlt in src/config.h")
    return match.group(1)


base = version_base()
short_hash = git("rev-parse", "--short=7", "HEAD")
exact_tag = git("describe", "--tags", "--exact-match", "HEAD")
dirty = bool(git("status", "--porcelain"))

version = base if exact_tag == f"v{base}" and not dirty else f"{base}+{short_hash or 'unknown'}"
OUT.write_text(
    "#pragma once\n"
    "// Automatisch durch scripts/version_label.py erzeugt. Nicht manuell bearbeiten.\n"
    f'#define APP_VERSION_LABEL "Dottie {version}"\n',
    encoding="utf-8",
)

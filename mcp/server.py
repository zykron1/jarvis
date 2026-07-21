import os
import re
import fnmatch
import hashlib
import shutil
import time
import subprocess
from pathlib import Path
from datetime import datetime

from mcp.server.fastmcp import FastMCP

mcp = FastMCP("Coding Agent")

WORKSPACE_ROOT = Path(os.environ.get("JARVIS_WORKSPACE", ".")).resolve()
WORKSPACE_ROOT.mkdir(parents=True, exist_ok=True)
SNAPSHOT_DIR = WORKSPACE_ROOT / ".jarvis_snapshots"


def _resolve(path: str) -> Path:
    target = (WORKSPACE_ROOT / path).resolve()
    if not str(target).startswith(str(WORKSPACE_ROOT)):
        raise ValueError(f"Path escapes workspace: {path}")
    return target


def _snapshot_key(abs_path: Path) -> str:
    return hashlib.sha256(str(abs_path).encode()).hexdigest()[:16]


def _snapshot_file_path(abs_path: Path) -> Path:
    key = _snapshot_key(abs_path)
    safe_name = str(abs_path).replace("/", "_").replace(".", "_")
    return SNAPSHOT_DIR / key / f"{safe_name}.{int(time.time())}.bak"


def _save_snapshot(abs_path: Path):
    if not abs_path.exists() or not abs_path.is_file():
        return
    SNAPSHOT_DIR.mkdir(parents=True, exist_ok=True)
    dest = _snapshot_file_path(abs_path)
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(abs_path, dest)


@mcp.tool()
def read_file(path: str, offset: int = 0, limit: int = 0) -> str:
    """
    Read the contents of a file. Returns text with line numbers.

    Args:
        path: Relative path to the file (within workspace)
        offset: Line number to start from (1-indexed, default 0 = from start)
        limit: Max number of lines to return (0 = all lines)
    """
    target = _resolve(path)
    if not target.exists():
        raise FileNotFoundError(f"File not found: {path}")
    if not target.is_file():
        raise IsADirectoryError(f"Not a file: {path}")

    lines = target.read_text(errors="replace").splitlines(keepends=True)

    start = max(0, offset - 1) if offset > 0 else 0
    end = start + limit if limit > 0 else len(lines)
    end = min(end, len(lines))

    numbered = []
    for i, line in enumerate(lines[start:end], start=start + 1):
        numbered.append(f"{i:>6}: {line.rstrip()}")

    return "\n".join(numbered)


@mcp.tool()
def write_file(path: str, content: str) -> str:
    """
    Create or overwrite a file with the given content. Creates parent directories as needed.

    Args:
        path: Relative path to the file (within workspace)
        content: The full file content to write
    """
    target = _resolve(path)
    _save_snapshot(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content)
    return f"Wrote {len(content)} bytes to {path}"


@mcp.tool()
def edit_file(path: str, old_string: str, new_string: str, replace_all: bool = False) -> str:
    """
    Replace exact text in a file. Fails if old_string is not found (or found multiple times without replace_all).

    Args:
        path: Relative path to the file (within workspace)
        old_string: The exact text to find and replace
        new_string: The text to replace it with
        replace_all: If true, replace all occurrences. If false, require exactly one match.
    """
    target = _resolve(path)
    if not target.exists():
        raise FileNotFoundError(f"File not found: {path}")

    _save_snapshot(target)
    content = target.read_text(errors="replace")
    count = content.count(old_string)

    if count == 0:
        raise ValueError(f"old_string not found in {path}")
    if count > 1 and not replace_all:
        raise ValueError(f"old_string found {count} times in {path}; use replace_all=true or provide more context")

    new_content = content.replace(old_string, new_string) if replace_all else content.replace(old_string, new_string, 1)
    target.write_text(new_content)

    action = "Replaced all" if replace_all else "Replaced first"
    return f"{action} occurrence of old_string in {path}"


@mcp.tool()
def list_files(path: str = ".", pattern: str = "") -> str:
    """
    List directory contents. Optionally filter by glob pattern.

    Args:
        path: Relative directory path (within workspace, default ".")
        pattern: Glob pattern to filter entries (e.g. "*.py", "**/*.json")
    """
    target = _resolve(path)
    if not target.exists():
        raise FileNotFoundError(f"Directory not found: {path}")
    if not target.is_dir():
        raise NotADirectoryError(f"Not a directory: {path}")

    entries = []
    for entry in sorted(target.iterdir()):
        name = entry.name
        prefix = "d" if entry.is_dir() else "f"
        if pattern:
            if "**" in pattern:
                if not fnmatch.fnmatch(str(entry.relative_to(WORKSPACE_ROOT)), pattern):
                    continue
            else:
                if not fnmatch.fnmatch(name, pattern):
                    continue
        entries.append(f"[{prefix}] {name}")

    if not entries:
        return "(empty)"

    return "\n".join(entries)


@mcp.tool()
def search_content(pattern: str, path: str = ".", include: str = "") -> str:
    """
    Search file contents using regex (like grep). Returns matching lines with file:line.

    Args:
        pattern: Regex pattern to search for
        path: Relative directory or file path to search in (default ".")
        include: Glob pattern to filter files (e.g. "*.py", "*.cpp")
    """
    target = _resolve(path)
    if not target.exists():
        raise FileNotFoundError(f"Path not found: {path}")

    regex = re.compile(pattern)
    results = []

    files_to_search = []
    if target.is_file():
        files_to_search = [target]
    else:
        for root, dirs, files in os.walk(target):
            dirs[:] = [d for d in dirs if not d.startswith(".") and d != "__pycache__"]
            for fname in files:
                if include and not fnmatch.fnmatch(fname, include):
                    continue
                files_to_search.append(Path(root) / fname)

    for fpath in files_to_search:
        try:
            lines = fpath.read_text(errors="replace").splitlines()
        except (PermissionError, OSError):
            continue
        for i, line in enumerate(lines, 1):
            if regex.search(line):
                rel = fpath.relative_to(WORKSPACE_ROOT)
                results.append(f"{rel}:{i}: {line.rstrip()}")

    if not results:
        return "(no matches)"

    return "\n".join(results[:200])


@mcp.tool()
def search_files(pattern: str, path: str = ".") -> str:
    """
    Search for files by name using glob pattern (like find). Supports ** for recursive matching.

    Args:
        pattern: Glob pattern for filenames (e.g. "*.py", "**/*.json", "test_*")
        path: Relative directory to search in (default ".")
    """
    target = _resolve(path)
    if not target.exists():
        raise FileNotFoundError(f"Path not found: {path}")
    if not target.is_dir():
        raise NotADirectoryError(f"Not a directory: {path}")

    matches = []
    for root, dirs, files in os.walk(target):
        dirs[:] = [d for d in dirs if not d.startswith(".") and d != "__pycache__"]
        for fname in dirs + files:
            full = Path(root) / fname
            rel = full.relative_to(WORKSPACE_ROOT)
            if fnmatch.fnmatch(rel, pattern) or fnmatch.fnmatch(fname, pattern):
                prefix = "d" if full.is_dir() else "f"
                matches.append(f"[{prefix}] {rel}")

    if not matches:
        return "(no matches)"

    return "\n".join(sorted(matches)[:200])


@mcp.tool()
def get_file_info(path: str) -> str:
    """
    Get metadata about a file or directory: size, last modified, type, permissions.

    Args:
        path: Relative path to the file or directory (within workspace)
    """
    target = _resolve(path)
    if not target.exists():
        raise FileNotFoundError(f"Not found: {path}")

    stat = target.stat()
    kind = "directory" if target.is_dir() else "file"
    size = stat.st_size
    mtime = datetime.fromtimestamp(stat.st_mtime).isoformat()

    parts = [
        f"Type: {kind}",
        f"Size: {size} bytes",
        f"Modified: {mtime}",
        f"Path: {target.relative_to(WORKSPACE_ROOT)}",
    ]

    if target.is_file():
        lines = target.read_text(errors="replace").splitlines()
        parts.append(f"Lines: {len(lines)}")

    return "\n".join(parts)


@mcp.tool()
def create_directory(path: str) -> str:
    """
    Create a directory (and any missing parents).

    Args:
        path: Relative directory path to create (within workspace)
    """
    target = _resolve(path)
    target.mkdir(parents=True, exist_ok=True)
    return f"Created directory: {path}"


@mcp.tool()
def delete_path(path: str) -> str:
    """
    Delete a file or directory. Snapshots the file first if it exists and is a file.

    Args:
        path: Relative path to delete (within workspace)
    """
    target = _resolve(path)
    if not target.exists():
        raise FileNotFoundError(f"Not found: {path}")

    _save_snapshot(target)

    if target.is_dir():
        shutil.rmtree(target)
        return f"Deleted directory: {path}"

    target.unlink()
    return f"Deleted file: {path}"


@mcp.tool()
def undo_edit(path: str) -> str:
    """
    Restore the most recent snapshot of a file before its last modification.

    Args:
        path: Relative path to the file to restore (within workspace)
    """
    target = _resolve(path)
    key = _snapshot_key(target)
    snapshot_dir = SNAPSHOT_DIR / key

    if not snapshot_dir.exists():
        raise FileNotFoundError(f"No snapshots found for {path}")

    snapshots = sorted(snapshot_dir.iterdir(), key=lambda f: f.name, reverse=True)
    if not snapshots:
        raise FileNotFoundError(f"No snapshots found for {path}")

    latest = snapshots[0]
    shutil.copy2(latest, target)
    return f"Restored {path} from snapshot {latest.name}"


@mcp.tool()
def bash(command: str, timeout: int = 30) -> str:
    """
    Run a shell command in the workspace directory. Returns stdout and stderr.

    Args:
        command: The shell command to execute
        timeout: Maximum seconds to wait (default 30, max 120)
    """
    timeout = min(max(timeout, 1), 120)

    try:
        result = subprocess.run(
            command,
            shell=True,
            cwd=str(WORKSPACE_ROOT),
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired:
        return f"Command timed out after {timeout}s: {command}"

    parts = []
    if result.stdout:
        parts.append(result.stdout.rstrip())
    if result.stderr:
        parts.append(result.stderr.rstrip())
    if not parts:
        parts.append("(no output)")

    parts.append(f"\nExit code: {result.returncode}")
    return "\n".join(parts)


if __name__ == "__main__":
    mcp.run()

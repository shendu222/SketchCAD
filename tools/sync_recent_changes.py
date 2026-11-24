#!/usr/bin/env python3
"""将最近修改内容同步到“最新版本”目标目录。

源内容：当前 Git 工作区相对 --base 的差异（默认 HEAD），包含已跟踪修改/删除
        与允许名单内的未跟踪新文件；自动排除构建产物与缓存目录。
目标版本：--target 指定目录；若目录为空且带 --init-from-head，则从同一 base
          用 `git archive` 初始化为“最新版本”快照。
同步方式：差异更新（三态比较 source/base/target），不是整目录覆盖。
冲突规则：target != base 且 source != base 视为冲突。默认 fail；可选 backup/overwrite。
验证结果：逐文件 SHA-256 对比、JSON 语法校验、可选外部验证命令，并写出报告。

示例：
  python tools/sync_recent_changes.py --target build/sync-target/latest --init-from-head
  python tools/sync_recent_changes.py --base HEAD~1 --target ../sketchCAD-latest --dry-run
  python tools/sync_recent_changes.py --on-conflict backup --verify-command "python -m json.tool config/command_catalog.json"
"""

import argparse
import fnmatch
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path

DEFAULT_INCLUDES = [
    "CMakeLists.txt", "README*", "RELEASE_NOTES.md", "COPYING", "LICENSE.txt",
    "src/**", "tests/**", "docs/**", "config/**", "sdk/**", "tools/**",
    "packaging/**", "resources/**", "hatch/**", "plotstyle_samples/**",
]

DEFAULT_EXCLUDES = [
    ".git/**", ".codebuddy/**", "build/**", "build-*/**", "dist/**",
    "release/**", "**/__pycache__/**", "**/*.pyc", "**/*.obj", "**/*.o",
    "**/*.exe", "**/*.dll", "**/*.a", "**/*.lib", "**/*.pdb", "**/*.qm",
]


def git_output(repo: Path, args: list[str], check: bool = True) -> bytes:
    result = subprocess.run(["git", "-C", str(repo), *args], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    if check and result.returncode != 0:
        raise RuntimeError(result.stderr.decode("utf-8", "replace").strip())
    return result.stdout


def path_allowed(rel: str, includes: list[str], excludes: list[str]) -> bool:
    rel = rel.replace("\\", "/").lstrip("./")
    if any(fnmatch.fnmatch(rel, pat) or rel.startswith(pat.rstrip("*") + "/")
           for pat in excludes):
        return False
    return any(fnmatch.fnmatch(rel, pat) or rel.startswith(pat.rstrip("*") + "/")
               for pat in includes)


def collect_changed_paths(repo: Path, base: str, includes: list[str], excludes: list[str]) -> list[str]:
    raw = git_output(repo, ["status", "--porcelain", "-z"]).decode("utf-8", "surrogateescape")
    paths: set[str] = set()
    for entry in raw.split("\0"):
        if not entry:
            continue
        status, tail = entry[:2], entry[3:]
        if " -> " in tail:
            tail = tail.split(" -> ", 1)[1]  # rename：同步新路径
        p = repo / tail
        dir_rel = tail.replace("\\", "/").rstrip("/")
        if p.is_dir():
            if not path_allowed(dir_rel + "/__probe__", includes, excludes):
                continue
            for child in p.rglob("*"):
                if child.is_file():
                    child_rel = child.relative_to(repo).as_posix()
                    if path_allowed(child_rel, includes, excludes):
                        paths.add(child_rel)
            continue
        if path_allowed(tail.replace("\\", "/"), includes, excludes):
            paths.add(tail.replace("\\", "/"))
    return sorted(paths)


def base_bytes(repo: Path, base: str, rel: str, cache: dict[str, bytes | None]) -> bytes | None:
    if rel in cache:
        return cache[rel]
    result = subprocess.run(["git", "-C", str(repo), "show", f"{base}:{rel}"],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    cache[rel] = result.stdout if result.returncode == 0 else None
    return cache[rel]


def source_bytes(repo: Path, rel: str) -> bytes | None:
    p = repo / rel
    return p.read_bytes() if p.is_file() else None


def target_bytes(target: Path, rel: str) -> bytes | None:
    p = target / rel
    return p.read_bytes() if p.is_file() else None


def normalized(data: bytes | None) -> bytes | None:
    """统一 CRLF/CR 为 LF，避免 git archive 与工作区的 EOL 差异误判为冲突。"""
    if data is None:
        return None
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def content_equal(a: bytes | None, b: bytes | None) -> bool:
    return normalized(a) == normalized(b)


def sha256(data: bytes | None) -> str:
    data = normalized(data)
    return hashlib.sha256(data).hexdigest() if data is not None else ""


def init_target_from_head(repo: Path, target: Path, base: str) -> None:
    if target.exists() and any(target.iterdir()):
        return
    target.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        zip_path = Path(tmp) / "latest.zip"
        subprocess.run(["git", "-C", str(repo), "archive", "--format=zip", base,
                        "-o", str(zip_path)], check=True)
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(target)


def classify(rel: str, src: bytes | None, base: bytes | None, tgt: bytes | None) -> tuple[str, str]:
    if content_equal(src, tgt):
        return "skip", "源与目标一致"
    if content_equal(tgt, base):
        if src is None:
            return "delete", "源文件已删除且目标仍为基线内容"
        if base is None:
            return "add", "目标缺少该新增文件"
        return "update", "目标仍为基线内容，可差异更新"
    return "conflict", "源与目标均相对基线发生变化"


def apply_action(target: Path, rel: str, action: str, src: bytes | None,
                 conflict_mode: str) -> tuple[str, str]:
    dest = target / rel
    if action == "skip":
        return "skip", ""
    if action == "conflict":
        if conflict_mode == "backup":
            incoming = dest.with_name(dest.name + ".sync-incoming")
            if src is not None:
                incoming.parent.mkdir(parents=True, exist_ok=True)
                incoming.write_bytes(src)
                return "conflict-backup", f"已保留目标文件，源版本写入 {incoming.name}"
            return "conflict-backup", "源端已删除，目标被保留"
        if conflict_mode == "overwrite":
            if src is None:
                if dest.exists():
                    dest.unlink()
                return "overwrite-delete", "按覆盖策略删除目标文件"
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(src)
            return "overwrite-write", "按覆盖策略写入源文件"
        return "conflict", "未修改目标"
    if action == "delete":
        if dest.exists():
            dest.unlink()
        return "delete", "已删除目标文件"
    if src is None:
        return "error", "源文件读取失败"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(src)
    return action, "已写入"


def verify(target: Path, applied: list[dict], verify_command: str | None) -> list[dict]:
    failures: list[dict] = []
    for item in applied:
        rel, action = item["path"], item["result"]
        if action in {"conflict", "conflict-backup"}:
            failures.append({"path": rel, "reason": "冲突未解决"})
            continue
        dest = target / rel
        if action == "delete":
            if dest.exists():
                failures.append({"path": rel, "reason": "目标文件仍存在"})
            continue
        if not dest.is_file():
            failures.append({"path": rel, "reason": "目标文件缺失"})
            continue
        if sha256(dest.read_bytes()) != item["source_sha256"]:
            failures.append({"path": rel, "reason": "SHA-256 不一致"})
            continue
        if rel.endswith(".json"):
            try:
                json.loads(dest.read_text(encoding="utf-8"))
            except Exception as exc:
                failures.append({"path": rel, "reason": f"JSON 语法错误: {exc}"})
    if verify_command:
        result = subprocess.run(verify_command, shell=True, cwd=str(target),
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if result.returncode != 0:
            failures.append({"path": "<verify-command>", "reason":
                             (result.stdout + result.stderr).strip() or "验证命令失败"})
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description="同步最近修改到最新版本目标目录")
    parser.add_argument("--repo", default=str(Path(__file__).resolve().parents[1]),
                        help="源仓库路径，默认脚本上一级目录")
    parser.add_argument("--base", default="HEAD", help="差异基线，默认 HEAD；可用 HEAD~1/标签/提交")
    parser.add_argument("--target", required=True, help="最新版本目标目录")
    parser.add_argument("--init-from-head", action="store_true",
                        help="目标为空时用 git archive <base> 初始化")
    parser.add_argument("--on-conflict", choices=["fail", "backup", "overwrite"], default="fail",
                        help="冲突处理策略，默认 fail")
    parser.add_argument("--dry-run", action="store_true", help="只输出计划，不写目标")
    parser.add_argument("--include", default=",".join(DEFAULT_INCLUDES), help="包含 glob，逗号分隔")
    parser.add_argument("--exclude", default=",".join(DEFAULT_EXCLUDES), help="排除 glob，逗号分隔")
    parser.add_argument("--verify-command", default=None, help="同步后执行的额外验证命令")
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    target = Path(args.target).resolve()
    includes = [p.strip() for p in args.include.split(",") if p.strip()]
    excludes = [p.strip() for p in args.exclude.split(",") if p.strip()]

    try:
        paths = collect_changed_paths(repo, args.base, includes, excludes)
        if args.init_from_head and not args.dry_run:
            init_target_from_head(repo, target, args.base)
        if not target.exists() and not args.init_from_head:
            raise RuntimeError(f"目标目录不存在：{target}（可加 --init-from-head）")

        base_cache: dict[str, bytes | None] = {}
        plan, applied = [], []
        for rel in paths:
            src = source_bytes(repo, rel)
            base = base_bytes(repo, args.base, rel, base_cache)
            tgt = target_bytes(target, rel)
            if (args.init_from_head and base is not None and
                    (not target.exists() or not any(target.iterdir()))):
                tgt = base  # dry-run 模拟从 base 初始化的目标内容
            action, reason = classify(rel, src, base, tgt)
            plan.append({"path": rel, "action": action, "reason": reason,
                         "source_sha256": sha256(src), "target_sha256": sha256(tgt)})
            if args.dry_run:
                continue
            result, detail = apply_action(target, rel, action, src, args.on_conflict)
            applied.append({"path": rel, "result": result, "detail": detail,
                            "source_sha256": sha256(src)})

        if args.dry_run:
            print(json.dumps({"mode": "dry-run", "base": args.base, "target": str(target),
                              "files": plan}, ensure_ascii=False, indent=2))
            return 0

        failures = verify(target, applied, args.verify_command)
        report = {
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "source_repo": str(repo),
            "base": args.base,
            "target": str(target),
            "sync_mode": "three-way-diff-update",
            "conflict_policy": args.on_conflict,
            "files": applied,
            "verification_failures": failures,
            "ok": not failures,
        }
        report_path = target / ".sync-report.json"
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                               encoding="utf-8")
        print(f"同步文件数: {len(applied)}，验证失败数: {len(failures)}")
        print(f"报告: {report_path}")
        for f in failures:
            print(f"FAIL {f['path']}: {f['reason']}", file=sys.stderr)
        return 0 if not failures else 2
    except Exception as exc:
        print(f"同步失败: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

"""
NOVA interactive REPL (prototype, Python-backed).

This is Step 13 of the roadmap: an interactive environment similar to Python,
but "NOVA-shaped".

Until the full compiler is wired end-to-end, this REPL executes Python, with:
- stdlib constellations available via `:absorb cosmos.stats` etc.
- convenience commands for introspection
"""

from __future__ import annotations

import argparse
import code
import json
import os
import runpy
import shlex
import sys
import textwrap
import traceback
from dataclasses import dataclass
from pathlib import Path
from types import ModuleType
from typing import Any, Dict, Optional


HELP_TEXT = """\
NOVA REPL commands:

  :help                     Show this help
  :quit / :q / :exit         Exit the REPL
  :reset                    Clear the REPL environment
  :vars                     List currently defined variables
  :absorb <module>           Import a constellation/module into the session
                            Example: :absorb cosmos.stats
  :absorb <module>.{a,b}     Import specific names
                            Example: :absorb cosmos.stats.{ pearson, linear_fit }
  :parse                     Parse tiny NOVA subset (reads multi-line until a blank line)
  :type <expr>               Evaluate expr and show Python type
  :doc <name>                Show docstring (if available)

Notes:
  - This REPL is a prototype: it runs Python today.
  - `stdlib/` is automatically added to the import path.
"""


def _repo_root() -> Path:
    # toolchain/nova_repl/repl.py -> repo root
    return Path(__file__).resolve().parents[2]


def _ensure_stdlib_on_path() -> None:
    stdlib_path = _repo_root() / "stdlib"
    if str(stdlib_path) not in sys.path:
        sys.path.insert(0, str(stdlib_path))


def _format_banner() -> str:
    return (
        "nova> (prototype)\n"
        "Type :help for commands. This REPL currently executes Python.\n"
    )


def _safe_repr(x: Any, max_len: int = 500) -> str:
    try:
        s = repr(x)
    except Exception:
        s = f"<unreprable {type(x).__name__}>"
    if len(s) > max_len:
        return s[: max_len - 3] + "..."
    return s


def _parse_absorb(arg: str) -> tuple[str, Optional[list[str]]]:
    """
    Parse:
      cosmos.stats
      cosmos.stats.{ pearson, linear_fit }
    """
    s = arg.strip()
    if ".{" not in s:
        return s, None
    # very small parser (no nesting)
    mod, rest = s.split(".{", 1)
    if not rest.endswith("}"):
        raise ValueError("invalid absorb syntax; expected 'module.{ a, b }'")
    inside = rest[:-1].strip()
    if not inside:
        raise ValueError("invalid absorb syntax; empty import list")
    names = [x.strip() for x in inside.split(",") if x.strip()]
    return mod.strip(), names


@dataclass
class NovaConsole:
    locals: Dict[str, Any]

    def reset(self) -> None:
        self.locals.clear()
        # Keep a tiny standard prelude.
        self.locals.update(
            {
                "__name__": "__nova_repl__",
                "__doc__": None,
            }
        )

    def do_absorb(self, arg: str) -> None:
        module_name, names = _parse_absorb(arg)
        mod = __import__(module_name, fromlist=["*"])
        if names is None:
            # import module binding by its last segment, plus also allow full dotted access
            short = module_name.split(".")[-1]
            self.locals[short] = mod
            print(f"absorbed {module_name} as {short}")
            return
        for n in names:
            if not hasattr(mod, n):
                raise AttributeError(f"{module_name} has no attribute {n!r}")
            self.locals[n] = getattr(mod, n)
        print(f"absorbed {module_name}.{{{', '.join(names)}}}")

    def do_vars(self) -> None:
        keys = sorted(k for k in self.locals.keys() if not k.startswith("__"))
        if not keys:
            print("(no user variables yet)")
            return
        for k in keys:
            print(f"{k} = {_safe_repr(self.locals[k])}")

    def do_type(self, expr: str) -> None:
        val = eval(expr, self.locals, self.locals)
        print(f"{expr} : {type(val).__name__}")

    def do_doc(self, name: str) -> None:
        name = name.strip()
        if not name:
            raise ValueError("usage: :doc <name>")
        obj = self.locals.get(name)
        if obj is None:
            obj = eval(name, self.locals, self.locals)
        doc = getattr(obj, "__doc__", None) or "(no docstring)"
        print(textwrap.dedent(str(doc)).strip())

    def do_parse(self, src: str) -> None:
        from .tiny_parser import parse_tiny

        ast = parse_tiny(src)
        print(json.dumps(ast, indent=2, ensure_ascii=False))

    def handle_command(self, line: str) -> bool:
        """
        Returns True to continue, False to exit.
        """
        raw = line.strip()
        if raw in (":q", ":quit", ":exit"):
            return False
        if raw == ":help":
            print(HELP_TEXT)
            return True
        if raw == ":reset":
            self.reset()
            print("reset")
            return True
        if raw == ":vars":
            self.do_vars()
            return True
        if raw.startswith(":absorb "):
            self.do_absorb(raw[len(":absorb ") :])
            return True
        if raw.startswith(":type "):
            self.do_type(raw[len(":type ") :])
            return True
        if raw.startswith(":doc "):
            self.do_doc(raw[len(":doc ") :])
            return True
        if raw == ":parse":
            print("enter NOVA code; end with a blank line")
            lines: list[str] = []
            while True:
                s = input("... ")
                if not s.strip():
                    break
                lines.append(s)
            self.do_parse("\n".join(lines))
            return True
        raise ValueError(f"unknown command: {raw}")


class NovaInteractiveConsole(code.InteractiveConsole):
    def __init__(self, nova: NovaConsole):
        super().__init__(locals=nova.locals)
        self._nova = nova

    def raw_input(self, prompt: str = "") -> str:  # type: ignore[override]
        return input(prompt)

    def interact(self, banner: str | None = None, exitmsg: str | None = None) -> None:  # type: ignore[override]
        if banner:
            print(banner, end="" if banner.endswith("\n") else "\n")
        more = False
        while True:
            try:
                prompt = "... " if more else "nova> "
                line = self.raw_input(prompt)
            except EOFError:
                print()
                break
            except KeyboardInterrupt:
                print("\n(interrupted)")
                more = False
                continue

            if not more and line.strip().startswith(":"):
                try:
                    if not self._nova.handle_command(line):
                        break
                except Exception as e:
                    print(f"error: {e}")
                continue

            try:
                more = self.push(line)
            except SystemExit:
                raise
            except Exception:
                traceback.print_exc()
                more = False

        if exitmsg:
            print(exitmsg)


def _run_script(path: str, nova_locals: dict[str, Any]) -> int:
    p = Path(path)
    if not p.exists():
        print(f"error: script not found: {path}")
        return 2
    # execute the script in the REPL environment
    code_obj = compile(p.read_text(encoding="utf-8"), str(p), "exec")
    exec(code_obj, nova_locals, nova_locals)
    return 0


def main(argv: Optional[list[str]] = None) -> int:
    _ensure_stdlib_on_path()

    ap = argparse.ArgumentParser(prog="nova-repl", add_help=True)
    ap.add_argument("--exec", dest="exec_code", default=None, help="Execute a single line of code and exit")
    ap.add_argument("--script", dest="script", default=None, help="Execute a .py script inside the REPL environment")
    args = ap.parse_args(argv)

    nova = NovaConsole(locals={})
    nova.reset()

    # A small prelude: make cosmos/nova importable and visible.
    try:
        import cosmos  # noqa: F401
        import nova as nova_stdlib  # noqa: F401
        nova.locals["cosmos"] = sys.modules.get("cosmos")
        nova.locals["nova"] = sys.modules.get("nova")
    except Exception:
        # stdlib is optional; the REPL should still start
        pass

    if args.exec_code:
        try:
            val = eval(args.exec_code, nova.locals, nova.locals)
            if val is not None:
                print(_safe_repr(val))
            return 0
        except SyntaxError:
            try:
                exec(args.exec_code, nova.locals, nova.locals)
                return 0
            except Exception:
                traceback.print_exc()
                return 1
        except Exception:
            traceback.print_exc()
            return 1

    if args.script:
        try:
            return _run_script(args.script, nova.locals)
        except Exception:
            traceback.print_exc()
            return 1

    console = NovaInteractiveConsole(nova)
    console.interact(banner=_format_banner(), exitmsg="bye")
    return 0


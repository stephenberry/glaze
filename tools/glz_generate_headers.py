#!/usr/bin/env python3
"""Generate Glaze headers from native module files."""

from __future__ import annotations

import argparse
import re
import shlex
import sys
from dataclasses import dataclass, field
from pathlib import Path


# Match complete C++ literals, comments, preprocessing numbers and directives so
# that module syntax inside them passes through untouched.
CPP_TOKENS = re.compile(
    r"""
    ^[ \t]*//[ \t]*glz:header\b(?P<metadata>[^\n]*)\n?
  | ^[ \t]*(?:export[ \t]+)?module\b[ \t]*(?P<module>[\w.:]*)[ \t]*;
  | ^[ \t]*(?:export[ \t]+)?import[ \t]+(?P<import_name>[^;\n]+?)[ \t]*;
  | ^[ \t]*\#(?:[^\\\n]|\\[\s\S])*  # include continuation lines
  | //[^\n]* | /\*[\s\S]*?(?:\*/|\Z)
  | (?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"
  | "(?:\\[\s\S]|[^"\\])*" | '(?:\\[\s\S]|[^'\\])*'
  | (?:\b[0-9]|\.[0-9])(?:[eEpP][+-]|[\w.]|'[\w])*
  | \b(?P<export>export)\b[ \t]?
    """,
    re.MULTILINE | re.VERBOSE,
)


class HeaderGenerationError(RuntimeError):
    pass


@dataclass
class HeaderMetadata:
    path: str | None = None
    std: list[str] = field(default_factory=list)
    includes: list[str] = field(default_factory=list)
    project_imports: str = "include"
    skip: str | None = None


@dataclass
class Module:
    name: str
    source_path: Path
    metadata: HeaderMetadata
    source: str
    directives: list[re.Match]


@dataclass
class GeneratedHeader:
    header_path: Path
    content: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-list", type=Path, help="Write the generated header paths, one per line")
    parser.add_argument("--module-root", type=Path, default=Path("modules"), help="Root directory containing .ixx files")
    parser.add_argument("--include-root", type=Path, default=Path("include"), help="Root directory for generated headers")
    parser.add_argument(
        "--module",
        action="append",
        default=[],
        help="Generate only a module name or source path. Can be passed more than once.",
    )
    parser.add_argument("--check", action="store_true", help="Fail if generated headers differ from disk")
    parser.add_argument("--dry-run", action="store_true", help="Print generated header paths without writing")
    parser.add_argument(
        "--allow-missing-metadata",
        action="store_true",
        help="Ignore .ixx files without glz:header metadata instead of failing",
    )
    parser.add_argument(
        "--no-copy-headers",
        action="store_true",
        help="Do not copy existing .hpp/.h support headers from module-root to include-root",
    )
    return parser.parse_args()


def parse_metadata(source_path: Path, source: str, directives: list[re.Match]) -> HeaderMetadata | None:
    metadata = HeaderMetadata()
    annotations = [match for match in directives if match.lastgroup == "metadata"]
    if not annotations:
        return None

    for match in annotations:
        line_number = source.count("\n", 0, match.start()) + 1
        body = match["metadata"]
        try:
            tokens = shlex.split(body, posix=True)
        except ValueError as exc:
            raise HeaderGenerationError(f"{source_path}:{line_number}: invalid glz:header metadata: {exc}") from exc
        for token in tokens:
            if "=" not in token:
                raise HeaderGenerationError(f"{source_path}:{line_number}: expected key=value metadata, got {token!r}")
            key, value = token.split("=", 1)
            if key == "path":
                metadata.path = value
            elif key == "std":
                metadata.std.append(normalize_std_include(value))
            elif key in {"include", "extra_include", "extra_includes"}:
                metadata.includes.append(normalize_project_include(value))
            elif key == "project_imports":
                if value not in {"include", "ignore"}:
                    raise HeaderGenerationError(
                        f"{source_path}:{line_number}: project_imports must be 'include' or 'ignore'"
                    )
                metadata.project_imports = value
            elif key == "skip":
                metadata.skip = value or "true"
            else:
                raise HeaderGenerationError(f"{source_path}:{line_number}: unknown glz:header key {key!r}")

    if metadata.skip is not None:
        return metadata
    if metadata.path is None:
        raise HeaderGenerationError(f"{source_path}: glz:header metadata must include path=...")
    return metadata


def normalize_std_include(value: str) -> str:
    value = value.strip()
    if value.startswith("<") and value.endswith(">"):
        return value
    return f"<{value}>"


def normalize_project_include(value: str) -> str:
    value = value.strip()
    if (value.startswith('"') and value.endswith('"')) or (value.startswith("<") and value.endswith(">")):
        return value
    return f'"{value}"'


def read_module(source_path: Path, allow_missing_metadata: bool) -> Module | None:
    source = source_path.read_text(encoding="utf-8")
    directives = [
        match for match in CPP_TOKENS.finditer(source)
        if match.lastgroup in {"module", "metadata", "import_name", "export"}
    ]
    metadata = parse_metadata(source_path, source, directives)
    if metadata is None:
        if allow_missing_metadata:
            return None
        raise HeaderGenerationError(f"{source_path}: missing glz:header metadata")
    if metadata.skip is not None:
        return None
    for match in directives:
        name = match["module"]
        if name and name != ":private":
            return Module(name, source_path, metadata, source, directives)
    raise HeaderGenerationError(f"{source_path}: missing named module declaration")


def transform_source(module: Module, header_paths: dict[str, str]) -> str:
    parts = []
    offset = 0
    preamble_written = False
    for match in module.directives:
        parts.append(module.source[offset:match.start()])
        offset = match.end()
        if match.lastgroup == "module":
            if not preamble_written:
                parts.append("#pragma once\n")
                parts.extend(f"#include {header}\n" for header in dict.fromkeys(module.metadata.std))
                preamble_written = True
            if match["module"] == module.name:
                parts.extend(f"#include {header}\n" for header in dict.fromkeys(module.metadata.includes))
        elif match.lastgroup == "import_name":
            name = match["import_name"].strip()
            if name == "std":
                if not module.metadata.std:
                    raise HeaderGenerationError(f"{module.source_path}: import std; requires glz:header std=...")
            elif module.metadata.project_imports == "include":
                if name.startswith(":"):
                    name = module.name.partition(":")[0] + name
                if name not in header_paths:
                    raise HeaderGenerationError(f"{module.source_path}: unknown module import {name!r}")
                parts.append(f'#include "{header_paths[name]}"')
        # Metadata comments and declaration-level export keywords are removed.
    parts.append(module.source[offset:])
    return "".join(parts)


def generate_headers(args: argparse.Namespace) -> list[GeneratedHeader]:
    modules = []
    header_paths = {}
    generated_paths = {}
    selected = set(args.module)
    for source_path in sorted(args.module_root.rglob("*.ixx")):
        module = read_module(source_path, args.allow_missing_metadata)
        if module is None:
            continue
        if module.name in header_paths:
            raise HeaderGenerationError(f"{source_path}: duplicate module declaration {module.name!r}")
        header_path = args.include_root / module.metadata.path
        if header_path in generated_paths:
            raise HeaderGenerationError(
                f"{source_path}: duplicate generated header path {header_path} also used by {generated_paths[header_path]}"
            )
        header_paths[module.name] = module.metadata.path
        generated_paths[header_path] = source_path
        if matches_selection(module.name, source_path, header_path, selected):
            modules.append((module, header_path))

    generated = [
        GeneratedHeader(path, transform_source(module, header_paths))
        for module, path in modules
    ]
    if not args.no_copy_headers:
        generated.extend(copy_support_headers(args.module_root, args.include_root, generated_paths, selected))
    return generated


def copy_support_headers(
    source_root: Path, include_root: Path, generated_paths: dict[Path, Path], selected: set[str]
) -> list[GeneratedHeader]:
    copied: list[GeneratedHeader] = []
    for source_path in sorted(list(source_root.rglob("*.hpp")) + list(source_root.rglob("*.h"))):
        header_path = include_root / source_path.relative_to(source_root)
        if header_path in generated_paths:
            continue
        if not matches_selection(None, source_path, header_path, selected):
            continue
        header = GeneratedHeader(
            header_path=header_path,
            content=source_path.read_text(encoding="utf-8"),
        )
        copied.append(header)
    return copied


def matches_selection(name: str | None, source: Path, destination: Path, selected: set[str]) -> bool:
    owner = name.partition(":")[0] if name else None
    return not selected or any(
        item in (name, owner) or item in source.as_posix() or item in destination.as_posix() for item in selected
    )


def write_or_check(headers: list[GeneratedHeader], check: bool, dry_run: bool) -> int:
    changed: list[GeneratedHeader] = []
    for header in headers:
        existing = header.header_path.read_text(encoding="utf-8") if header.header_path.exists() else None
        if existing != header.content:
            changed.append(header)
        if dry_run:
            status = "would update" if existing != header.content else "up to date"
            print(f"{status}: {header.header_path}")
        elif not check and existing != header.content:
            header.header_path.parent.mkdir(parents=True, exist_ok=True)
            with header.header_path.open("w", encoding="utf-8", newline="\n") as output:
                output.write(header.content)
            print(f"generated: {header.header_path}")

    if check and changed:
        for header in changed:
            print(f"out of date: {header.header_path}", file=sys.stderr)
        return 1
    if check:
        print(f"checked {len(headers)} header(s)")
    elif not dry_run:
        print(f"generated {len(headers)} header(s)")
    return 0


def main() -> int:
    args = parse_args()
    try:
        headers = generate_headers(args)
        if not headers:
            print("no module files with glz:header metadata found", file=sys.stderr)
            return 1
        result = write_or_check(headers, check=args.check, dry_run=args.dry_run)
        if result == 0 and args.output_list:
            args.output_list.write_text("".join(f"{h.header_path.as_posix()}\n" for h in headers), encoding="utf-8")
        return result
    except HeaderGenerationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

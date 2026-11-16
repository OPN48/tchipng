"""定位随包安装的动态库和命令行。"""

from __future__ import annotations

import sys
from importlib.resources import files
from pathlib import Path

LIBRARY_FILENAMES: dict[str, str] = {
    "darwin": "libtchipng.dylib",
    "linux": "libtchipng.so",
    "win32": "tchipng.dll",
}
CLI_FILENAME: str = "_tchipng_cli"


class TchiPngError(RuntimeError):
    """tchipng 调用失败。"""


def library_filename() -> str:
    """返回当前系统的动态库文件名。

    步骤：
    1. 按 sys.platform 查表。
    2. 没有对应系统就失败。
    """
    name: str | None = LIBRARY_FILENAMES.get(sys.platform)
    if name is None:
        raise TchiPngError(f"当前系统没有 tchipng 动态库: {sys.platform}")
    return name


def artifact_path(name: str) -> Path:
    """在包目录里查找随安装放进来的文件。

    步骤：
    1. 先看本文件旁边，对应普通安装。
    2. 再遍历包的 __path__，编辑安装可能有多处。
    3. 最后试 importlib.resources。
    """
    local = Path(__file__).resolve().parent / name
    if local.is_file():
        return local
    package = sys.modules.get("tchipng")
    entries: tuple[str, ...] = ()
    if package is not None:
        entries = tuple(getattr(package, "__path__", ()))
    for entry in entries:
        candidate = Path(entry) / name
        if candidate.is_file():
            return candidate
    resource = files("tchipng").joinpath(name)
    located = Path(str(resource))
    if located.is_file():
        return located
    raise TchiPngError(f"缺少 {name}，请重新执行 pip install -e native/tchipng")

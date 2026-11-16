"""把命令行参数交给包内的 C 程序。"""

from __future__ import annotations

import os
import sys

from tchipng._paths import CLI_FILENAME, TchiPngError, artifact_path


def main() -> None:
    """用包内的 C 命令行处理当前参数。

    步骤：
    1. 定位 _tchipng_cli。
    2. 用它替换当前进程，PNG 和 JPEG 行为与直接运行 C 程序一致。
    """
    try:
        binary = artifact_path(CLI_FILENAME)
    except TchiPngError as exc:
        raise SystemExit(str(exc)) from exc
    os.execv(str(binary), [str(binary), *sys.argv[1:]])

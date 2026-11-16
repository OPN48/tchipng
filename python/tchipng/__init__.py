"""调用 tchipng 压缩图片，或直接压缩、量化 RGB。"""

from __future__ import annotations

import ctypes
import subprocess
from collections.abc import Buffer
from pathlib import Path

from tchipng._paths import CLI_FILENAME, TchiPngError, artifact_path, library_filename

TCHIPNG_OK: int = 0
TCHIPNG_ERR_ARG: int = -1
TCHIPNG_ERR_NOMEM: int = -2
TCHIPNG_ERR_ENCODE: int = -3
_ERROR_TEXT: dict[int, str] = {
    TCHIPNG_ERR_ARG: "参数无效",
    TCHIPNG_ERR_NOMEM: "内存不足",
    TCHIPNG_ERR_ENCODE: "编码失败",
}

JPEG_QUALITY: int = 90
_JPEG_SUFFIXES: set[str] = {".jpg", ".jpeg"}
_LIBRARY: ctypes.CDLL | None = None

__all__ = ["TchiPngError", "compress", "compress_rgb", "quantize_rgb"]


def _library() -> ctypes.CDLL:
    """装载包内动态库并声明参数类型。

    步骤：
    1. 已装载则直接返回。
    2. 按平台文件名定位动态库。
    3. 声明压缩、量化和释放函数。
    """
    global _LIBRARY
    if _LIBRARY is not None:
        return _LIBRARY
    path = artifact_path(library_filename())
    library = ctypes.CDLL(str(path))
    rgb_args = [
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),
        ctypes.POINTER(ctypes.c_size_t),
    ]
    library.tchipng_compress_rgb.argtypes = rgb_args
    library.tchipng_compress_rgb.restype = ctypes.c_int
    library.tchipng_quantize_rgb.argtypes = rgb_args
    library.tchipng_quantize_rgb.restype = ctypes.c_int
    library.tchipng_free.argtypes = [ctypes.c_void_p]
    library.tchipng_free.restype = None
    _LIBRARY = library
    return library


def _rgb_view(rgb: Buffer) -> memoryview:
    """把缓冲区收成一维字节视图。

    步骤：
    1. 已经是连续的一维字节就直接用。
    2. 否则摊成 B；摊不平就先复制。
    """
    view = memoryview(rgb)
    if view.format == "B" and view.ndim == 1 and view.c_contiguous:
        return view
    try:
        return view.cast("B")
    except (TypeError, ValueError):
        return memoryview(bytes(view))


def _c_pointer(view: memoryview) -> tuple[ctypes.Array[ctypes.c_uint8], ctypes._Pointer[ctypes.c_uint8]]:
    """得到在调用期间保持有效的 C 指针。

    步骤：
    1. 只读或非连续缓冲区先复制。
    2. 连续可写缓冲区直接引用。
    """
    if view.readonly or not view.c_contiguous:
        holder = (ctypes.c_uint8 * view.nbytes).from_buffer_copy(view)
    else:
        holder = (ctypes.c_uint8 * view.nbytes).from_buffer(view)
    pointer = ctypes.cast(holder, ctypes.POINTER(ctypes.c_uint8))
    return holder, pointer


def _invoke(function_name: str, rgb: Buffer, width: int, height: int, stride: int) -> bytes:
    """调用 C 接口并拷回输出字节。

    步骤：
    1. 核对宽高、行距和缓冲区长度。
    2. 调用压缩或量化。
    3. 拷贝输出并释放 C 侧内存。
    """
    if width <= 0 or height <= 0 or stride < width * 3:
        raise TchiPngError(_ERROR_TEXT[TCHIPNG_ERR_ARG])
    view = _rgb_view(rgb)
    needed = stride * (height - 1) + width * 3
    if view.nbytes < needed:
        raise TchiPngError(_ERROR_TEXT[TCHIPNG_ERR_ARG])
    holder, pointer = _c_pointer(view)
    output = ctypes.POINTER(ctypes.c_uint8)()
    size = ctypes.c_size_t()
    library = _library()
    function = getattr(library, function_name)
    status: int = function(pointer, width, height, stride, ctypes.byref(output), ctypes.byref(size))
    del holder
    if status != TCHIPNG_OK:
        if output:
            library.tchipng_free(output)
        raise TchiPngError(_ERROR_TEXT.get(status, f"tchipng 返回 {status}"))
    try:
        return ctypes.string_at(output, size.value)
    finally:
        library.tchipng_free(output)


def compress(source: str | Path, destination: str | Path, quality: int = JPEG_QUALITY) -> None:
    """把图片压到目标路径。

    步骤：
    1. 核对质量。输出是 JPEG 时才把质量传给命令行。
    2. 调用包内的 C 命令行。
    3. 非零退出码视为失败。
    """
    if quality < 1 or quality > 100:
        raise TchiPngError(_ERROR_TEXT[TCHIPNG_ERR_ARG])
    source_path = Path(source)
    dest_path = Path(destination)
    command: list[str] = [str(artifact_path(CLI_FILENAME))]
    if dest_path.suffix.lower() in _JPEG_SUFFIXES:
        command.extend(["-q", str(quality)])
    command.extend([str(source_path), str(dest_path)])
    result = subprocess.run(command, check=False, capture_output=True)
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise TchiPngError(detail or f"tchipng 退出码 {result.returncode}")


def compress_rgb(rgb: Buffer, width: int, height: int, stride: int) -> bytes:
    """把行优先 RGB 压成索引 PNG 字节。

    步骤：
    1. 交给 tchipng_compress_rgb。
    2. 返回完整 PNG。
    """
    return _invoke("tchipng_compress_rgb", rgb, width, height, stride)


def quantize_rgb(rgb: Buffer, width: int, height: int, stride: int) -> bytes:
    """把行优先 RGB 量化成最多 256 色的紧凑 RGB。

    步骤：
    1. 交给 tchipng_quantize_rgb。
    2. 返回宽乘高乘 3 的字节。
    """
    return _invoke("tchipng_quantize_rgb", rgb, width, height, stride)

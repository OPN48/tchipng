# tchipng

PNG with human-visible color compression by TchiTchi.

tchipng 把行优先 RGB 收成最多 256 个原图里真实出现过的颜色，再写成索引 PNG。输出是 JPEG 时，先做同样的量化，再按质量编码。命令行和 Python 调用的都是同一份 C 核心。

## 快速上手

发布到 PyPI 之后：

```bash
pip install tchipng
```

当前仓库内：

```bash
pip install .
```

```python
import tchipng

tchipng.compress("input.png", "output.png")
tchipng.compress("input.png", "output.jpg", quality=90)
```

原图 768×1280，约 1.5MB（1,455,017 字节）。tchipng 索引 PNG 约 443KB（442,532 字节）。下面两张图缩到 180px 宽，只作对照。

| 原图 | tchipng |
| --- | --- |
| ![原图](examples/testimage001.png) | ![tchipng](examples/testimage001_tchipng.png) |

许可证是 [Apache License 2.0](LICENSE)。`third_party/` 里的 stb 头文件是 public domain，只给命令行读写 PNG 和 JPEG。

## 布局

| 路径 | 内容 |
| --- | --- |
| `tchipng.h`、`tchipng.c` | 压缩与量化 |
| `tchipng_cli.c` | 命令行 |
| `python/tchipng/` | `import tchipng` 和 `tchipng` 命令入口 |
| `android/tchipng_jni.c` | Android JNI 绑定，默认 CMake 不编译它 |
| `third_party/` | stb 图像读写 |

`build/`、`build-python/`、`build-android/`、`build-ios/`、`build-harmony/` 是本地编译结果，已写入 `.gitignore`。

## 命令行

依赖 C 编译器、CMake 3.16 或更高版本，以及 zlib。

```bash
cmake -S . -B build
cmake --build build
./build/tchipng input.png output.png
./build/tchipng -q 90 input.png output.jpg
```

用法是 `tchipng [-q quality] <input> <output>`。

- 输入由 stb 解码，按 RGB 读取 PNG 或 JPEG。
- 输出扩展名是 `.png` 时，写出索引 PNG。
- 输出扩展名是 `.jpg` 或 `.jpeg` 时，量化后写 JPEG。`-q` 范围是 1 到 100，省略时为 90。

本机产物：

- `build/tchipng`
- `build/libtchipng.dylib`（Linux 上是 `libtchipng.so`）
- `build/libtchipng.a`

## Python

需要 Python 3.10 或更高版本。通过 pip 安装时还需要 CMake 3.31 或更高版本，因为安装步骤会编译并装入动态库和命令行。

在仓库根目录执行：

```bash
pip install -e .
```

之后可以直接调用已安装的命令，效果与上面的 C 程序相同：

```bash
tchipng input.png output.png
tchipng -q 90 input.png output.jpg
```

`compress` 按输出扩展名选择格式，和命令行相同。已经持有行优先 RGB 时，`stride` 是每行字节数，紧凑排列时为 `width * 3`。

```python
from tchipng import compress_rgb, quantize_rgb

png = compress_rgb(rgb, width, height, width * 3)
quantized = quantize_rgb(rgb, width, height, width * 3)
```

`compress_rgb` 返回完整 PNG 字节。`quantize_rgb` 返回量化后的紧凑 RGB，长度是 `width * height * 3`。参数不合法、内存不足或编码失败时抛出 `TchiPngError`。

改了 C 源码之后，需要再执行一次 `pip install -e .`。编辑安装不会在每次运行时自动重编译。

## C 接口

```c
int tchipng_compress_rgb(const uint8_t *rgb, int width, int height, int stride,
                         uint8_t **out_png, size_t *out_size);
int tchipng_quantize_rgb(const uint8_t *rgb, int width, int height, int stride,
                         uint8_t **out_rgb, size_t *out_size);
void tchipng_free(void *ptr);
```

成功返回 `0`。返回的缓冲区用 `tchipng_free` 释放。

# tchipng

PNG with human-visible color compression by TchiTchi.

tchipng 把行优先 RGB 收成最多 256 个原图里真实出现过的颜色，再写成索引 PNG。输出是 JPEG 时，先做同样的量化，再按质量编码。

## 快速上手

```bash
pip3 install tchipng
```

```python
import tchipng

tchipng.compress("input.png", "output.png")
tchipng.compress("input.png", "output.jpg", quality=90)
```

命令行同样可用：

```bash
tchipng input.png output.png
tchipng -q 90 input.png output.jpg
```

输入是 PNG 或 JPEG。输出扩展名是 `.png` 时写索引 PNG；是 `.jpg` 或 `.jpeg` 时写 JPEG。`-q` 和 `quality` 的范围是 1 到 100，省略时为 90。

原图 768×1280，约 1.5MB（1,455,017 字节）。tchipng 索引 PNG 约 443KB（442,532 字节）。下面两张图缩到 180px 宽，只作对照。

| 原图 | tchipng |
| --- | --- |
| ![原图](examples/testimage001.png) | ![tchipng](examples/testimage001_tchipng.png) |

## Python

已经持有行优先 RGB 时，`stride` 是每行字节数，紧凑排列时为 `width * 3`。

```python
from tchipng import compress_rgb, quantize_rgb

png = compress_rgb(rgb, width, height, width * 3)
quantized = quantize_rgb(rgb, width, height, width * 3)
```

`compress_rgb` 返回完整 PNG 字节。`quantize_rgb` 返回量化后的紧凑 RGB，长度是 `width * height * 3`。参数不合法、内存不足或编码失败时抛出 `TchiPngError`。

## C 接口

```c
int tchipng_compress_rgb(const uint8_t *rgb, int width, int height, int stride,
                         uint8_t **out_png, size_t *out_size);
int tchipng_quantize_rgb(const uint8_t *rgb, int width, int height, int stride,
                         uint8_t **out_rgb, size_t *out_size);
void tchipng_free(void *ptr);
```

成功返回 `0`。返回的缓冲区用 `tchipng_free` 释放。

从源码编译命令行需要 C 编译器、CMake 3.16 或更高版本，以及 zlib：

```bash
cmake -S . -B build
cmake --build build
./build/tchipng input.png output.png
```

许可证是 [Apache License 2.0](LICENSE)。`third_party/` 里的 stb 头文件是 public domain，只给命令行读写 PNG 和 JPEG。

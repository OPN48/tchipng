# tchipng

PNG with human-visible color compression by TchiTchi.

tchipng 把行优先 RGB 收成最多 256 色，再写成索引 PNG。每一色是一组像素的加权平均。输出是 JPEG 时，先做同样的量化，再按质量编码。

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

原图 1080×1446，2,748,179 字节。tchipng 索引 PNG 837,216 字节。下面两张图缩到 180px 宽，只作对照。

| 原图 | tchipng |
| --- | --- |
| ![原图](examples/testimg2.png) | ![tchipng](examples/testimg2_tchipng.png) |

0.1.0 把每个像素收成一种最近色。渐变会变成整段同一颜色，再突然跳到下一种，边缘显得锐利。0.2.0 在两种相近的调色板颜色之间把误差分给邻像素，过渡变成这两种颜色的杂点，局部平均仍接近被删掉的中间色。两种颜色差得远时不做这步混合，轮廓保持切开。

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

## 参考文献

- Heckbert, Paul. 1982. Color Image Quantization for Frame Buffer Display. *SIGGRAPH*. 中位切，按像素误差把颜色分成至多 256 组。
- Floyd, Robert W., and Louis Steinberg. 1976. An Adaptive Algorithm for Spatial Grey Scale. *Proceedings of the Society for Information Display* 17: 75–77. 误差扩散。相近色才扩散、大色差边界停住，是在这套扩散上加的限制。

许可证是 [Apache License 2.0](LICENSE)。`third_party/` 里的 stb 头文件是 public domain，只给命令行读写 PNG 和 JPEG。

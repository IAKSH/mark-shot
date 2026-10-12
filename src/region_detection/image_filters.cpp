#include "region_detection/image_filters.h"

#include <QVector>

#include <array>
#include <cmath>

namespace markshot::region_detection {
namespace {

constexpr float kGaussianSigma = 1.4f;
constexpr int kGaussianRadius = 3;
constexpr int kGaussianSize = 2 * kGaussianRadius + 1;
// tan(22.5°) ≈ 0.414，用来把 Sobel 方向量化成 4 个 45° 档，避免每像素 atan2。
constexpr float kTan22_5 = 0.41421356f;

/**
 * 生成半径 3、sigma 1.4 的一维高斯核。
 * @return 归一化核。
 */
const std::array<float, kGaussianSize> &gaussianKernel()
{
    static const std::array<float, kGaussianSize> kernel = [] {
        std::array<float, kGaussianSize> values{};
        float sum = 0.0f;
        for (int i = -kGaussianRadius; i <= kGaussianRadius; ++i) {
            const float value = std::exp(-(static_cast<float>(i * i)) / (2.0f * kGaussianSigma * kGaussianSigma));
            values[static_cast<size_t>(i + kGaussianRadius)] = value;
            sum += value;
        }
        for (float &value : values) {
            value /= sum;
        }
        return values;
    }();
    return kernel;
}

/**
 * 对灰度图做可分离高斯模糊，结果量化为 8 位。
 * @param image 输入图。
 * @return 模糊后的灰度图。
 */
GrayImage gaussianBlur(const GrayImage &image)
{
    const std::array<float, kGaussianSize> &kernel = gaussianKernel();
    const int width = image.width;
    const int height = image.height;
    QVector<float> horizontal(width * height);
    GrayImage blurred = GrayImage::create(width, height);
    const quint8 *src = image.pixels.constData();
    float *mid = horizontal.data();
    quint8 *dst = blurred.pixels.data();

    // 1. 先沿水平方向卷积；内部像素不越界，只在两端钳制
    for (int y = 0; y < height; ++y) {
        const quint8 *row = src + y * width;
        float *out = mid + y * width;
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            if (x >= kGaussianRadius && x + kGaussianRadius < width) {
                for (int k = -kGaussianRadius; k <= kGaussianRadius; ++k) {
                    sum += static_cast<float>(row[x + k]) * kernel[k + kGaussianRadius];
                }
            } else {
                for (int k = -kGaussianRadius; k <= kGaussianRadius; ++k) {
                    sum += static_cast<float>(row[qBound(0, x + k, width - 1)]) * kernel[k + kGaussianRadius];
                }
            }
            out[x] = sum;
        }
    }

    // 2. 再沿垂直方向卷积并量化
    for (int y = 0; y < height; ++y) {
        quint8 *out = dst + y * width;
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            if (y >= kGaussianRadius && y + kGaussianRadius < height) {
                for (int k = -kGaussianRadius; k <= kGaussianRadius; ++k) {
                    sum += mid[(y + k) * width + x] * kernel[k + kGaussianRadius];
                }
            } else {
                for (int k = -kGaussianRadius; k <= kGaussianRadius; ++k) {
                    sum += mid[qBound(0, y + k, height - 1) * width + x] * kernel[k + kGaussianRadius];
                }
            }
            out[x] = static_cast<quint8>(qBound(0, qRound(sum), 255));
        }
    }
    return blurred;
}

/**
 * 把 Sobel 方向量化成 0/45/90/135 四档。
 * @param gx 水平梯度。
 * @param gy 垂直梯度。
 * @return 方向档。
 */
quint8 quantizedDirection(float gx, float gy)
{
    const float ax = std::fabs(gx);
    const float ay = std::fabs(gy);
    if (ay <= ax * kTan22_5) {
        return 0;
    }
    if (ax <= ay * kTan22_5) {
        return 2;
    }
    return (gx * gy >= 0.0f) ? 1 : 3;
}

} // namespace

GrayImage canny(const GrayImage &image, float lowThreshold, float highThreshold)
{
    GrayImage edges = GrayImage::create(image.width, image.height);
    if (image.width < 3 || image.height < 3) {
        return edges;
    }

    const GrayImage blurred = gaussianBlur(image);
    const int width = image.width;
    const int height = image.height;
    const quint8 *src = blurred.pixels.constData();
    QVector<float> magnitude(width * height, 0.0f);
    QVector<quint8> direction(width * height, 0);
    float *mag = magnitude.data();
    quint8 *dir = direction.data();
    const float lowSq = lowThreshold * lowThreshold;
    const float highSq = highThreshold * highThreshold;

    // 1. Sobel 梯度与方向量化；比较平方幅值，避免 hypot/atan2
    for (int y = 1; y < height - 1; ++y) {
        const quint8 *rowPrev = src + (y - 1) * width;
        const quint8 *row = src + y * width;
        const quint8 *rowNext = src + (y + 1) * width;
        for (int x = 1; x < width - 1; ++x) {
            const float gx = -static_cast<float>(rowPrev[x - 1]) + static_cast<float>(rowPrev[x + 1])
                - 2.0f * static_cast<float>(row[x - 1]) + 2.0f * static_cast<float>(row[x + 1])
                - static_cast<float>(rowNext[x - 1]) + static_cast<float>(rowNext[x + 1]);
            const float gy = -static_cast<float>(rowPrev[x - 1]) - 2.0f * static_cast<float>(rowPrev[x])
                - static_cast<float>(rowPrev[x + 1]) + static_cast<float>(rowNext[x - 1])
                + 2.0f * static_cast<float>(rowNext[x]) + static_cast<float>(rowNext[x + 1]);
            mag[y * width + x] = gx * gx + gy * gy;
            dir[y * width + x] = quantizedDirection(gx, gy);
        }
    }

    // 2. 非极大值抑制
    QVector<quint8> classified(width * height, 0);
    quint8 *cls = classified.data();
    QVector<int> stack;
    stack.reserve(width + height);
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            const int index = y * width + x;
            const float value = mag[index];
            float neighborA = 0.0f;
            float neighborB = 0.0f;
            switch (dir[index]) {
            case 0:
                neighborA = mag[index - 1];
                neighborB = mag[index + 1];
                break;
            case 1:
                neighborA = mag[(y - 1) * width + (x + 1)];
                neighborB = mag[(y + 1) * width + (x - 1)];
                break;
            case 2:
                neighborA = mag[(y - 1) * width + x];
                neighborB = mag[(y + 1) * width + x];
                break;
            default:
                neighborA = mag[(y - 1) * width + (x - 1)];
                neighborB = mag[(y + 1) * width + (x + 1)];
                break;
            }
            if (value <= neighborA || value < neighborB) {
                continue;
            }
            if (value >= highSq) {
                cls[index] = 255;
                stack.append(index);
            } else if (value >= lowSq) {
                cls[index] = 128;
            }
        }
    }

    // 3. 双阈值与滞后连接
    quint8 *out = edges.pixels.data();
    while (!stack.isEmpty()) {
        const int index = stack.takeLast();
        out[index] = 255;
        const int x = index % width;
        const int y = index / width;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                const int nx = x + dx;
                const int ny = y + dy;
                if (nx <= 0 || ny <= 0 || nx >= width - 1 || ny >= height - 1) {
                    continue;
                }
                const int neighbor = ny * width + nx;
                if (cls[neighbor] == 128) {
                    cls[neighbor] = 255;
                    stack.append(neighbor);
                }
            }
        }
    }
    return edges;
}

GrayImage dilateLInf1(const GrayImage &image)
{
    GrayImage output = GrayImage::create(image.width, image.height);
    const int width = image.width;
    const int height = image.height;
    const quint8 *src = image.pixels.constData();
    quint8 *dst = output.pixels.data();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            quint8 value = 0;
            const int y0 = qMax(0, y - 1);
            const int y1 = qMin(height - 1, y + 1);
            const int x0 = qMax(0, x - 1);
            const int x1 = qMin(width - 1, x + 1);
            for (int ny = y0; ny <= y1 && value == 0; ++ny) {
                const quint8 *row = src + ny * width;
                for (int nx = x0; nx <= x1; ++nx) {
                    value = qMax(value, row[nx]);
                }
            }
            dst[y * width + x] = value;
        }
    }
    return output;
}

GrayImage erodeLInf1(const GrayImage &image)
{
    GrayImage output = GrayImage::create(image.width, image.height);
    const int width = image.width;
    const int height = image.height;
    const quint8 *src = image.pixels.constData();
    quint8 *dst = output.pixels.data();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            quint8 value = 255;
            const int y0 = qMax(0, y - 1);
            const int y1 = qMin(height - 1, y + 1);
            const int x0 = qMax(0, x - 1);
            const int x1 = qMin(width - 1, x + 1);
            for (int ny = y0; ny <= y1 && value != 0; ++ny) {
                const quint8 *row = src + ny * width;
                for (int nx = x0; nx <= x1; ++nx) {
                    value = qMin(value, row[nx]);
                }
            }
            dst[y * width + x] = value;
        }
    }
    return output;
}

GrayImage closeLInf1(const GrayImage &image)
{
    return erodeLInf1(dilateLInf1(image));
}

} // namespace markshot::region_detection

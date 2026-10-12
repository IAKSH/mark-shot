#include "region_detection/image_gray.h"

namespace markshot::region_detection {

QImage normalizedRgbImage(const QImage &image)
{
    if (image.isNull()) {
        return {};
    }
    if (image.format() == QImage::Format_ARGB32 || image.format() == QImage::Format_RGB32) {
        return image;
    }
    return image.convertToFormat(QImage::Format_ARGB32);
}

GrayImage grayscaleFromRgba(const QImage &image)
{
    const QImage source = normalizedRgbImage(image);
    GrayImage gray = GrayImage::create(source.width(), source.height());
    if (gray.width == 0 || gray.height == 0) {
        return gray;
    }

    // 1. 使用 image 库相同的 BT.601 定点权重，避免和参考实现出现阈值偏差
    quint8 *dst = gray.pixels.data();
    for (int y = 0; y < gray.height; ++y) {
        const QRgb *row = reinterpret_cast<const QRgb *>(source.constScanLine(y));
        for (int x = 0; x < gray.width; ++x) {
            const int r = qRed(row[x]);
            const int g = qGreen(row[x]);
            const int b = qBlue(row[x]);
            dst[y * gray.width + x] = static_cast<quint8>((19595 * r + 38470 * g + 7471 * b) >> 16);
        }
    }
    return gray;
}

} // namespace markshot::region_detection

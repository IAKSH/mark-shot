#include "region_detection/region_detection_internal.h"

#include "region_detection/image_filters.h"

#include <utility>

namespace markshot::region_detection {
namespace {

/**
 * 把足够长的弱梯度线段提升为方向边缘。
 * @param gradient 主方向梯度。
 * @param other 正交方向梯度。
 * @param vertical 是否沿垂直方向扫描。
 * @return 弱线段掩码。
 */
GrayImage weakLines(const GrayImage &gradient, const GrayImage &other, bool vertical)
{
    GrayImage mask = GrayImage::create(gradient.width, gradient.height);
    const int across = vertical ? gradient.width : gradient.height;
    const int along = vertical ? gradient.height : gradient.width;
    for (int fixed = 0; fixed < across; ++fixed) {
        int start = -1;
        for (int position = 0; position <= along; ++position) {
            const int x = vertical ? fixed : position;
            const int y = vertical ? position : fixed;
            const bool hit = position < along
                && gradient.at(x, y) >= 5
                && static_cast<int>(gradient.at(x, y)) * 4 >= static_cast<int>(other.at(x, y)) * 3;
            if (hit) {
                if (start < 0) {
                    start = position;
                }
            } else if (start >= 0 && position - start >= 24) {
                for (int alongPos = start; alongPos < position; ++alongPos) {
                    const int px = vertical ? fixed : alongPos;
                    const int py = vertical ? alongPos : fixed;
                    mask.set(px, py, 255);
                }
                start = -1;
            } else {
                start = -1;
            }
        }
    }
    return mask;
}

} // namespace

EdgeField buildEdgeField(const QImage &image)
{
    const int width = image.width();
    const int height = image.height();
    EdgeField field;
    field.vertical = GrayImage::create(width, height);
    field.horizontal = GrayImage::create(width, height);
    quint32 histogram[256] = {};

    quint8 *vertical = field.vertical.pixels.data();
    quint8 *horizontal = field.horizontal.pixels.data();
    for (int y = 1; y < height - 1; ++y) {
        const QRgb *prev = reinterpret_cast<const QRgb *>(image.constScanLine(y - 1));
        const QRgb *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        const QRgb *next = reinterpret_cast<const QRgb *>(image.constScanLine(y + 1));
        quint8 *gxRow = vertical + y * width;
        quint8 *gyRow = horizontal + y * width;
        for (int x = 1; x < width - 1; ++x) {
            const QRgb center = row[x];
            const quint8 gx = qMax(colorDifference(row[x - 1], center), colorDifference(center, row[x + 1]));
            const quint8 gy = qMax(colorDifference(prev[x], center), colorDifference(center, next[x]));
            gxRow[x] = gx;
            gyRow[x] = gy;
            const quint8 mag = qMax(gx, gy);
            if (mag > 2) {
                histogram[mag] += 1;
            }
        }
    }

    // 1. 取中低分位并设上下界：保留弱边框，也不让少量亮文字把全局阈值抬高
    quint32 count = 0;
    for (quint32 value : histogram) {
        count += value;
    }
    quint32 accumulated = 0;
    quint8 percentile = 12;
    for (int value = 0; value < 256; ++value) {
        accumulated += histogram[value];
        if (accumulated > count * 35 / 100) {
            percentile = static_cast<quint8>(value);
            break;
        }
    }
    field.threshold = qBound(quint8(5), static_cast<quint8>(percentile / 2), quint8(18));

    const GrayImage weakVertical = weakLines(field.vertical, field.horizontal, true);
    const GrayImage weakHorizontal = weakLines(field.horizontal, field.vertical, false);
    const float high = qBound(15.0f, static_cast<float>(field.threshold) * 3.0f, 54.0f);
    const GrayImage gray = grayscaleFromRgba(image);
    const GrayImage cannyEdges = canny(gray, high * 0.4f, high);

    const quint8 *gxSrc = field.vertical.pixels.constData();
    const quint8 *gySrc = field.horizontal.pixels.constData();
    const quint8 *weakV = weakVertical.pixels.constData();
    const quint8 *weakH = weakHorizontal.pixels.constData();
    auto rawGx = [&](int x, int y) -> quint8 {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return 0;
        }
        return gxSrc[y * width + x];
    };
    auto rawGy = [&](int x, int y) -> quint8 {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return 0;
        }
        return gySrc[y * width + x];
    };

    // 2. 方向色差边做非极大值抑制，避免 2 像素台阶把控件外接框撑厚
    GrayImage binaryVertical = GrayImage::create(width, height);
    GrayImage binaryHorizontal = GrayImage::create(width, height);
    GrayImage colorMask = GrayImage::create(width, height);
    quint8 *binV = binaryVertical.pixels.data();
    quint8 *binH = binaryHorizontal.pixels.data();
    quint8 *mask = colorMask.pixels.data();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = y * width + x;
            const quint8 gx = gxSrc[index];
            const quint8 gy = gySrc[index];
            const bool verticalPeak = gx >= rawGx(x - 1, y) && gx > rawGx(x + 1, y);
            const bool horizontalPeak = gy >= rawGy(x, y - 1) && gy > rawGy(x, y + 1);
            const bool verticalHit = verticalPeak
                && ((gx >= field.threshold && static_cast<int>(gx) * 4 >= static_cast<int>(gy) * 3)
                    || weakV[index] != 0);
            const bool horizontalHit = horizontalPeak
                && ((gy >= field.threshold && static_cast<int>(gy) * 4 >= static_cast<int>(gx) * 3)
                    || weakH[index] != 0);
            binV[index] = verticalHit ? 255 : 0;
            binH[index] = horizontalHit ? 255 : 0;
            if (verticalHit || horizontalHit) {
                mask[index] = 255;
            }
        }
    }

    // 3. Canny 只填补远离色差边的空隙，避免在强边外侧形成光晕
    const GrayImage dilatedColor = dilateLInf1(dilateLInf1(colorMask));
    field.combined = colorMask;
    const quint8 *cannySrc = cannyEdges.pixels.constData();
    const quint8 *dilated = dilatedColor.pixels.constData();
    quint8 *combined = field.combined.pixels.data();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = y * width + x;
            if (cannySrc[index] != 0 && dilated[index] == 0
                && qMax(gxSrc[index], gySrc[index]) < field.threshold) {
                combined[index] = 255;
            }
        }
    }
    field.vertical = std::move(binaryVertical);
    field.horizontal = std::move(binaryHorizontal);
    return field;
}

} // namespace markshot::region_detection

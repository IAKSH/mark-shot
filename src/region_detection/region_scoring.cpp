#include "region_detection/region_detection_internal.h"

#include <utility>

namespace markshot::region_detection {
namespace {

/**
 * 计算单条边上的连续支持度。
 * @param edges 方向边缘图。
 * @param fixed 固定坐标。
 * @param start 起点。
 * @param end 终点。
 * @param vertical 是否为垂直边。
 * @return 支持度。
 */
float sideSupport(const GrayImage &edges, int fixed, int start, int end, bool vertical)
{
    const int limit = vertical ? edges.width : edges.height;
    if (fixed <= 1 || fixed + 2 >= limit) {
        return 0.85f;
    }
    const int margin = qMin((end - start) / 10, 12);
    start += margin;
    end -= margin;
    if (end <= start) {
        return 0.0f;
    }
    const int step = qMax(1, (end - start) / 160);
    float best = 0.0f;
    const int acrossMax = qMin(fixed + 2, limit - 1);
    for (int across = qMax(0, fixed - 2); across <= acrossMax; ++across) {
        int supported = 0;
        int samples = 0;
        for (int along = start; along < end; along += step) {
            const bool hit = vertical ? edges.at(across, along) != 0 : edges.at(along, across) != 0;
            supported += hit ? 1 : 0;
            samples += 1;
        }
        best = qMax(best, static_cast<float>(supported) / static_cast<float>(qMax(1, samples)));
    }
    return best;
}

/**
 * 在原图上精修单条边。
 * @param image 原图。
 * @param original 原始坐标。
 * @param start 另一方向起点。
 * @param end 另一方向终点。
 * @param vertical 是否为垂直边。
 * @param radius 搜索半径。
 * @param threshold 色差阈值。
 * @return 精修坐标。
 */
int refineSide(const QImage &image,
               int original,
               int start,
               int end,
               bool vertical,
               int radius,
               quint8 threshold)
{
    const int limit = vertical ? image.width() : image.height();
    if (original <= 1 || original >= limit - 1) {
        return original;
    }
    const int margin = qBound(1, (end - start) / 10, 24);
    start = qMax(0, start + margin);
    const int alongLimit = vertical ? image.height() : image.width();
    end = qMin(alongLimit, end - margin);
    if (end <= start) {
        return original;
    }
    const int step = qMax(1, (end - start) / 128);
    int best = original;
    float bestScore = 0.0f;
    const int from = qMax(1, original - radius);
    const int to = qMin(limit - 1, original + radius);
    for (int position = from; position <= to; ++position) {
        int count = 0;
        float strength = 0.0f;
        int samples = 0;
        for (int along = start; along < end; along += step) {
            const QRgb a = vertical
                ? pixelAt(image, position - 1, along)
                : pixelAt(image, along, position - 1);
            const QRgb b = vertical
                ? pixelAt(image, position, along)
                : pixelAt(image, along, position);
            const quint8 diff = colorDifference(a, b);
            count += diff >= qMin(threshold, quint8(8)) ? 1 : 0;
            strength += static_cast<float>(qMin(diff, quint8(48))) / 48.0f;
            samples += 1;
        }
        const float coverage = static_cast<float>(count) / static_cast<float>(qMax(1, samples));
        const float score = coverage * 0.85f + strength / static_cast<float>(qMax(1, samples)) * 0.15f
            - static_cast<float>(qAbs(position - original)) * 0.008f;
        if (coverage >= 0.55f && score > bestScore) {
            bestScore = score;
            best = position;
        }
    }
    return best;
}

/**
 * 判断两个矩形是否足够接近，应合并为同一候选。
 * @param a 矩形 A。
 * @param b 矩形 B。
 * @param tolerance 边界容差。
 * @return 应合并时返回 true。
 */
bool nearlySameRect(QRect a, QRect b, int tolerance)
{
    const qint64 areaA = rectArea(a);
    const qint64 areaB = rectArea(b);
    return qAbs(a.x() - b.x()) <= tolerance
        && qAbs(a.y() - b.y()) <= tolerance
        && qAbs(a.x() + a.width() - (b.x() + b.width())) <= tolerance
        && qAbs(a.y() + a.height() - (b.y() + b.height())) <= tolerance
        && static_cast<double>(qMin(areaA, areaB)) / static_cast<double>(qMax(areaA, areaB)) > 0.8;
}

} // namespace

std::array<float, 4> borderSides(const EdgeField &field, AnalysisRect rect)
{
    return {
        sideSupport(field.vertical, rect.left, rect.top, rect.bottom, true),
        sideSupport(field.horizontal, rect.top, rect.left, rect.right, false),
        sideSupport(field.vertical, rect.right - 1, rect.top, rect.bottom, true),
        sideSupport(field.horizontal, rect.bottom - 1, rect.left, rect.right, false),
    };
}

float interiorDensity(const GrayImage &edges, AnalysisRect rect)
{
    const int inset = 3;
    if (rect.width() <= inset * 2 || rect.height() <= inset * 2) {
        return 0.0f;
    }
    int count = 0;
    int samples = 0;
    const int yStep = qMax(1, rect.height() / 32);
    const int xStep = qMax(1, rect.width() / 32);
    for (int y = rect.top + inset; y < rect.bottom - inset; y += yStep) {
        for (int x = rect.left + inset; x < rect.right - inset; x += xStep) {
            count += edges.at(x, y) != 0 ? 1 : 0;
            samples += 1;
        }
    }
    return static_cast<float>(count) / static_cast<float>(qMax(1, samples));
}

float insetSupport(const EdgeField &field, AnalysisRect rect)
{
    if (rect.width() < 12 || rect.height() < 12) {
        return 0.0f;
    }
    const AnalysisRect inner{rect.left + 4, rect.top + 4, rect.right - 4, rect.bottom - 4};
    const std::array<float, 4> sides = borderSides(field, inner);
    return (sides[0] + sides[1] + sides[2] + sides[3]) / 4.0f;
}

QRect scaleRect(AnalysisRect rect, int analysisWidth, int analysisHeight, int width, int height)
{
    auto convert = [](int value, int from, int to) {
        return static_cast<int>((static_cast<qint64>(value) * to + from / 2) / from);
    };
    const int left = convert(rect.left, analysisWidth, width);
    const int top = convert(rect.top, analysisHeight, height);
    const int right = convert(rect.right, analysisWidth, width);
    const int bottom = convert(rect.bottom, analysisHeight, height);
    return QRect(left, top, qMax(0, right - left), qMax(0, bottom - top));
}

QRect refineRect(const QImage &image, QRect rect, int radius, quint8 threshold)
{
    const int left = refineSide(image, rect.x(), rect.y(), rect.y() + rect.height(), true, radius, threshold);
    const int right = refineSide(image, rect.x() + rect.width(), rect.y(), rect.y() + rect.height(), true, radius, threshold);
    const int top = refineSide(image, rect.y(), left, right, false, radius, threshold);
    const int bottom = refineSide(image, rect.y() + rect.height(), left, right, false, radius, threshold);
    if (right - left < 8 || bottom - top < 8) {
        return rect;
    }
    QRect refined(left, top, right - left, bottom - top);
    return refined.intersected(QRect(0, 0, image.width(), image.height()));
}

void mergeCandidate(QVector<RegionCandidate> *candidates, RegionCandidate candidate, int tolerance)
{
    for (RegionCandidate &existing : *candidates) {
        if (!nearlySameRect(existing.rect, candidate.rect, tolerance)) {
            continue;
        }
        QStringList sources = existing.sources;
        for (const QString &source : candidate.sources) {
            if (!sources.contains(source)) {
                sources.append(source);
            }
        }
        if (candidate.quality > existing.quality) {
            existing = std::move(candidate);
        }
        existing.sources = sources;
        return;
    }
    candidates->append(std::move(candidate));
}

} // namespace markshot::region_detection

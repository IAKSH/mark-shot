#include "region_detection/region_detection_internal.h"

#include "region_detection/image_filters.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace markshot::region_detection {

QSize analysisSize(int width, int height)
{
    const int longest = qMax(width, height);
    if (longest <= kMaxAnalysisDimension) {
        return {width, height};
    }
    const double scale = static_cast<double>(kMaxAnalysisDimension) / static_cast<double>(longest);
    return {
        qMax(3, qRound(width * scale)),
        qMax(3, qRound(height * scale)),
    };
}

/**
 * 对截图执行一次区域检测。
 * @param image 冻结截图。
 * @return 已评分的候选。
 */
QVector<RegionCandidate> detectRegions(const QImage &image)
{
    const QImage source = normalizedRgbImage(image);
    const int width = source.width();
    const int height = source.height();
    if (width < 3 || height < 3) {
        return {};
    }

    const QSize analysis = analysisSize(width, height);
    QImage analysisImage = source;
    if (analysis.width() != width || analysis.height() != height) {
        analysisImage = source.scaled(analysis, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (analysisImage.format() != QImage::Format_ARGB32 && analysisImage.format() != QImage::Format_RGB32) {
            analysisImage = analysisImage.convertToFormat(QImage::Format_ARGB32);
        }
    }

    const EdgeField field = buildEdgeField(analysisImage);
    const GrayImage closed = closeLInf1(field.combined);
    QVector<Proposal> proposals = contourProposals(closed);
    proposals += componentProposals(closed, false);
    proposals += lineProposals(field);

    // 1. 边缘围住的背景区域提供面板/分隔布局线索
    const GrayImage barriers = dilateLInf1(field.combined);
    GrayImage background = GrayImage::create(analysis.width(), analysis.height());
    for (int y = 0; y < background.height; ++y) {
        for (int x = 0; x < background.width; ++x) {
            background.set(x, y, barriers.at(x, y) == 0 ? 255 : 0);
        }
    }
    proposals += componentProposals(background, true);

    const float scale = static_cast<float>(width) / static_cast<float>(analysis.width());
    const int minWidth = qMax(5, static_cast<int>(std::ceil(12.0f / scale)));
    const int minHeight = qMax(5, static_cast<int>(std::ceil(12.0f / scale)));
    const qint64 fullArea = static_cast<qint64>(analysis.width()) * analysis.height();
    proposals.erase(std::remove_if(proposals.begin(), proposals.end(),
                                   [&](const Proposal &proposal) {
                                       return proposal.rect.width() < minWidth
                                           || proposal.rect.height() < minHeight
                                           || proposal.rect.area() >= fullArea * 995 / 1000;
                                   }),
                    proposals.end());

    // 2. 小控件与大面板各保留一部分预算
    std::sort(proposals.begin(), proposals.end(),
              [](const Proposal &a, const Proposal &b) { return a.rect.area() < b.rect.area(); });
    if (proposals.size() > kMaxProposals) {
        const int keep = kMaxProposals / 2;
        QVector<Proposal> trimmed;
        trimmed.reserve(kMaxProposals);
        trimmed += proposals.mid(0, keep);
        trimmed += proposals.mid(proposals.size() - keep);
        proposals = std::move(trimmed);
    }

    QVector<RegionCandidate> candidates;
    const int mergeTolerance = static_cast<int>(std::ceil(scale)) + 1;
    for (const Proposal &proposal : proposals) {
        const AnalysisRect rect = proposal.rect;
        const std::array<float, 4> sides = borderSides(field, rect);
        std::array<float, 4> sorted = sides;
        std::sort(sorted.begin(), sorted.end());
        const float support = (sides[0] + sides[1] + sides[2] + sides[3]) / 4.0f;
        if (sorted[1] < 0.48f || sorted[0] < 0.18f || support < 0.58f) {
            continue;
        }
        if (QLatin1String(proposal.source) == QLatin1String("lines")
            && (sorted[0] < 0.70f || support < 0.85f)) {
            continue;
        }
        const float density = interiorDensity(field.combined, rect);
        const float inset = insetSupport(field, rect);
        const bool compact = qMin(rect.width(), rect.height()) * scale < 32.0f;
        if (compact && (density > 0.30f || (density > 0.12f && inset > 0.60f))) {
            continue;
        }
        const float texturePenalty = compact ? density * 0.30f + inset * 0.18f : density * 0.08f;
        RegionCandidate candidate;
        candidate.rect = scaleRect(rect, analysis.width(), analysis.height(), width, height);
        candidate.quality = qBound(0.0f, support * 0.65f + sorted[0] * 0.25f + 0.10f - texturePenalty, 1.0f);
        candidate.sides = sides;
        candidate.interiorEdges = density;
        candidate.sources.append(QString::fromLatin1(proposal.source));
        mergeCandidate(&candidates, std::move(candidate), mergeTolerance);
    }

    // 3. 只精修有限个高质量候选
    std::sort(candidates.begin(), candidates.end(),
              [](const RegionCandidate &a, const RegionCandidate &b) {
                  return a.quality > b.quality;
              });
    if (candidates.size() > kMaxRefined) {
        candidates.resize(kMaxRefined);
    }
    const int radius = qBound(3, static_cast<int>(std::ceil(scale)) * 3, 16);
    for (RegionCandidate &candidate : candidates) {
        candidate.rect = refineRect(source, candidate.rect, radius, field.threshold);
    }
    QVector<RegionCandidate> refined;
    for (const RegionCandidate &candidate : candidates) {
        mergeCandidate(&refined, candidate, 2);
    }
    return refined;
}

} // namespace markshot::region_detection

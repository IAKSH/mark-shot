#include "region_detection/region_detection_internal.h"

#include "region_detection/image_components.h"

#include <algorithm>

namespace markshot::region_detection {
namespace {

struct Segment {
    int fixed = 0;
    int start = 0;
    int end = 0;

    bool operator==(const Segment &other) const
    {
        return fixed == other.fixed && start == other.start && end == other.end;
    }
};

/**
 * 提取足够长的方向线段。
 * @param edges 方向边缘图。
 * @param vertical 是否沿垂直方向扫描。
 * @return 去重后的长线段。
 */
QVector<Segment> longSegments(const GrayImage &edges, bool vertical)
{
    const int across = vertical ? edges.width : edges.height;
    const int along = vertical ? edges.height : edges.width;
    QVector<Segment> segments;
    for (int fixed = 0; fixed < across; ++fixed) {
        int start = -1;
        int last = 0;
        for (int position = 0; position <= along; ++position) {
            const bool hit = position < along
                && (vertical ? edges.at(fixed, position) != 0 : edges.at(position, fixed) != 0);
            if (hit) {
                if (start < 0) {
                    start = position;
                }
                last = position;
            } else if (start >= 0 && (position == along || position - last > 3)) {
                if (last - start >= 24) {
                    segments.append(Segment{fixed, start, last + 1});
                }
                start = -1;
            }
        }
    }

    std::sort(segments.begin(), segments.end(), [](const Segment &a, const Segment &b) {
        return (a.end - a.start) > (b.end - b.start);
    });
    QVector<Segment> unique;
    for (const Segment &line : segments) {
        bool duplicate = false;
        for (const Segment &other : unique) {
            if (qAbs(line.fixed - other.fixed) <= 2
                && qAbs(line.start - other.start) <= 5
                && qAbs(line.end - other.end) <= 5) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            unique.append(line);
            if (unique.size() >= 192) {
                break;
            }
        }
    }
    return unique;
}

} // namespace

QVector<Proposal> contourProposals(const GrayImage &edges)
{
    QVector<Proposal> proposals;
    const QVector<QRect> rects = findContourRects(edges);
    for (const QRect &rect : rects) {
        if (rect.width() <= 0 || rect.height() <= 0) {
            continue;
        }
        proposals.append(Proposal{
            AnalysisRect{rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height()},
            "contour",
        });
    }
    return proposals;
}

QVector<Proposal> componentProposals(const GrayImage &image, bool background)
{
    const Connectivity connectivity = background ? Connectivity::Four : Connectivity::Eight;
    const QVector<ComponentBounds> bounds = connectedComponents(image, connectivity);
    QVector<Proposal> proposals;
    for (int i = 1; i < bounds.size(); ++i) {
        const ComponentBounds &item = bounds[i];
        if (item.pixels < 24) {
            continue;
        }
        AnalysisRect rect{item.left, item.top, item.right, item.bottom};
        if (background && static_cast<double>(item.pixels) / static_cast<double>(qMax<qint64>(1, rect.area())) < 0.72) {
            continue;
        }
        proposals.append(Proposal{rect, background ? "background" : "component"});
    }
    return proposals;
}

QVector<Proposal> lineProposals(const EdgeField &field)
{
    QVector<Proposal> proposals;
    for (bool vertical : {true, false}) {
        const GrayImage &edges = vertical ? field.vertical : field.horizontal;
        QVector<Segment> segments = longSegments(edges, vertical);
        std::sort(segments.begin(), segments.end(),
                  [](const Segment &a, const Segment &b) { return a.fixed < b.fixed; });
        for (int index = 0; index < segments.size(); ++index) {
            const Segment first = segments[index];
            QVector<Segment> aligned;
            for (int next = index + 1; next < segments.size(); ++next) {
                const Segment second = segments[next];
                const int tolerance = qBound(4, qMin(first.end - first.start, second.end - second.start) / 12, 16);
                if (qAbs(first.fixed - second.fixed) >= 8
                    && qAbs(first.start - second.start) <= tolerance
                    && qAbs(first.end - second.end) <= tolerance) {
                    aligned.append(second);
                }
            }
            QVector<Segment> pairs;
            for (int i = 0; i < qMin(2, aligned.size()); ++i) {
                pairs.append(aligned[i]);
            }
            if (!aligned.isEmpty() && !pairs.contains(aligned.last())) {
                pairs.append(aligned.last());
            }
            for (const Segment &second : pairs) {
                const int tolerance = qBound(4, qMin(first.end - first.start, second.end - second.start) / 12, 16);
                if (qAbs(first.fixed - second.fixed) < 8
                    || qAbs(first.start - second.start) > tolerance
                    || qAbs(first.end - second.end) > tolerance) {
                    continue;
                }
                const int lo = qMin(first.fixed, second.fixed);
                const int hi = qMax(first.fixed, second.fixed) + 1;
                const int start = qMin(first.start, second.start);
                const int end = qMax(first.end, second.end);
                AnalysisRect rect = vertical
                    ? AnalysisRect{lo, start, hi, end}
                    : AnalysisRect{start, lo, end, hi};
                proposals.append(Proposal{rect, "lines"});
            }
        }
    }
    return proposals;
}

} // namespace markshot::region_detection

#include "region_detection/image_components.h"

#include <QVector>

namespace markshot::region_detection {
namespace {

struct UnionFind {
    QVector<int> parent;

    UnionFind()
    {
        parent.reserve(1024);
        parent.append(0);
    }

    int makeLabel()
    {
        const int label = parent.size();
        parent.append(label);
        return label;
    }

    int find(int label)
    {
        int root = label;
        while (parent[root] != root) {
            root = parent[root];
        }
        int current = label;
        while (current != root) {
            const int next = parent[current];
            parent[current] = root;
            current = next;
        }
        return root;
    }

    void unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a != b) {
            parent[b] = a;
        }
    }
};

} // namespace

QVector<ComponentBounds> connectedComponents(const GrayImage &image, Connectivity connectivity)
{
    if (image.width <= 0 || image.height <= 0) {
        return {};
    }

    UnionFind labels;
    QVector<int> map(image.width * image.height, 0);
    const quint8 *src = image.pixels.constData();
    int *labelsMap = map.data();
    const int width = image.width;
    const int height = image.height;

    // 1. 第一遍扫描：给前景像素分配临时标签并合并相邻标签
    for (int y = 0; y < height; ++y) {
        const quint8 *row = src + y * width;
        int *out = labelsMap + y * width;
        for (int x = 0; x < width; ++x) {
            if (row[x] == 0) {
                continue;
            }
            int current = 0;
            if (x > 0 && out[x - 1] != 0) {
                current = out[x - 1];
            }
            if (y > 0) {
                const int *prev = labelsMap + (y - 1) * width;
                auto consider = [&](int label) {
                    if (label == 0) {
                        return;
                    }
                    if (current == 0) {
                        current = label;
                    } else if (current != label) {
                        labels.unite(current, label);
                    }
                };
                consider(prev[x]);
                if (connectivity == Connectivity::Eight) {
                    if (x > 0) {
                        consider(prev[x - 1]);
                    }
                    if (x + 1 < width) {
                        consider(prev[x + 1]);
                    }
                }
            }
            if (current == 0) {
                current = labels.makeLabel();
            }
            out[x] = current;
        }
    }

    // 2. 第二遍扫描：压缩标签并累计外接矩形
    QVector<int> remap(labels.parent.size(), 0);
    int next = 1;
    QVector<ComponentBounds> bounds;
    bounds.reserve(labels.parent.size());
    bounds.append(ComponentBounds{});
    for (int y = 0; y < height; ++y) {
        const int *row = labelsMap + y * width;
        for (int x = 0; x < width; ++x) {
            int label = row[x];
            if (label == 0) {
                continue;
            }
            const int root = labels.find(label);
            if (remap[root] == 0) {
                remap[root] = next++;
                bounds.append(ComponentBounds{x, y, x + 1, y + 1, 0});
            }
            ComponentBounds &item = bounds[remap[root]];
            item.left = qMin(item.left, x);
            item.top = qMin(item.top, y);
            item.right = qMax(item.right, x + 1);
            item.bottom = qMax(item.bottom, y + 1);
            item.pixels += 1;
        }
    }
    return bounds;
}

QVector<QRect> findContourRects(const GrayImage &edges)
{
    QVector<QRect> rects;
    // 闭运算后的边缘是细环。外接框已覆盖空心控件；不再对整图反相做连通域。
    const QVector<ComponentBounds> outer = connectedComponents(edges, Connectivity::Eight);
    for (int i = 1; i < outer.size(); ++i) {
        const ComponentBounds &bounds = outer[i];
        if (bounds.pixels < 16) {
            continue;
        }
        rects.append(QRect(bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top));
    }
    return rects;
}

} // namespace markshot::region_detection

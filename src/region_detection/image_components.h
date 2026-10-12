#pragma once

#include "region_detection/image_gray.h"

#include <QRect>
#include <QVector>

namespace markshot::region_detection {

enum class Connectivity {
    Four,
    Eight,
};

struct ComponentBounds {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    quint32 pixels = 0;
};

/**
 * 对非零像素做连通域标记。
 * @param image 输入图。
 * @param connectivity 四连通或八连通。
 * @return 每个连通域的外接矩形，下标从 1 开始对应标签。
 */
QVector<ComponentBounds> connectedComponents(const GrayImage &image, Connectivity connectivity);

/**
 * 提取闭合边缘连通域的外接矩形。
 * @param edges 二值边缘图。
 * @return 轮廓外接矩形。
 */
QVector<QRect> findContourRects(const GrayImage &edges);

} // namespace markshot::region_detection

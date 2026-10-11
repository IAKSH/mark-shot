#pragma once

#include "annotation/arrow_style.h"

#include <QPointF>
#include <QVector>

class QPainter;

namespace markshot::annotation {

/**
 * 【标注】【箭头绘制】按骨架绘制直线或曲线箭头，线型不影响箭头头部
 * @param painter 当前绘制器，使用其画笔颜色且保留调用前状态
 * @param points 骨架点，前两项是端点，其余项是曲线控制点
 * @param width 线宽，单位与骨架坐标一致
 * @param style 箭头头部样式
 * @param lineStyle 箭身的实线、虚线或点线样式
 * @return 无返回值
 */
void drawArrow(QPainter &painter, const QVector<QPointF> &points, qreal width,
               markshot::shot::types::ArrowStyle style,
               markshot::shot::types::ArrowLineStyle lineStyle);

} // namespace markshot::annotation

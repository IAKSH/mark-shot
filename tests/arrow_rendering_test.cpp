#include "annotation/arrow_rendering.h"
#include "line_skeleton_path.h"

#include <QImage>
#include <QPainter>
#include <QtTest>

using markshot::shot::types::ArrowLineStyle;
using markshot::shot::types::ArrowStyle;

namespace {

/// @brief 渲染指定头部、线型和骨架的箭头
/// @param points 箭头骨架
/// @param style 头部样式
/// @param lineStyle 箭身线型
/// @return 透明背景上的箭头图像
QImage renderArrow(const QVector<QPointF> &points, ArrowStyle style, ArrowLineStyle lineStyle)
{
    QImage image(460, 240, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setPen(QPen(Qt::black, 4));
    markshot::annotation::drawArrow(painter, points, 4, style, lineStyle);
    return image;
}

/// @brief 统计指定区域内实际绘制的像素
/// @param image 渲染图像
/// @param area 比较区域
/// @return 非透明像素数量
int inkPixels(const QImage &image, const QRect &area)
{
    int count = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            count += qAlpha(image.pixel(x, y)) > 64;
        }
    }
    return count;
}

/// @brief 沿箭身中心采样覆盖率，不依赖实心燕尾箭头的渐变宽度
/// @param image 渲染图像
/// @param path 箭头中心路径
/// @return 中段一百个采样点中的非透明点数
int centerLineInk(const QImage &image, const QPainterPath &path)
{
    int count = 0;
    for (int sample = 0; sample < 100; ++sample) {
        const QPoint point = path.pointAtPercent(0.25 + sample * 0.005).toPoint();
        count += qAlpha(image.pixel(point)) > 64;
    }
    return count;
}

} // namespace

/// @brief 验证线型只改变箭身，且兼容曲线、双向头部和绘制器状态
class ArrowRenderingTest final : public QObject {
    Q_OBJECT

private slots:
    /// @brief 组合全部已有头部样式与直线、曲线骨架，无参数和返回值
    void rendersShaftPatterns_data()
    {
        QTest::addColumn<int>("head");
        QTest::addColumn<bool>("curved");
        for (int head = 0; head <= int(ArrowStyle::BidirectionalKde); ++head) {
            for (bool curved : {false, true}) {
                QTest::newRow(qPrintable(QStringLiteral("head-%1-curve-%2").arg(head).arg(curved)))
                    << head << curved;
            }
        }
    }

    /// @brief 比较实线、虚线和点线的覆盖面积，并核对头部与曲线命中，无参数和返回值
    void rendersShaftPatterns()
    {
        QFETCH(int, head);
        QFETCH(bool, curved);
        QVector<QPointF> points{QPointF(30, 70), QPointF(420, 70)};
        if (curved) {
            points.append(QPointF(225, 180));
        }
        const auto style = static_cast<ArrowStyle>(head);
        const QImage solid = renderArrow(points, style, ArrowLineStyle::Solid);
        const QImage dashed = renderArrow(points, style, ArrowLineStyle::Dashed);
        const QImage dotted = renderArrow(points, style, ArrowLineStyle::Dotted);
        const auto path = markshot::shot::lineSkeletonPath(points);
        const int solidInk = centerLineInk(solid, path);
        const int dashedInk = centerLineInk(dashed, path);
        const int dottedInk = centerLineInk(dotted, path);
        QCOMPARE(solidInk, 100);
        QVERIFY(dashedInk > 0 && dashedInk < solidInk);
        QVERIFY(dottedInk > 0 && dottedInk < dashedInk);
        for (const QImage &image : {solid, dashed, dotted}) {
            QVERIFY(inkPixels(image, QRect(416, 66, 8, 8)) > 0);
            if (style == ArrowStyle::BidirectionalFletched || style == ArrowStyle::BidirectionalKde) {
                QVERIFY(inkPixels(image, QRect(26, 66, 8, 8)) > 0);
            }
        }
        QVERIFY(markshot::shot::lineSkeletonContainsPoint(points, path.pointAtPercent(0.5), 5));
    }

    /// @brief 确保箭头渲染不会污染后续标注的画笔、画刷或变换，无参数和返回值
    void preservesPainterState()
    {
        QImage image(460, 240, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setPen(QPen(QColor(20, 30, 40, 128), 6, Qt::DashDotLine));
        painter.setBrush(Qt::blue);
        painter.translate(2, 3);
        const QPen pen = painter.pen();
        const QBrush brush = painter.brush();
        const QTransform transform = painter.transform();
        for (auto style : {ArrowStyle::Fletched, ArrowStyle::Kde, ArrowStyle::BidirectionalFletched,
                           ArrowStyle::BidirectionalKde}) {
            for (auto line : {ArrowLineStyle::Solid, ArrowLineStyle::Dashed, ArrowLineStyle::Dotted}) {
                markshot::annotation::drawArrow(painter, {{30, 70}, {420, 70}}, 4, style, line);
                QCOMPARE(painter.pen(), pen);
                QCOMPARE(painter.brush(), brush);
                QCOMPARE(painter.transform(), transform);
            }
        }
    }

    /// @brief 退化骨架不产生残留像素，无参数和返回值
    void ignoresDegenerateArrows()
    {
        const QImage image = renderArrow({{30, 70}, {30, 70}}, ArrowStyle::Fletched, ArrowLineStyle::Dotted);
        QCOMPARE(inkPixels(image, image.rect()), 0);
    }
};

QTEST_APPLESS_MAIN(ArrowRenderingTest)
#include "arrow_rendering_test.moc"

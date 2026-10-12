#pragma once

#include "region_detection/image_gray.h"
#include "region_detection/region_detector.h"

#include <QImage>
#include <QRect>
#include <QSize>
#include <QVector>

#include <array>

namespace markshot::region_detection {

constexpr int kMaxAnalysisDimension = 1280;
constexpr int kMaxProposals = 4096;
constexpr int kMaxRefined = 512;
constexpr int kMaxCandidates = 512;

struct AnalysisRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    int width() const
    {
        return right - left;
    }
    int height() const
    {
        return bottom - top;
    }
    qint64 area() const
    {
        return static_cast<qint64>(width()) * height();
    }
};

struct Proposal {
    AnalysisRect rect;
    const char *source = "";
};

struct EdgeField {
    GrayImage vertical;
    GrayImage horizontal;
    GrayImage combined;
    quint8 threshold = 0;
};

/**
 * 对截图执行一次区域检测。
 * @param image 冻结截图。
 * @return 已评分的候选。
 */
QVector<RegionCandidate> detectRegions(const QImage &image);

/**
 * 计算分析分辨率。
 * @param width 原图宽度。
 * @param height 原图高度。
 * @return 分析宽高。
 */
QSize analysisSize(int width, int height);

/**
 * 构建方向边缘场。
 * @param image 分析分辨率图像。
 * @return 边缘场。
 */
EdgeField buildEdgeField(const QImage &image);

/**
 * 由闭合边缘生成轮廓提案。
 * @param edges 闭合边缘图。
 * @return 轮廓提案。
 */
QVector<Proposal> contourProposals(const GrayImage &edges);

/**
 * 由连通域生成提案。
 * @param image 二值图。
 * @param background 是否按背景连通域处理。
 * @return 连通域提案。
 */
QVector<Proposal> componentProposals(const GrayImage &image, bool background);

/**
 * 由长直线配对生成提案。
 * @param field 边缘场。
 * @return 直线提案。
 */
QVector<Proposal> lineProposals(const EdgeField &field);

/**
 * 计算矩形四条边的方向支持度。
 * @param field 边缘场。
 * @param rect 分析矩形。
 * @return 左、上、右、下支持度。
 */
std::array<float, 4> borderSides(const EdgeField &field, AnalysisRect rect);

/**
 * 计算矩形内部边缘密度。
 * @param edges 边缘图。
 * @param rect 分析矩形。
 * @return 内部密度。
 */
float interiorDensity(const GrayImage &edges, AnalysisRect rect);

/**
 * 计算内缩一圈后的边界支持度。
 * @param field 边缘场。
 * @param rect 分析矩形。
 * @return 内缩支持度。
 */
float insetSupport(const EdgeField &field, AnalysisRect rect);

/**
 * 把分析矩形映射回原图像素。
 * @param rect 分析矩形。
 * @param analysisWidth 分析宽度。
 * @param analysisHeight 分析高度。
 * @param width 原图宽度。
 * @param height 原图高度。
 * @return 原图矩形。
 */
QRect scaleRect(AnalysisRect rect, int analysisWidth, int analysisHeight, int width, int height);

/**
 * 在原图上精修候选边界。
 * @param image 原图。
 * @param rect 候选矩形。
 * @param radius 搜索半径。
 * @param threshold 色差阈值。
 * @return 精修后的矩形。
 */
QRect refineRect(const QImage &image, QRect rect, int radius, quint8 threshold);

/**
 * 读取 ARGB32 像素。
 * @param image 图像。
 * @param x 列坐标。
 * @param y 行坐标。
 * @return 像素。
 */
inline QRgb pixelAt(const QImage &image, int x, int y)
{
    const QRgb *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
    return row[x];
}

/**
 * 计算两个像素的最大通道色差。
 * @param a 像素 A。
 * @param b 像素 B。
 * @return 色差。
 */
inline quint8 colorDifference(QRgb a, QRgb b)
{
    const int dr = qAbs(qRed(a) - qRed(b));
    const int dg = qAbs(qGreen(a) - qGreen(b));
    const int db = qAbs(qBlue(a) - qBlue(b));
    return static_cast<quint8>(qMax(dr, qMax(dg, db)));
}

} // namespace markshot::region_detection

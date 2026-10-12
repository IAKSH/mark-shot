#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QStringList>
#include <QVector>

#include <array>
#include <optional>

namespace markshot::region_detection {

/**
 * 区域候选：保留检测来源和质量，避免把文字轮廓与完整面板一视同仁。
 */
struct RegionCandidate {
    QRect rect;
    float quality = 0.0f;
    std::array<float, 4> sides = {0.0f, 0.0f, 0.0f, 0.0f};
    float interiorEdges = 0.0f;
    QStringList sources;
};

/**
 * 截图分析只执行一次；悬停与层级切换只查询已缓存的矩形。
 */
class RegionDetector {
public:
    explicit RegionDetector(const QImage &image);
    RegionDetector(int imageWidth, int imageHeight, QVector<RegionCandidate> candidates);

    int imageWidth() const;
    int imageHeight() const;
    const QVector<RegionCandidate> &candidates() const;
    QRect bounds() const;

    /**
     * 把合成器窗口矩形注入候选，供滚轮切换父层级；悬停仍优先 UI 控件。
     * @param windows 已换算到图像坐标的窗口矩形。
     * @return 无返回值。
     */
    void addWindowRects(const QVector<QRect> &windows);

    /**
     * 返回覆盖指定图像坐标的最佳候选；没有命中时回退到整图。
     * @param point 图像像素坐标。
     * @return 候选矩形。
     */
    QRect regionAt(QPoint point) const;

    /**
     * 返回覆盖归一化坐标的最佳候选。
     * @param x 水平归一化坐标，范围 [0, 1]。
     * @param y 垂直归一化坐标，范围 [0, 1]。
     * @return 候选矩形。
     */
    QRect regionAtNormalized(float x, float y) const;

    /**
     * 从小到大返回一条真正包含关系的链。
     * @param point 图像像素坐标。
     * @return 子区域到整图的矩形链。
     */
    QVector<QRect> regionsAt(QPoint point) const;

    /**
     * 把归一化坐标转换成图像像素。
     * @param x 水平归一化坐标。
     * @param y 垂直归一化坐标。
     * @return 图像像素坐标。
     */
    QPoint pointFromNormalized(float x, float y) const;

private:
    int m_imageWidth = 0;
    int m_imageHeight = 0;
    QVector<RegionCandidate> m_candidates;
};

/**
 * 会话中的悬停状态：小幅移动不跳框，滚轮选择不会在单击时被重置。
 */
class SelectionCursor {
public:
    /**
     * 根据指针位置更新当前候选。
     * @param detector 已缓存的区域检测器。
     * @param point 图像像素坐标。
     * @return 当前候选矩形。
     */
    QRect hover(const RegionDetector &detector, QPoint point);

    /**
     * 正方向扩大到父区域，负方向缩小到子区域。
     * @param detector 已缓存的区域检测器。
     * @param point 图像像素坐标。
     * @param direction 正值扩大，负值缩小。
     * @return 切换后的候选矩形。
     */
    QRect step(const RegionDetector &detector, QPoint point, int direction);

    /**
     * 判断当前框是否来自滚轮手动切换。
     * @return 手动层级时返回 true。
     */
    bool manualLevel() const;

    /**
     * 返回当前缓存的矩形。
     * @return 当前矩形；尚未悬停时为空。
     */
    std::optional<QRect> current() const;

    /**
     * 清空悬停状态。
     * @return 无返回值。
     */
    void reset();

private:
    std::optional<QRect> m_current;
    std::optional<QPointF> m_anchor;
    bool m_manualLevel = false;
};

/**
 * 计算矩形面积。
 * @param rect 目标矩形。
 * @return 面积。
 */
qint64 rectArea(QRect rect);

/**
 * 判断父矩形是否完整包含子矩形。
 * @param parent 外层矩形。
 * @param child 内层矩形。
 * @return 完整包含时返回 true。
 */
bool containsRect(QRect parent, QRect child);

/**
 * 合并相近候选。
 * @param candidates 已有候选。
 * @param candidate 新候选。
 * @param tolerance 边界容差，单位像素。
 * @return 无返回值。
 */
void mergeCandidate(QVector<RegionCandidate> *candidates, RegionCandidate candidate, int tolerance);

} // namespace markshot::region_detection

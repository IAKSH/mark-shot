#include "shot_window_module.h"

#include "debug_log.h"

#include <QCursor>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <memory>

/**
 * 【截图】【区域识别】在后台分析冻结截图，生成 UI 级候选区域。
 * @return 无返回值。
 */
void ShotWindow::initializeRegionDetection()
{
    if (m_frozenFrame.isNull() || m_frozenFrame.width() < 3 || m_frozenFrame.height() < 3) {
        return;
    }

    m_regionDetectorWatcher =
        new QFutureWatcher<std::shared_ptr<markshot::region_detection::RegionDetector>>(this);
    connect(m_regionDetectorWatcher,
            &QFutureWatcher<std::shared_ptr<markshot::region_detection::RegionDetector>>::finished,
            this,
            [this] {
                if (!m_regionDetectorWatcher) {
                    return;
                }
                applyRegionDetector(m_regionDetectorWatcher->result());
            });

    // QImage 隐式共享，工作线程只读，不必复制整屏。
    const QImage snapshot = m_frozenFrame;
    m_regionDetectorWatcher->setFuture(QtConcurrent::run([snapshot]() {
        return std::make_shared<markshot::region_detection::RegionDetector>(snapshot);
    }));
}

/**
 * 【截图】【区域识别】接入后台检测结果，并注入窗口矩形。
 * @param detector 后台线程生成的检测器。
 * @return 无返回值。
 */
void ShotWindow::applyRegionDetector(std::shared_ptr<markshot::region_detection::RegionDetector> detector)
{
    if (!detector) {
        return;
    }
    m_regionDetector = std::make_unique<markshot::region_detection::RegionDetector>(std::move(*detector));
    m_regionCursor.reset();

    QVector<QRect> windows;
    windows.reserve(m_windowInfos.size());
    for (const markshot::WindowInfo &info : std::as_const(m_windowInfos)) {
        windows.append(info.rect);
    }
    m_regionDetector->addWindowRects(windows);
    int windowCount = 0;
    for (const auto &candidate : m_regionDetector->candidates()) {
        if (candidate.sources.contains(QStringLiteral("window"))) {
            ++windowCount;
        }
    }
    markshot::debugLog("region-detection",
                       "【截图】【区域识别】候选就绪 count=%d windows=%d size=%dx%d",
                       static_cast<int>(m_regionDetector->candidates().size()),
                       windowCount,
                       m_regionDetector->imageWidth(),
                       m_regionDetector->imageHeight());

    if (m_mode == Mode::Selecting && !m_dragging) {
        const QPoint imagePoint = m_startupHoverValid
            ? m_startupHoverImagePoint.toPoint()
            : widgetToImage(mapFromGlobal(QCursor::pos())).toPoint();
        const QRegion dirty = refreshHoveredRegion(imagePoint);
        if (!dirty.isEmpty()) {
            update(dirty);
        }
    }
}

/**
 * 【截图】【区域识别】查询当前指针下的 UI 或窗口候选。
 * @param imagePoint 图像像素坐标。
 * @return 候选矩形；没有命中时为空。
 */
std::optional<QRect> ShotWindow::hoveredRegionAt(QPoint imagePoint)
{
    if (m_regionDetector) {
        const QRect rect = m_regionCursor.hover(*m_regionDetector, imagePoint);
        if (rect != m_regionDetector->bounds() || m_regionCursor.manualLevel()) {
            return rect;
        }
    }
    return markshot::window_selection::topmostWindowRectAt(m_windowInfos, imagePoint);
}

/**
 * 【截图】【区域识别】更新悬停高亮并返回需要重绘的区域。
 * @param rect 新的悬停矩形。
 * @return 脏区域。
 */
QRegion ShotWindow::applyHoveredRect(const std::optional<QRect> &rect)
{
    if (rect == m_hoveredWindowRect) {
        return {};
    }
    QRegion dirty;
    if (m_hoveredWindowRect.has_value()) {
        dirty |= QRegion(imageRectToWidget(QRectF(*m_hoveredWindowRect)).toAlignedRect());
    }
    m_hoveredWindowRect = rect;
    if (m_hoveredWindowRect.has_value()) {
        dirty |= QRegion(imageRectToWidget(QRectF(*m_hoveredWindowRect)).toAlignedRect());
    }
    return dirty;
}

/**
 * 【截图】【区域识别】按当前指针刷新悬停高亮。
 * @param imagePoint 图像像素坐标。
 * @return 脏区域。
 */
QRegion ShotWindow::refreshHoveredRegion(QPoint imagePoint)
{
    return applyHoveredRect(hoveredRegionAt(imagePoint));
}

/**
 * 【截图】【区域识别】用滚轮在父子区域之间切换。
 * @param imagePoint 图像像素坐标。
 * @param direction 正值扩大到父区域，负值缩小到子区域。
 * @return 已处理时返回 true。
 */
bool ShotWindow::cycleHoveredRegion(QPoint imagePoint, int direction)
{
    if (!m_regionDetector) {
        return false;
    }
    const QRect rect = m_regionCursor.step(*m_regionDetector, imagePoint, direction);
    const QRegion dirty = applyHoveredRect(rect);
    if (!dirty.isEmpty()) {
        update(dirty);
    }
    return true;
}

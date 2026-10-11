#include "shot_window_module.h"

/// @brief 【标注】【箭头样式】修改当前箭头头部，或更新未绘制箭头的默认样式
/// @param style 箭头头部样式
/// @return 无返回值
void ShotWindow::setSelectedAnnotationArrowStyle(ArrowStyle style)
{
    if (m_selectedAnnotationId.has_value()) {
        Annotation *annotation = annotationById(*m_selectedAnnotationId);
        if (!annotation || annotation->tool != Tool::Arrow || annotation->arrowStyle == style) {
            return;
        }
        pushHistorySnapshot();
        annotation = annotationById(*m_selectedAnnotationId);
        annotation->arrowStyle = style;
    } else {
        if (m_tool != Tool::Arrow || m_arrowStyle == style) {
            return;
        }
        m_arrowStyle = style;
    }
    updateAnnotationPropertyPanel();
    update();
    persistAnnotationState();
}

void ShotWindow::setSelectedArrowLineStyle(ArrowLineStyle style)
{
    if (m_selectedAnnotationId.has_value()) {
        Annotation *annotation = annotationById(*m_selectedAnnotationId);
        if (!annotation || annotation->tool != Tool::Arrow || annotation->arrowLineStyle == style) {
            return;
        }
        // 1. 【标注】【箭身线型】修改现有箭头时保留撤销记录，不改变其骨架或头部
        pushHistorySnapshot();
        // 2. 【标注】【撤销隔离】快照会共享 QVector 存储，重新取得可写元素以分离历史数据
        annotation = annotationById(*m_selectedAnnotationId);
        annotation->arrowLineStyle = style;
    } else {
        if (m_tool != Tool::Arrow || m_arrowLineStyle == style) {
            return;
        }
        m_arrowLineStyle = style;
    }
    if (m_draft && m_draft->tool == Tool::Arrow) {
        m_draft->arrowLineStyle = style;
    }
    updateAnnotationPropertyPanel();
    update();
    persistAnnotationState();
}

void ShotWindow::updateArrowPropertyControls(const Annotation *annotation, bool visible)
{
    const auto sync = [visible](QComboBox *combo, int value) {
        if (!combo) {
            return;
        }
        combo->setVisible(visible);
        if (visible) {
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(std::max(0, combo->findData(value)));
        }
    };
    sync(m_propertyArrowStyleCombo, static_cast<int>(annotation ? annotation->arrowStyle : m_arrowStyle));
    sync(m_propertyArrowLineStyleCombo,
         static_cast<int>(annotation ? annotation->arrowLineStyle : m_arrowLineStyle));
}

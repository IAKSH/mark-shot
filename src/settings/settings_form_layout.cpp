#include "settings/settings_form_layout.h"

#include <QAbstractScrollArea>
#include <QCheckBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QLabel>
#include <QScopedValueRollback>
#include <QTextLayout>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace markshot::settings {
namespace {

/// @brief 按可用宽度换行开关文字，同时保留完整的可访问名称
/// @param text 原始开关说明
/// @param font 当前表单字体
/// @param width 扣除指示器后的文本可用宽度
/// @return 适合当前行宽的完整文本
QString wrappedSwitchText(const QString &text, const QFont &font, int width)
{
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    QStringList lines;
    layout.beginLayout();
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(std::max(40, width));
        lines.append(text.mid(line.textStart(), line.textLength()).trimmed());
    }
    layout.endLayout();
    return lines.join(QLatin1Char('\n'));
}

/// @brief 根据实际视区宽度排列设置字段，避免旧最小宽度阻止窄屏换行
class SettingsFormLayout final : public QFormLayout {
public:
    /// @brief 应用标签列宽并计算表单几何
    /// @param rect 父布局分配的表单范围
    /// @return 无返回值
    void setGeometry(const QRect &rect) override
    {
        if (m_arranging || !parentWidget()) {
            QFormLayout::setGeometry(rect);
            return;
        }
        const QScopedValueRollback<bool> arranging(m_arranging, true);

        // 1. 【设置】【表单排布】优先使用滚动视区的真实宽度，窄窗口统一上下排列
        int availableWidth = rect.width();
        for (QWidget *ancestor = parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
            if (auto *scroll = qobject_cast<QAbstractScrollArea *>(ancestor)) {
                availableWidth = std::min(availableWidth, scroll->viewport()->width() - 40);
                break;
            }
        }
        const bool compact = availableWidth < 440;
        const auto wrapPolicy = compact ? QFormLayout::WrapAllRows : QFormLayout::DontWrapRows;
        // 2. 【设置】【空闲布局】Qt 即使收到相同策略也会使布局失效，避免在几何计算中循环排队
        if (rowWrapPolicy() != wrapPolicy) {
            setRowWrapPolicy(wrapPolicy);
        }
        const int labelWidth = parentWidget()->fontMetrics().height() * 7;

        // 3. 【设置】【标签对齐】各分组采用相同标签列，长标签换行且保留控件关联
        for (int row = 0; row < rowCount(); ++row) {
            QLayoutItem *field = itemAt(row, QFormLayout::SpanningRole);
            auto *box = field ? qobject_cast<QCheckBox *>(field->widget()) : nullptr;
            if (box && box->property("settingsSwitchLabel").isValid()) {
                box->setText(wrappedSwitchText(box->property("settingsSwitchLabel").toString(),
                                              box->font(), availableWidth - 32));
            }
            QLayoutItem *item = itemAt(row, QFormLayout::LabelRole);
            auto *label = item ? qobject_cast<QLabel *>(item->widget()) : nullptr;
            if (!label) {
                continue;
            }
            if (!label->wordWrap()) {
                label->setWordWrap(true);
            }
            label->setMinimumWidth(compact ? 0 : labelWidth);
            label->setMaximumWidth(compact ? QWIDGETSIZE_MAX : labelWidth);
        }
        QFormLayout::setGeometry(rect);
    }

private:
    bool m_arranging = false;
};

}

QFormLayout *createSettingsFormLayout(QVBoxLayout *parentLayout)
{
    auto *form = new SettingsFormLayout;
    form->setObjectName(QStringLiteral("settingsCardForm"));
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignTop | Qt::AlignLeft);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    parentLayout->addLayout(form);
    return form;
}

}

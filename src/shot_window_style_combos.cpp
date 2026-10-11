#include "shot_window_module.h"

using namespace markshot::shot;

void ShotWindow::initializePropertyStyleCombos(QHBoxLayout *propertyLayout)
{
    // 1. 矩形风格切换:描边/高亮/反色/聚光灯,仅在 Tool::Rectangle 选中或激活时显示
    m_propertyRectangleStyleCombo = new QComboBox(m_annotationPropertyPanel);
    m_propertyRectangleStyleCombo->setCursor(Qt::ArrowCursor);
    m_propertyRectangleStyleCombo->setFocusPolicy(Qt::NoFocus);
    m_propertyRectangleStyleCombo->addItem(MS_TR("Stroke"), static_cast<int>(RectangleStyle::Stroke));
    m_propertyRectangleStyleCombo->addItem(MS_TR("Highlight"), static_cast<int>(RectangleStyle::Highlight));
    m_propertyRectangleStyleCombo->addItem(MS_TR("Invert"), static_cast<int>(RectangleStyle::Invert));
    m_propertyRectangleStyleCombo->addItem(MS_TR("Spotlight"), static_cast<int>(RectangleStyle::Spotlight));
    m_propertyRectangleStyleCombo->setToolTip(MS_TR("Rectangle style"));
    m_propertyRectangleStyleCombo->setAccessibleName(MS_TR("Rectangle style"));
    connect(m_propertyRectangleStyleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index < 0 || !m_propertyRectangleStyleCombo) {
            return;
        }
        setSelectedRectangleStyle(
            static_cast<RectangleStyle>(m_propertyRectangleStyleCombo->itemData(index).toInt()));
    });
    propertyLayout->addWidget(m_propertyRectangleStyleCombo);
    // 2. 马赛克滤镜切换：像素化/模糊/灰度/反色/提亮，仅在 Tool::Mosaic 选中或激活时显示
    m_propertyMosaicStyleCombo = new QComboBox(m_annotationPropertyPanel);
    m_propertyMosaicStyleCombo->setCursor(Qt::ArrowCursor);
    m_propertyMosaicStyleCombo->setFocusPolicy(Qt::NoFocus);
    m_propertyMosaicStyleCombo->addItem(MS_TR("Pixelate"), static_cast<int>(MosaicStyle::Pixelate));
    m_propertyMosaicStyleCombo->addItem(MS_TR("Blur"), static_cast<int>(MosaicStyle::Blur));
    m_propertyMosaicStyleCombo->addItem(MS_TR("Grayscale"), static_cast<int>(MosaicStyle::Grayscale));
    m_propertyMosaicStyleCombo->addItem(MS_TR("Invert"), static_cast<int>(MosaicStyle::Invert));
    m_propertyMosaicStyleCombo->addItem(MS_TR("Brighten"), static_cast<int>(MosaicStyle::Brighten));
    m_propertyMosaicStyleCombo->setToolTip(MS_TR("Filter effect"));
    m_propertyMosaicStyleCombo->setAccessibleName(MS_TR("Filter effect"));
    connect(m_propertyMosaicStyleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index < 0 || !m_propertyMosaicStyleCombo) {
            return;
        }
        setSelectedMosaicStyle(static_cast<MosaicStyle>(m_propertyMosaicStyleCombo->itemData(index).toInt()));
    });
    propertyLayout->addWidget(m_propertyMosaicStyleCombo);
    // 3. 箭头样式
    m_propertyArrowStyleCombo = new QComboBox(m_annotationPropertyPanel);
    m_propertyArrowStyleCombo->setObjectName(QStringLiteral("annotationArrowStyle"));
    m_propertyArrowStyleCombo->setCursor(Qt::ArrowCursor);
    m_propertyArrowStyleCombo->setFocusPolicy(Qt::NoFocus);
    m_propertyArrowStyleCombo->addItem(MS_TR("Fletched"), static_cast<int>(ArrowStyle::Fletched));
    m_propertyArrowStyleCombo->addItem(MS_TR("KDE"), static_cast<int>(ArrowStyle::Kde));
    m_propertyArrowStyleCombo->addItem(MS_TR("Double fletched"), static_cast<int>(ArrowStyle::BidirectionalFletched));
    m_propertyArrowStyleCombo->addItem(MS_TR("Double KDE"), static_cast<int>(ArrowStyle::BidirectionalKde));
    m_propertyArrowStyleCombo->setToolTip(MS_TR("Arrow style"));
    m_propertyArrowStyleCombo->setAccessibleName(MS_TR("Arrow style"));
    connect(m_propertyArrowStyleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index < 0 || !m_propertyArrowStyleCombo) {
            return;
        }
        setSelectedAnnotationArrowStyle(static_cast<ArrowStyle>(m_propertyArrowStyleCombo->itemData(index).toInt()));
    });
    propertyLayout->addWidget(m_propertyArrowStyleCombo);
    m_propertyArrowLineStyleCombo = new QComboBox(m_annotationPropertyPanel);
    m_propertyArrowLineStyleCombo->setObjectName(QStringLiteral("annotationArrowLineStyle"));
    m_propertyArrowLineStyleCombo->setCursor(Qt::ArrowCursor);
    m_propertyArrowLineStyleCombo->setFocusPolicy(Qt::NoFocus);
    m_propertyArrowLineStyleCombo->addItem(MS_TR("Solid"), static_cast<int>(ArrowLineStyle::Solid));
    m_propertyArrowLineStyleCombo->addItem(MS_TR("Dashed"), static_cast<int>(ArrowLineStyle::Dashed));
    m_propertyArrowLineStyleCombo->addItem(MS_TR("Dotted"), static_cast<int>(ArrowLineStyle::Dotted));
    m_propertyArrowLineStyleCombo->setToolTip(MS_TR("Arrow line style"));
    m_propertyArrowLineStyleCombo->setAccessibleName(MS_TR("Arrow line style"));
    connect(m_propertyArrowLineStyleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index >= 0) {
            setSelectedArrowLineStyle(static_cast<ArrowLineStyle>(m_propertyArrowLineStyleCombo->itemData(index).toInt()));
        }
    });
    propertyLayout->addWidget(m_propertyArrowLineStyleCombo);
    // 4. 荧光笔样式
    m_propertyHighlighterStyleCombo = new QComboBox(m_annotationPropertyPanel);
    m_propertyHighlighterStyleCombo->setCursor(Qt::ArrowCursor);
    m_propertyHighlighterStyleCombo->setFocusPolicy(Qt::NoFocus);
    m_propertyHighlighterStyleCombo->addItem(MS_TR("Pen"), static_cast<int>(HighlighterStyle::Freehand));
    m_propertyHighlighterStyleCombo->addItem(MS_TR("Line"), static_cast<int>(HighlighterStyle::StraightLine));
    m_propertyHighlighterStyleCombo->setToolTip(MS_TR("Highlighter style"));
    m_propertyHighlighterStyleCombo->setAccessibleName(MS_TR("Highlighter style"));
    connect(m_propertyHighlighterStyleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index < 0 || !m_propertyHighlighterStyleCombo) {
            return;
        }
        setSelectedHighlighterStyle(
            static_cast<HighlighterStyle>(m_propertyHighlighterStyleCombo->itemData(index).toInt()));
    });
    propertyLayout->addWidget(m_propertyHighlighterStyleCombo);
    // 5. 序号样式
    m_propertyNumberStyleCombo = new QComboBox(m_annotationPropertyPanel);
    m_propertyNumberStyleCombo->setCursor(Qt::ArrowCursor);
    m_propertyNumberStyleCombo->setFocusPolicy(Qt::NoFocus);
    m_propertyNumberStyleCombo->addItem(MS_TR("1, 2, 3"), static_cast<int>(NumberStyle::Arabic));
    m_propertyNumberStyleCombo->addItem(MS_TR("A, B, C"), static_cast<int>(NumberStyle::UpperAlpha));
    m_propertyNumberStyleCombo->addItem(MS_TR("a, b, c"), static_cast<int>(NumberStyle::LowerAlpha));
    m_propertyNumberStyleCombo->addItem(MS_TR("I, II, III"), static_cast<int>(NumberStyle::UpperRoman));
    m_propertyNumberStyleCombo->addItem(MS_TR("i, ii, iii"), static_cast<int>(NumberStyle::LowerRoman));
    m_propertyNumberStyleCombo->addItem(MS_TR("甲, 乙, 丙"), static_cast<int>(NumberStyle::HeavenlyStem));
    m_propertyNumberStyleCombo->addItem(MS_TR("一, 二, 三"), static_cast<int>(NumberStyle::Chinese));
    m_propertyNumberStyleCombo->setToolTip(MS_TR("Number style"));
    m_propertyNumberStyleCombo->setAccessibleName(MS_TR("Number style"));
    connect(m_propertyNumberStyleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index < 0 || !m_propertyNumberStyleCombo) {
            return;
        }
        setSelectedNumberStyle(static_cast<NumberStyle>(m_propertyNumberStyleCombo->itemData(index).toInt()));
    });
    propertyLayout->addWidget(m_propertyNumberStyleCombo);
}

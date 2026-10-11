#include "scroll/stitcher.h"

#include <QPainter>
#include <QtTest>

namespace {

/// @brief 生成没有短周期重复的文档行颜色
/// @param row 文档中的绝对行号
/// @return 对应行颜色
QColor documentColor(int row)
{
    return QColor((row * 7 + 31) % 251, (row * 11 + 59) % 241, (row * 13 + 83) % 239);
}

/// @brief 生成带固定侧栏和可选顶栏的滚动页面
/// @param offset 主内容滚动位移
/// @param sidebarWidth 固定侧栏宽度
/// @param right 侧栏是否位于右侧
/// @param headerHeight 固定顶栏高度
/// @return 归一化的截图帧
QImage sidebarFrame(int offset, int sidebarWidth, bool right, int headerHeight)
{
    QImage image(800, 600, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    const int contentLeft = right ? 0 : sidebarWidth;
    const int sidebarLeft = right ? image.width() - sidebarWidth : 0;
    for (int y = 0; y < image.height(); ++y) {
        painter.fillRect(contentLeft, y, image.width() - sidebarWidth, 1,
                         documentColor(offset + y - headerHeight));
        painter.fillRect(sidebarLeft, y, sidebarWidth, 1, documentColor(y * 3 + 701));
    }
    if (headerHeight > 0) {
        painter.fillRect(0, 0, image.width(), headerHeight, QColor(210, 220, 230));
    }
    return image;
}

/// @brief 生成固定目录侧栏随章节切换高亮的网页帧
/// @param offset 正文滚动位移
/// @param section 当前高亮的目录项序号
/// @return 1280x800 的截图帧
QImage highlightedSidebarFrame(int offset, int section)
{
    QImage image(1280, 800, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    for (int y = 0; y < image.height(); ++y) {
        painter.fillRect(360, y, 920, 1, documentColor(offset + y));
        painter.fillRect(0, y, 360, 1, (y / 24) % 2 ? QColor(245, 246, 248) : QColor(232, 235, 240));
    }
    // 目录高亮项随章节下移，模拟网页滚动监听
    painter.fillRect(16, 40 + section * 48, 328, 40, QColor(70, 120, 220));
    return image;
}

/// @brief 生成只有中间正文滚动、右侧留白的稀疏文档
/// @param offset 文档滚动位移
/// @param height 图像高度
/// @return 带固定导航及稀疏文本行的截图
QImage sparseDocumentFrame(int offset, int height = 600)
{
    QImage image = sidebarFrame(0, 224, false, 0);
    if (height != image.height()) {
        image = QImage(800, height, QImage::Format_ARGB32_Premultiplied);
    }
    QPainter painter(&image);
    painter.fillRect(224, 0, 576, height, Qt::white);
    for (int y = 0; y < height; ++y) {
        painter.fillRect(0, y, 224, 1, documentColor(y * 3 + 701));
        const int row = offset + y;
        if (row % 28 < 6) {
            const int width = 120 + ((row / 28) * 37) % 180;
            painter.fillRect(250, y, width, 1, QColor(35, 40, 45));
        }
    }
    return image;
}

/// @brief 生成具有相似空白页边、正文含轻微采集噪声的文档帧
/// @param offset 文档位移
/// @param noisy 是否给正文增加一个亮度级别
/// @return 当前可见帧
QImage blankMarginFrame(int offset, bool noisy)
{
    QImage image(160, 600, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    for (int y = 0; y < image.height(); ++y) {
        const int row = offset + y;
        QColor color = Qt::white;
        if (row >= 150 && row < 650) {
            color = documentColor(row);
            if (noisy) {
                color = QColor(color.red() + 1, color.green() + 1, color.blue() + 1);
            }
        }
        painter.fillRect(0, y, image.width(), 1, color);
    }
    return image;
}

} // namespace

/// @brief 验证固定导航侧栏不会使滚动内容停止拼接或截断
class StitcherSidebarTest final : public QObject {
    Q_OBJECT

private slots:
    /// @brief 覆盖不同侧栏位置、占比和浏览器顶栏，无参数和返回值
    void appendsWholeContent_data()
    {
        QTest::addColumn<int>("sidebarWidth");
        QTest::addColumn<bool>("right");
        QTest::addColumn<int>("headerHeight");
        QTest::newRow("left-navigation") << 224 << false << 0;
        QTest::newRow("right-navigation") << 224 << true << 0;
        QTest::newRow("wide-navigation") << 320 << false << 0;
        QTest::newRow("browser-navigation") << 224 << false << 60;
    }

    /// @brief 校验每次位移及导出图所有内容行，防止仅检查成功状态，无参数和返回值
    void appendsWholeContent()
    {
        QFETCH(int, sidebarWidth);
        QFETCH(bool, right);
        QFETCH(int, headerHeight);
        markshot::scroll::Stitcher stitcher(markshot::scroll::defaultConfig());
        QCOMPARE(stitcher.pushFrame(sidebarFrame(0, sidebarWidth, right, headerHeight)).status,
                 markshot::scroll::StitchStatus::FirstFrame);
        for (int offset : {180, 360, 540, 720}) {
            const auto result = stitcher.pushFrame(sidebarFrame(offset, sidebarWidth, right, headerHeight));
            QCOMPARE(result.status, markshot::scroll::StitchStatus::Appended);
            QCOMPARE(result.added, 180);
            QCOMPARE(result.position, offset);
        }
        const QImage full = stitcher.fullImage();
        QCOMPARE(full.size(), QSize(800, 1320));
        const int contentX = right ? 100 : 700;
        for (int y = headerHeight; y < full.height(); ++y) {
            QCOMPARE(full.pixelColor(contentX, y), documentColor(y - headerHeight));
        }

        // 1. 【长截图测试】【重复帧】固定侧栏不能使静止页面继续增长
        const auto repeated = stitcher.pushFrame(sidebarFrame(720, sidebarWidth, right, headerHeight));
        QCOMPARE(repeated.status, markshot::scroll::StitchStatus::NoProgress);
        QCOMPARE(stitcher.fullImage(), full);
    }

    /// @brief 倒退或返回已知内容时不重复追加，重新越过末端后继续追加，无参数和返回值
    void revisitsKnownContent()
    {
        markshot::scroll::Stitcher stitcher(markshot::scroll::defaultConfig());
        for (int offset : {0, 180, 360}) {
            stitcher.pushFrame(sidebarFrame(offset, 224, false, 60));
        }
        const QImage before = stitcher.fullImage();
        for (int offset : {180, 0, 180, 360}) {
            const auto result = stitcher.pushFrame(sidebarFrame(offset, 224, false, 60));
            QCOMPARE(result.status, markshot::scroll::StitchStatus::NoProgress);
            QCOMPARE(result.position, offset);
            QCOMPARE(stitcher.fullImage(), before);
        }
        const auto result = stitcher.pushFrame(sidebarFrame(540, 224, false, 60));
        QCOMPARE(result.status, markshot::scroll::StitchStatus::Appended);
        QCOMPARE(result.added, 180);
    }

    /// @brief 反向滚动时保留完整正文且只在顶部保留一份固定顶栏，无参数和返回值
    void prependsPastFixedNavigation()
    {
        markshot::scroll::Stitcher stitcher(markshot::scroll::defaultConfig());
        stitcher.pushFrame(sidebarFrame(360, 224, false, 60));
        for (int offset : {180, 0}) {
            const auto result = stitcher.pushFrame(sidebarFrame(offset, 224, false, 60));
            QCOMPARE(result.status, markshot::scroll::StitchStatus::Appended);
            QCOMPARE(result.edge, markshot::scroll::StitchEdge::Start);
            QCOMPARE(result.added, 180);
        }
        const QImage full = stitcher.fullImage();
        QCOMPARE(full.height(), 960);
        for (int y = 60; y < full.height(); ++y) {
            QCOMPARE(full.pixelColor(700, y), documentColor(y - 60));
        }
    }

    /// @brief 侧栏目录随滚动切换高亮、单次滚动超过预测窗口时仍按真实位移拼接，无参数和返回值
    void followsLargeStepsWithSidebarHighlight()
    {
        markshot::scroll::Stitcher stitcher(markshot::scroll::defaultConfig());
        int offset = 0;
        stitcher.pushFrame(highlightedSidebarFrame(offset, 0));
        const int steps[] = {240, 410, 180, 460, 300, 520};
        for (int i = 0; i < 6; ++i) {
            offset += steps[i];
            const auto result = stitcher.pushFrame(highlightedSidebarFrame(offset, i + 1));
            QCOMPARE(result.status, markshot::scroll::StitchStatus::Appended);
            QCOMPARE(result.position, offset);
        }
        const QImage full = stitcher.fullImage();
        QCOMPARE(full.height(), offset + 800);
        for (int y = 0; y < full.height(); y += 7) {
            QCOMPARE(full.pixelColor(900, y), documentColor(y));
        }
    }

    /// @brief 横向截图复用相同的运动区域和位移验证，无参数和返回值
    void supportsHorizontalCapture()
    {
        markshot::scroll::Stitcher stitcher(markshot::scroll::defaultConfig());
        stitcher.setAxis(markshot::scroll::ScrollAxis::Horizontal);
        const QTransform transpose(0, 1, 1, 0, 0, 0);
        stitcher.pushFrame(sidebarFrame(0, 224, false, 0).transformed(transpose));
        const auto result = stitcher.pushFrame(sidebarFrame(181, 224, false, 0).transformed(transpose));
        QCOMPARE(result.status, markshot::scroll::StitchStatus::Appended);
        QCOMPARE(result.added, 181);
        QCOMPARE(stitcher.fullImage().size(), QSize(781, 800));
        QCOMPARE(stitcher.fullImage().pixelColor(780, 700), documentColor(780));
    }

    /// @brief 稀疏正文不会被静止侧栏或空白边栏误判为未滚动，无参数和返回值
    void stitchesSparseDocuments()
    {
        markshot::scroll::Stitcher stitcher(markshot::scroll::defaultConfig());
        stitcher.pushFrame(sparseDocumentFrame(0));
        for (int offset : {137, 274, 411}) {
            const auto result = stitcher.pushFrame(sparseDocumentFrame(offset));
            QCOMPARE(result.status, markshot::scroll::StitchStatus::Appended);
            QCOMPARE(result.added, 137);
        }
        const QImage full = stitcher.fullImage();
        QCOMPARE(full.height(), 1011);
        const QRect content(224, 0, 576, full.height());
        QCOMPARE(full.copy(content), sparseDocumentFrame(0, 1011).copy(content));
    }

    /// @brief 排除固定侧栏后仍拒绝没有正文重叠的页面，无参数和返回值
    void rejectsUnrelatedContent()
    {
        markshot::scroll::Stitcher stitcher(markshot::scroll::defaultConfig());
        const QImage first = sidebarFrame(0, 224, false, 0);
        stitcher.pushFrame(first);
        QImage unrelated = first;
        {
            QPainter painter(&unrelated);
            painter.fillRect(224, 0, 576, 600, Qt::black);
        }
        QCOMPARE(stitcher.pushFrame(unrelated).status, markshot::scroll::StitchStatus::NoMatch);
        QCOMPARE(stitcher.fullImage(), first);
    }

    /// @brief 相同空白页边不能击败具有轻微采集误差的真实正文重叠，无参数和返回值
    void prefersTextOverlapToBlankMargins()
    {
        markshot::scroll::Stitcher stitcher(markshot::scroll::defaultConfig());
        stitcher.pushFrame(blankMarginFrame(0, false));
        const auto result = stitcher.pushFrame(blankMarginFrame(200, true));
        QCOMPARE(result.status, markshot::scroll::StitchStatus::Appended);
        QCOMPARE(result.edge, markshot::scroll::StitchEdge::End);
        QCOMPARE(result.added, 200);
        QCOMPARE(stitcher.fullImage().height(), 800);
    }

    /// @brief 完全空白的静止帧只报告没有进展，不推断虚假的位移，无参数和返回值
    void keepsBlankFramesIdle()
    {
        markshot::scroll::Stitcher stitcher(markshot::scroll::defaultConfig());
        QImage frame(160, 600, QImage::Format_ARGB32_Premultiplied);
        frame.fill(Qt::white);
        stitcher.pushFrame(frame);
        QCOMPARE(stitcher.pushFrame(frame.copy()).status, markshot::scroll::StitchStatus::NoProgress);
        QCOMPARE(stitcher.stats().frameCount, 1);
        QCOMPARE(stitcher.fullImage(), frame);
    }
};

QTEST_APPLESS_MAIN(StitcherSidebarTest)
#include "stitcher_sidebar_test.moc"

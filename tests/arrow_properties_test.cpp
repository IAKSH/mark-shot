#include "annotation_state_store.h"
#include "shot_window.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

namespace {

/// @brief 通过实际工具栏选择标注工具
/// @param window 标注窗口
/// @param action 工具动作名称
/// @return 是否找到并触发工具按钮
bool chooseTool(ShotWindow &window, const char *action)
{
    for (QPushButton *button : window.findChildren<QPushButton *>()) {
        if (button->property("action").toString() == QLatin1String(action)) {
            button->click();
            return true;
        }
    }
    return false;
}

/// @brief 触发属性下拉框的用户选择信号
/// @param combo 实际线型控件
/// @param style 目标线型
/// @return 控件是否成功切换并发送信号
bool activateStyle(QComboBox *combo, ShotWindow::ArrowLineStyle style)
{
    const int index = combo->findData(static_cast<int>(style));
    if (index < 0) {
        return false;
    }
    combo->setCurrentIndex(index);
    return QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection, Q_ARG(int, index));
}

/// @brief 向实际画布发送带完整按键状态的绘制事件
/// @param window 标注窗口
/// @param type 事件类型
/// @param position 窗口坐标
/// @param button 本次变化的鼠标按键
/// @param buttons 事件后的按键状态
/// @return 无返回值
void pointerEvent(ShotWindow &window, QEvent::Type type, QPoint position,
                  Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(position), QPointF(window.mapToGlobal(position)),
                      button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &event);
}

} // namespace

/// @brief 检查箭身线型从实际控件到标注历史及磁盘配置的完整链路
class ArrowPropertiesTest final : public QObject {
    Q_OBJECT

private slots:
    /// @brief 每项测试恢复默认工具样式，无参数和返回值
    void init()
    {
        QVERIFY(markshot::saveAnnotationState(markshot::AnnotationState{}));
    }

    /// @brief 通过实际控件修改默认值，并核对重启后的恢复，无参数和返回值
    void persistsToolDefault()
    {
        QImage image(640, 400, QImage::Format_RGB32);
        image.fill(Qt::white);
        {
            ShotWindow window(image, {}, {}, {}, false);
            window.setAttribute(Qt::WA_DeleteOnClose, false);
            window.startFullscreenAnnotation();
            window.resize(960, 700);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QVERIFY(chooseTool(window, "Arrow"));
            auto *combo = window.findChild<QComboBox *>(QStringLiteral("annotationArrowLineStyle"));
            QVERIFY(combo && combo->isVisible());
            QCOMPARE(combo->currentData().toInt(), int(ShotWindow::ArrowLineStyle::Solid));
            QVERIFY(activateStyle(combo, ShotWindow::ArrowLineStyle::Dotted));
            window.close();
        }
        QCOMPARE(markshot::loadAnnotationState().arrowLineStyle, ShotWindow::ArrowLineStyle::Dotted);
        ShotWindow reopened(image, {}, {}, {}, false);
        reopened.startFullscreenAnnotation();
        QVERIFY(chooseTool(reopened, "Arrow"));
        auto *restored = reopened.findChild<QComboBox *>(QStringLiteral("annotationArrowLineStyle"));
        QVERIFY(restored);
        QCOMPARE(restored->currentData().toInt(), int(ShotWindow::ArrowLineStyle::Dotted));
    }

    /// @brief 修改已绘制箭头的线型后支持撤销和重做，无参数和返回值
    void recordsExistingArrowChanges()
    {
        QImage image(640, 400, QImage::Format_RGB32);
        image.fill(Qt::white);
        ShotWindow window(image, {}, {}, {}, false);
        window.setAttribute(Qt::WA_DeleteOnClose, false);
        window.startFullscreenAnnotation();
        window.resize(960, 700);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(chooseTool(window, "Arrow"));
        pointerEvent(window, QEvent::MouseButtonPress, {250, 300}, Qt::LeftButton, Qt::LeftButton);
        pointerEvent(window, QEvent::MouseMove, {600, 300}, Qt::NoButton, Qt::LeftButton);
        pointerEvent(window, QEvent::MouseButtonRelease, {600, 300}, Qt::LeftButton, Qt::NoButton);
        QVERIFY(chooseTool(window, "Select"));
        pointerEvent(window, QEvent::MouseButtonPress, {425, 300}, Qt::LeftButton, Qt::LeftButton);
        pointerEvent(window, QEvent::MouseButtonRelease, {425, 300}, Qt::LeftButton, Qt::NoButton);
        auto *combo = window.findChild<QComboBox *>(QStringLiteral("annotationArrowLineStyle"));
        QVERIFY(combo && combo->isVisible());
        QVERIFY(activateStyle(combo, ShotWindow::ArrowLineStyle::Dashed));
        QTest::keyClick(&window, Qt::Key_Z, Qt::ControlModifier);
        QTRY_COMPARE(combo->currentData().toInt(), int(ShotWindow::ArrowLineStyle::Solid));
        QTest::keyClick(&window, Qt::Key_Y, Qt::ControlModifier);
        QTRY_COMPARE(combo->currentData().toInt(), int(ShotWindow::ArrowLineStyle::Dashed));
    }

    /// @brief 缺失或损坏的线型配置保持旧版实线默认行为，无参数和返回值
    void acceptsLegacyAndInvalidState()
    {
        for (const QByteArray &json : {QByteArray("{}"), QByteArray("{\"arrowLineStyle\":-1}"),
                                       QByteArray("{\"arrowLineStyle\":99}"), QByteArray("{\"arrowLineStyle\":\"dotted\"}")}) {
            QFile file(markshot::annotationStateFilePath());
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(json), json.size());
            file.close();
            QCOMPARE(markshot::loadAnnotationState().arrowLineStyle, ShotWindow::ArrowLineStyle::Solid);
        }
    }
};

/// @brief 在临时配置目录运行箭头属性测试
/// @param argc 参数数量
/// @param argv 参数内容
/// @return 测试退出码
int main(int argc, char **argv)
{
    QTemporaryDir isolated;
    if (!isolated.isValid()) {
        return 1;
    }
    for (const char *name : {"XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_DATA_HOME"}) {
        const QString path = isolated.filePath(QString::fromLatin1(name));
        QDir().mkpath(path);
        qputenv(name, path.toUtf8());
    }
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("mark-shot-arrow-test"));
    ArrowPropertiesTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "arrow_properties_test.moc"

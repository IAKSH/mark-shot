#include "settings/settings_dialog.h"
#include "settings/settings_design_tokens.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QTemporaryDir>
#include <QtTest>

namespace {

/// @brief 统计目标窗口及其子控件的实际绘制事件
class PaintCounter final : public QObject {
public:
    /// @brief 记录待观测窗口，无返回值
    /// @param window 实际设置窗口
    explicit PaintCounter(QWidget *window) : m_window(window) {}

    int paints = 0;
    int updates = 0;

protected:
    /// @brief 只计数而不消费事件，保持真实事件循环不变
    /// @param watched 事件目标
    /// @param event 待处理事件
    /// @return 始终返回 false
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget->window() == m_window) {
            paints += event->type() == QEvent::Paint;
            updates += event->type() == QEvent::UpdateRequest;
        }
        return false;
    }

private:
    QWidget *m_window;
};

} // namespace

/// @brief 检查完整设置窗口在无输入时停止绘制，修改字段后仍能更新
class SettingsIdleRepaintTest final : public QObject {
    Q_OBJECT

private slots:
    /// @brief 覆盖常规与紧凑窗口尺寸，无参数和返回值
    void staysIdle_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<bool>("light");
        QTest::newRow("desktop-dark") << 860 << false;
        QTest::newRow("compact-dark") << 480 << false;
        QTest::newRow("desktop-light") << 860 << true;
        QTest::newRow("compact-light") << 480 << true;
    }

    /// @brief 统计稳定后的空闲绘制次数，同时验证交互与隐藏状态，无参数和返回值
    void staysIdle()
    {
        QFETCH(int, width);
        QFETCH(bool, light);
        markshot::settings::SettingsDialog dialog;
        const auto theme = light ? markshot::ui::UiThemeMode::Light : markshot::ui::UiThemeMode::Dark;
        qApp->setPalette(markshot::settings::tokens::settingsPalette(theme));
        dialog.setStyleSheet(markshot::settings::tokens::settingsStyleSheet(theme));
        dialog.resize(width, 620);
        PaintCounter counter(&dialog);
        qApp->installEventFilter(&counter);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));

        // 1. 【设置测试】【空闲重绘】首轮布局完成后不应持续提交绘制请求
        QTest::qWait(250);
        counter.paints = 0;
        counter.updates = 0;
        QTest::qWait(250);
        QVERIFY2(counter.updates <= 2,
                 qPrintable(QStringLiteral("Idle update requests: %1, paint events: %2")
                                .arg(counter.updates).arg(counter.paints)));
        QVERIFY(counter.paints <= 4);

        // 2. 【设置测试】【字段刷新】不能通过禁用窗口更新掩盖空闲重绘
        QComboBox *field = nullptr;
        for (QComboBox *combo : dialog.findChildren<QComboBox *>()) {
            if (combo->isVisible() && combo->count() > 1) {
                field = combo;
                break;
            }
        }
        QVERIFY(field);
        counter.paints = 0;
        field->setCurrentIndex((field->currentIndex() + 1) % field->count());
        QTRY_VERIFY(counter.paints > 0);
        QTest::qWait(100);
        counter.updates = 0;
        QTest::qWait(150);
        QVERIFY(counter.updates <= 2);

        // 3. 【设置测试】【隐藏窗口】关闭可见窗口后不应继续触发绘制
        dialog.hide();
        QTest::qWait(50);
        counter.paints = 0;
        QTest::qWait(100);
        QCOMPARE(counter.paints, 0);
    }
};

/// @brief 使用隔离配置运行设置窗口回归测试
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
    QApplication::setApplicationName(QStringLiteral("mark-shot-settings-test"));
    SettingsIdleRepaintTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "settings_idle_repaint_test.moc"

#include "providers/provider_runtime_dependencies.h"

#include "debug_log.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QLibrary>
#include <QLibraryInfo>
#include <QPluginLoader>

#include <memory>

namespace markshot::providers {
namespace {

#ifdef Q_OS_LINUX
/**
 * 【插件】【运行时恢复】列出插件私有目录和系统目录中的 ONNX Runtime
 * @param pluginPath 插件绝对路径
 * @return 去重后的现有动态库路径，私有运行时优先于系统运行时
 */
QStringList onnxRuntimeCandidates(const QString &pluginPath)
{
    const QDir pluginDir = QFileInfo(pluginPath).absoluteDir();
    const QDir appDir(QCoreApplication::applicationDirPath());
    QStringList directories{
        pluginDir.absolutePath(),
        pluginDir.absoluteFilePath(QStringLiteral("../lib")),
        appDir.absoluteFilePath(QStringLiteral("../lib")),
        QLibraryInfo::path(QLibraryInfo::LibrariesPath),
        QStringLiteral("/usr/lib64"), QStringLiteral("/usr/lib"),
        QStringLiteral("/lib64"), QStringLiteral("/lib")
    };
    // 1. 【插件】【系统依赖】兼容使用独立 Qt 安装目录的 Debian 多架构系统
    for (const QString &name : QDir(QStringLiteral("/usr/lib")).entryList(
             {QStringLiteral("*-linux-gnu"), QStringLiteral("*-linux-gnueabihf")},
             QDir::Dirs | QDir::NoDotAndDotDot)) {
        directories.append(QStringLiteral("/usr/lib/") + name);
    }

    QStringList candidates;
    for (const QString &directory : directories) {
        const QFileInfo library(QDir(directory).filePath(QStringLiteral("libonnxruntime.so.1")));
        const QString path = library.canonicalFilePath();
        if (library.isFile() && !path.isEmpty() && !candidates.contains(path)) {
            candidates.append(path);
        }
    }
    return candidates;
}
#endif

} // namespace

QObject *loadProviderPluginWithRuntimeFallback(QPluginLoader *loader)
{
    if (!loader) {
        return nullptr;
    }
    QObject *instance = loader->instance();
#ifdef Q_OS_LINUX
    const QJsonObject metadata = loader->metaData().value(QStringLiteral("MetaData")).toObject();
    if (instance || metadata.value(QStringLiteral("name")).toString() != QStringLiteral("mark-shot-ocr-rapid")
        || !loader->errorString().contains(QStringLiteral("libonnxruntime.so"))) {
        return instance;
    }

    // 1. 【插件】【运行时恢复】正常加载保持原有 RUNPATH，仅在依赖失败后绕过同名库缓存
    const QString pluginPath = loader->fileName();
    const QLibrary::LoadHints hints = loader->loadHints();
    for (const QString &path : onnxRuntimeCandidates(pluginPath)) {
        auto runtime = std::make_unique<QLibrary>(path);
        runtime->setLoadHints(QLibrary::ResolveAllSymbolsHint);
        if (!runtime->load()) {
            continue;
        }
        // 2. 【插件】【重新加载】清除 QPluginLoader 的失败尝试状态，确保再次执行动态加载
        loader->setFileName(pluginPath);
        loader->setLoadHints(hints);
        instance = loader->instance();
        if (instance) {
            // 3. 【插件】【运行时恢复】插件存活期间保留依赖，不修改链接器配置或用户目录
            runtime->setParent(loader);
            runtime.release();
            markshot::debugLog("providers", "【插件】【运行时恢复】ONNX Runtime path=%s",
                               path.toUtf8().constData());
            return instance;
        }
        runtime->unload();
    }
#endif
    return instance;
}

} // namespace markshot::providers

#pragma once

class QObject;
class QPluginLoader;

namespace markshot::providers {

/**
 * 【插件】【运行时恢复】加载插件，仅在 Rapid OCR 的 ONNX 依赖失败时尝试明确的运行时路径
 * @param loader 插件加载器，成功时持有恢复所用的运行时库
 * @return 插件实例，失败时返回空指针并保留加载器错误信息
 */
QObject *loadProviderPluginWithRuntimeFallback(QPluginLoader *loader);

} // namespace markshot::providers

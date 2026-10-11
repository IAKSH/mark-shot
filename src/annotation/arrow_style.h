#pragma once

namespace markshot::shot::types {

/// @brief 箭头头部样式，已有编号保持配置兼容
enum class ArrowStyle {
    Fletched,
    Kde,
    BidirectionalFletched,
    BidirectionalKde,
};

/// @brief 与箭头头部独立组合的线条样式
enum class ArrowLineStyle {
    Solid,
    Dashed,
    Dotted,
};

} // namespace markshot::shot::types

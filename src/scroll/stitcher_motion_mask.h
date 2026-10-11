#pragma once

#include "scroll/stitcher_frame_profile.h"

#include <utility>

namespace markshot::scroll {

// 滚动截屏中的横向静止区域掩码。网页导航栏、PDF 缩略图栏等固定侧栏在相邻
// 两帧同一位置保持不变,混入行签名比较会掩盖正文的真实位移。掩码在同一屏幕
// 位置比较相邻帧的各采样带,只让发生变化的带参与位移匹配。
class MotionBandMask {
public:
    /// @brief 清空历史运动量,帧尺寸或滚动轴变化时调用,无返回值
    void reset();

    /// @brief 用相邻两帧在同一位置的签名差更新各采样带的运动量,无返回值
    /// @param previous 上一帧的行签名
    /// @param current 当前帧的行签名
    void observe(const ColSamples &previous, const ColSamples &current);

    /// @brief 位移匹配使用的采样带权重
    /// @return 静止带为 0、运动带为 1 的权重;未识别出侧栏时全部为 1
    const BandWeights &weights() const;

    /// @brief 是否已排除了部分静止采样带
    /// @return 存在被排除的采样带时返回 true
    bool partial() const;

    /// @brief 运动采样带覆盖的像素列区间,供逐像素行比较避开固定侧栏
    /// @param width 帧宽
    /// @return 起始列与宽度;未识别出侧栏时为整帧宽度
    std::pair<int, int> movingSpan(int width) const;

private:
    /// @brief 根据平滑后的运动量重新生成权重,无返回值
    void rebuildWeights();

    std::array<float, kColBandCount> m_motion{};
    bool m_hasMotion = false;
    BandWeights m_weights = frame_profile::kAllBands;
    bool m_partial = false;
};

}  // namespace markshot::scroll

#include "scroll/stitcher_motion_mask.h"

#include <algorithm>
#include <cmath>

namespace markshot::scroll {

namespace {

// 全帧最强运动量低于该值时视为未滚动,保留已有掩码,避免静止帧清空侧栏判定
constexpr float kMinFrameMotion = 1.0f;
// 采样带运动量低于最强带的该比例时视为静止;跨越侧栏边界、大部分为侧栏的带一并剔除
constexpr float kStaticMotionRatio = 0.10f;
// 静止判定的绝对下限,覆盖压缩噪声与侧栏选中高亮等细微变化
constexpr float kStaticMotionFloor = 1.0f;
// 新观察在平滑运动量中的占比
constexpr float kMotionBlend = 0.5f;
// 剔除侧栏后至少保留的运动采样带数,不足时退回全宽比较
constexpr int kMinMovingBands = 3;

}  // namespace

void MotionBandMask::reset()
{
    m_motion.fill(0.0f);
    m_hasMotion = false;
    m_weights = frame_profile::kAllBands;
    m_partial = false;
}

void MotionBandMask::observe(const ColSamples &previous, const ColSamples &current)
{
    const int height = static_cast<int>(current.size());
    if (height <= 0 || previous.size() != current.size()) {
        return;
    }

    // 1. 【长截图】【静止区域】在同一屏幕位置比较各采样带,跳过易受工具条干扰的上下边缘
    const int start = frame_profile::contentTopIgnore(height);
    const int end = height - frame_profile::contentBottomIgnore(height);
    if (end <= start) {
        return;
    }
    std::array<float, kColBandCount> motion{};
    for (int y = start; y < end; ++y) {
        for (int g = 0; g < kColBandCount; ++g) {
            motion[g] += std::abs(previous[y][g] - current[y][g]);
        }
    }
    float strongest = 0.0f;
    for (float &value : motion) {
        value /= static_cast<float>(end - start);
        strongest = std::max(strongest, value);
    }

    // 2. 【长截图】【静止区域】画面整体未动时没有区分依据,沿用已有掩码
    if (strongest < kMinFrameMotion) {
        return;
    }

    // 3. 【长截图】【静止区域】按全帧最强运动归一化后平滑,单帧动画不会立即改变掩码
    for (int g = 0; g < kColBandCount; ++g) {
        const float relative = motion[g] < kStaticMotionFloor ? 0.0f : motion[g] / strongest;
        m_motion[g] = m_hasMotion ? m_motion[g] * (1.0f - kMotionBlend) + relative * kMotionBlend : relative;
    }
    m_hasMotion = true;
    rebuildWeights();
}

void MotionBandMask::rebuildWeights()
{
    BandWeights weights{};
    int moving = 0;
    for (int g = 0; g < kColBandCount; ++g) {
        if (m_motion[g] >= kStaticMotionRatio) {
            weights[g] = 1.0f;
            ++moving;
        }
    }
    // 运动带过少时证据不足,宁可退回全宽比较,也不只凭局部动画定位
    if (moving < kMinMovingBands || moving == kColBandCount) {
        m_weights = frame_profile::kAllBands;
        m_partial = false;
        return;
    }
    m_weights = weights;
    m_partial = true;
}

const BandWeights &MotionBandMask::weights() const
{
    return m_weights;
}

bool MotionBandMask::partial() const
{
    return m_partial;
}

std::pair<int, int> MotionBandMask::movingSpan(int width) const
{
    if (!m_partial || width <= 0) {
        return {0, width};
    }
    int first = -1;
    int last = -1;
    for (int g = 0; g < kColBandCount; ++g) {
        if (m_weights[g] > 0.0f) {
            first = first < 0 ? g : first;
            last = g;
        }
    }
    const int start = frame_profile::bandColumns(width, first).first;
    const int end = frame_profile::bandColumns(width, last).second;
    return {start, std::max(1, end - start + 1)};
}

}  // namespace markshot::scroll

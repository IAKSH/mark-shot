#include "scroll/stitcher_frame_profile.h"

#include "scroll/stitcher.h"
#include "scroll/stitcher_internal.h"

#include <algorithm>
#include <cmath>

namespace markshot::scroll::frame_profile {

// 顶部按比例忽略的高度占比
constexpr float kColTopIgnoreRatio = 0.10f;
// 底部按比例忽略的高度占比
constexpr float kColBottomIgnoreRatio = 0.08f;
// 按比例忽略时的最小像素数
constexpr int kColMinIgnorePx = 16;
// 行像素比较最多抽取的采样列数
constexpr int kLineMaxSampleCols = 256;
// 采样带覆盖范围的起止宽度占比
constexpr float kColStartRatio = 0.04f;
constexpr float kColEndRatio = 0.96f;
// 相邻采样带之间留空的比例(相对单带宽度)
constexpr float kColBandGapRatio = 0.05f;

/// @brief 按高度与占比计算边缘忽略行数。
/// @param height 帧高。
/// @param ratio 忽略占比。
/// @return 忽略的行数;帧太矮时为 0。
static int scaledIgnore(int height, float ratio)
{
    if (height < 80) {
        return 0;
    }
    return std::min(height / 4, std::max(kColMinIgnorePx, static_cast<int>(height * ratio)));
}

int contentTopIgnore(int height)
{
    return scaledIgnore(height, kColTopIgnoreRatio);
}

int contentBottomIgnore(int height)
{
    return scaledIgnore(height, kColBottomIgnoreRatio);
}

bool isContentRow(int y, int height)
{
    return y >= contentTopIgnore(height) && y < height - contentBottomIgnore(height);
}

bool shouldCropContentRows(int overlapLen, int frameHeight, int minOverlap)
{
    return overlapLen >= minOverlap + contentTopIgnore(frameHeight) + contentBottomIgnore(frameHeight);
}

int requiredComparedRows(int minOverlap, bool cropped)
{
    return cropped ? std::max(24, minOverlap / 2) : minOverlap;
}

std::pair<int, int> bandColumns(int width, int band)
{
    // 采样带均分 4%~96% 的宽度,带间留出约 0.6% 的间隙,避开窗口边框与滚动条
    const float span = (kColEndRatio - kColStartRatio) / kColBandCount;
    const float start = kColStartRatio + span * band;
    return stitcher_internal::bandRange(width, start, start + span * (1.0f - kColBandGapRatio));
}

ColSamples computeColSamples(const QImage &frame)
{
    const int w = frame.width();
    const int h = frame.height();
    if (w <= 0 || h <= 0) {
        return {};
    }

    // 1. 帧在进入拼接管线前已统一为 ARGB32_Premultiplied,直接按 QRgb 读取
    //    即可;仅在遇到其它格式时才做一次转换,避免每帧的整图拷贝
    QImage converted;
    const QImage *source = &frame;
    if (frame.format() != QImage::Format_ARGB32_Premultiplied
        && frame.format() != QImage::Format_RGB32
        && frame.format() != QImage::Format_ARGB32) {
        converted = frame.convertToFormat(QImage::Format_RGB32);
        source = &converted;
    }

    // 2. 【长截图】【行签名】多个窄采样带覆盖整个宽度,固定侧栏只污染其所在的带
    ColSamples result(h);
    for (int g = 0; g < kColBandCount; ++g) {
        const auto [start, end] = bandColumns(w, g);
        const int sampleCount = std::max(1, std::min(kColMaxBandSamples, end - start + 1));
        const float step = sampleCount > 1
            ? static_cast<float>(end - start) / static_cast<float>(sampleCount - 1)
            : 0.0f;
        // 3. 每行对采样列求亮度均值,形成该行在此带上的签名
        for (int y = 0; y < h; ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(source->constScanLine(y));
            float sum = 0.0f;
            for (int i = 0; i < sampleCount; ++i) {
                const int x = std::clamp(static_cast<int>(std::lround(start + i * step)), 0, w - 1);
                const QRgb px = line[x];
                sum += 0.299f * qRed(px) + 0.587f * qGreen(px) + 0.114f * qBlue(px);
            }
            result[y][g] = sum / static_cast<float>(sampleCount);
        }
    }
    return result;
}

float rowMeanAbsDiff(const QRgb *a, const QRgb *b, int startX, int width)
{
    if (!a || !b || width <= 0) {
        return kNoMatchConfidence;
    }

    const int step = std::max(1, width / kLineMaxSampleCols);
    float sum = 0.0f;
    int count = 0;
    for (int x = startX; x < startX + width; x += step) {
        const QRgb ap = a[x];
        const QRgb bp = b[x];
        sum += std::abs(qRed(ap) - qRed(bp));
        sum += std::abs(qGreen(ap) - qGreen(bp));
        sum += std::abs(qBlue(ap) - qBlue(bp));
        count += 3;
    }
    return count > 0 ? sum / static_cast<float>(count) : kNoMatchConfidence;
}

/// @brief 判断帧内某行是否允许参与比较。
/// @param y 行号。
/// @param height 帧高。
/// @param fixedTop 顶部固定带高度。
/// @param fixedBottom 底部固定带高度。
/// @param softCrop 是否附加按比例的边缘裁剪。
/// @return 允许比较返回 true。
static bool usableRow(int y, int height, int fixedTop, int fixedBottom, bool softCrop)
{
    if (y < fixedTop || y >= height - fixedBottom) {
        return false;
    }
    return !softCrop || isContentRow(y, height);
}

// 剔除误差最大的一个采样带所需的最少参与带数
constexpr int kTrimWorstBandMin = 4;
// 判定一行具有纹理所需的相邻行亮度变化
constexpr float kTextureRowStep = 1.0f;
// 重叠区至少包含的纹理行数
constexpr int kMinTexturedRows = 8;

/// @brief 判断帧内某行在参与比较的采样带上是否与上一行存在亮度变化
/// @param cols 帧的行签名
/// @param y 行号
/// @param weights 各采样带权重
/// @return 存在纹理边缘时返回 true
static bool texturedRow(const ColSamples &cols, int y, const BandWeights &weights)
{
    if (y <= 0) {
        return false;
    }
    for (int g = 0; g < kColBandCount; ++g) {
        if (weights[g] > 0.0f && std::abs(cols[y][g] - cols[y - 1][g]) > kTextureRowStep) {
            return true;
        }
    }
    return false;
}

/// @brief 逐带累加两行签名的绝对差,无返回值
/// @param a 第一行签名
/// @param b 第二行签名
/// @param bandSums 各采样带的累计绝对差
static void accumulateRowDiff(const std::array<float, kColBandCount> &a,
                              const std::array<float, kColBandCount> &b,
                              std::array<float, kColBandCount> &bandSums)
{
    for (int g = 0; g < kColBandCount; ++g) {
        bandSums[g] += std::abs(a[g] - b[g]);
    }
}

/// @brief 汇总各采样带误差,参与带足够多时剔除误差最大的一带
/// @param bandSums 各采样带的累计绝对差
/// @param rows 参与比较的行数
/// @param weights 各采样带权重
/// @return 每行每带的平均差;没有参与带时返回极大值
static float trimmedBandMean(const std::array<float, kColBandCount> &bandSums, int rows,
                             const BandWeights &weights)
{
    float sum = 0.0f;
    float total = 0.0f;
    float worst = 0.0f;
    float worstWeight = 0.0f;
    int active = 0;
    for (int g = 0; g < kColBandCount; ++g) {
        if (weights[g] <= 0.0f) {
            continue;
        }
        const float weighted = weights[g] * bandSums[g];
        sum += weighted;
        total += weights[g];
        ++active;
        if (weighted > worst) {
            worst = weighted;
            worstWeight = weights[g];
        }
    }
    // 【长截图】【稳健误差】侧栏选中高亮、跨越侧栏边界的带只影响单个采样带,剔除后不拖累正文
    if (active >= kTrimWorstBandMin) {
        sum -= worst;
        total -= worstWeight;
    }
    if (rows <= 0 || total <= 0.0f) {
        return kNoMatchConfidence;
    }
    return sum / (static_cast<float>(rows) * total);
}

float frameColDiff(const ColSamples &cols1,
                   const ColSamples &cols2,
                   int offset,
                   int minOverlap,
                   int fixedTop,
                   int fixedBottom,
                   const BandWeights &weights)
{
    const int h1 = static_cast<int>(cols1.size());
    const int h2 = static_cast<int>(cols2.size());
    if (h1 == 0 || h2 == 0) {
        return kNoMatchConfidence;
    }

    // 1. 计算几何重叠长度,不足最小重叠直接放弃
    const int overlapLen = offset >= 0
        ? std::min(h1 - offset, h2)
        : std::min(h1, h2 + offset);
    if (overlapLen < minOverlap) {
        return kNoMatchConfidence;
    }

    // 2. 固定带行永远剔除;按比例裁剪仅在重叠足够长时附加
    const bool softCrop = shouldCropContentRows(overlapLen, std::min(h1, h2), minOverlap);
    const int start1 = offset >= 0 ? offset : 0;
    const int start2 = offset >= 0 ? 0 : -offset;
    std::array<float, kColBandCount> bandSums{};
    int rows = 0;
    int texturedRows = 0;
    for (int i = 0; i < overlapLen; ++i) {
        const int y1 = start1 + i;
        const int y2 = start2 + i;
        if (!usableRow(y1, h1, fixedTop, fixedBottom, softCrop)
            || !usableRow(y2, h2, fixedTop, fixedBottom, softCrop)) {
            continue;
        }
        ++rows;
        accumulateRowDiff(cols1[y1], cols2[y2], bandSums);
        if (texturedRows < kMinTexturedRows && texturedRow(cols2, y2, weights)) {
            ++texturedRows;
        }
    }

    // 3. 【长截图】【短重叠】仅按比例裁边时放宽行数;固定带削去后的残余重叠仍须满足最小重叠
    if (rows < requiredComparedRows(minOverlap, softCrop)) {
        return kNoMatchConfidence;
    }
    // 4. 【长截图】【空白重叠】整段空白的重叠无法区分位移,不能凭页边留白定位
    if (texturedRows < kMinTexturedRows) {
        return kNoMatchConfidence;
    }
    return trimmedBandMean(bandSums, rows, weights);
}

float overlapColDiff(const ColSamples &fullCols,
                     const ColSamples &frameCols,
                     int framePos,
                     int minOverlap,
                     int fixedTop,
                     int fixedBottom,
                     int *overlapLen,
                     int fullTopExclude,
                     int fullBottomExclude,
                     const BandWeights &weights)
{
    const int fullH = static_cast<int>(fullCols.size());
    const int frameH = static_cast<int>(frameCols.size());
    // 长图两端即将被裁掉的旧固定带不参与比较,避免固定带与新内容互比
    fullTopExclude = std::clamp(fullTopExclude, 0, fullH);
    fullBottomExclude = std::clamp(fullBottomExclude, 0, fullH - fullTopExclude);
    const int fullStart = std::max(fullTopExclude, framePos);
    const int fullEnd = std::min(fullH - fullBottomExclude, framePos + frameH);
    const int len = fullEnd - fullStart;
    if (overlapLen) {
        *overlapLen = std::max(0, len);
    }
    if (len < minOverlap) {
        return kNoMatchConfidence;
    }

    // 1. 只在帧与长图的重叠区内比较;长图内容无固定带,窗口只作用于帧行
    const int frameStart = fullStart - framePos;
    const bool softCrop = shouldCropContentRows(len, frameH, minOverlap);
    std::array<float, kColBandCount> bandSums{};
    int rows = 0;
    int texturedRows = 0;
    for (int row = 0; row < len; ++row) {
        const int frameY = frameStart + row;
        if (!usableRow(frameY, frameH, fixedTop, fixedBottom, softCrop)) {
            continue;
        }
        ++rows;
        accumulateRowDiff(fullCols[fullStart + row], frameCols[frameY], bandSums);
        if (texturedRows < kMinTexturedRows && texturedRow(frameCols, frameY, weights)) {
            ++texturedRows;
        }
    }

    // 2. 有效行不足或整段空白时拒绝,防止在稀疏比较上得出高置信结论
    if (rows < requiredComparedRows(minOverlap, softCrop) || texturedRows < kMinTexturedRows) {
        return kNoMatchConfidence;
    }
    return trimmedBandMean(bandSums, rows, weights);
}

}  // namespace markshot::scroll::frame_profile

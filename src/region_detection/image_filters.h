#pragma once

#include "region_detection/image_gray.h"

namespace markshot::region_detection {

/**
 * 对灰度图执行 Canny 边缘检测。
 * @param image 灰度图。
 * @param lowThreshold 弱边缘阈值。
 * @param highThreshold 强边缘阈值。
 * @return 二值边缘图，边缘像素为 255。
 */
GrayImage canny(const GrayImage &image, float lowThreshold, float highThreshold);

/**
 * 以 LInf 半径 1 对二值图做膨胀。
 * @param image 输入图。
 * @return 膨胀结果。
 */
GrayImage dilateLInf1(const GrayImage &image);

/**
 * 以 LInf 半径 1 对二值图做腐蚀。
 * @param image 输入图。
 * @return 腐蚀结果。
 */
GrayImage erodeLInf1(const GrayImage &image);

/**
 * 以 LInf 半径 1 对二值图做闭运算。
 * @param image 输入图。
 * @return 闭运算结果。
 */
GrayImage closeLInf1(const GrayImage &image);

} // namespace markshot::region_detection

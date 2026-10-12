#pragma once

#include <QImage>
#include <QVector>

#include <QtGlobal>

namespace markshot::region_detection {

/**
 * 单通道灰度缓冲，像素按行优先存储。
 */
struct GrayImage {
    int width = 0;
    int height = 0;
    QVector<quint8> pixels;

    /**
     * 创建指定尺寸的灰度缓冲，像素初值为 0。
     * @param w 宽度。
     * @param h 高度。
     * @return 空灰度图。
     */
    static GrayImage create(int w, int h)
    {
        GrayImage image;
        image.width = qMax(0, w);
        image.height = qMax(0, h);
        image.pixels.fill(0, image.width * image.height);
        return image;
    }

    /**
     * 读取指定坐标的像素。
     * @param x 列坐标。
     * @param y 行坐标。
     * @return 灰度值。
     */
    quint8 at(int x, int y) const
    {
        return pixels[y * width + x];
    }

    /**
     * 写入指定坐标的像素。
     * @param x 列坐标。
     * @param y 行坐标。
     * @param value 灰度值。
     * @return 无返回值。
     */
    void set(int x, int y, quint8 value)
    {
        pixels[y * width + x] = value;
    }
};

/**
 * 把彩色图转换为未预乘的 ARGB32。
 * @param image 输入图像。
 * @return ARGB32 图像。
 */
QImage normalizedRgbImage(const QImage &image);

/**
 * 按 BT.601 整数权重把彩色图转为灰度。
 * @param image 输入图像。
 * @return 灰度缓冲。
 */
GrayImage grayscaleFromRgba(const QImage &image);

} // namespace markshot::region_detection

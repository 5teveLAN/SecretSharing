#include "bitmapitem.h"
#include <QFileInfo>
#include <QVector>
#include <QtGlobal>

BitmapItem::BitmapItem(const QString &filePath)
    : m_filePath(filePath)
    , m_width(0)
    , m_height(0)
    , m_valid(false)         // 預設「失敗」，成功才改成 true
{
    QImage img(filePath);    // 用路徑載入
    if (img.isNull()) {
        return;              // 載入失敗 → 直接 return，m_valid 保持 false
    }

    // 到這裡代表載入成功
    m_name     = QFileInfo(filePath).fileName();
    m_original = img;
    m_width    = img.width();
    m_height   = img.height();
    m_valid    = true;       // 標記成功
}

bool BitmapItem::isValid() const
{
    return m_valid;
}

// ===== 基本資訊 getters =====
QString BitmapItem::getName() const
{
    return m_name;
}

QString BitmapItem::getFilePath() const
{
    return m_filePath;
}

int BitmapItem::getWidth() const
{
    return m_width;
}

int BitmapItem::getHeight() const
{
    return m_height;
}

// ===== 影像 getters =====

// 原圖：建構子已載入，直接回傳
QImage BitmapItem::getOriginal() const
{
    return m_original;
}

// 灰階：第一次呼叫才建立並快取 (m_gray 是 mutable)
QImage BitmapItem::getGray() const
{
    if (m_gray.isNull()) {
        createGray();
    }
    return m_gray;
}

// 二值：第一次呼叫才建立並快取
QImage BitmapItem::getBinary() const
{
    if (m_binary.isNull()) {
        createBinary();
    }
    return m_binary;
}

// 8 色：第一次呼叫才建立並快取
QImage BitmapItem::getColor8() const
{
    if (m_color8.isNull()) {
        create8Color();
    }
    return m_color8;
}

void BitmapItem::create8Color() const
{
    if (m_original.isNull()) {
        return;              // 沒有原圖 → 無法產生，m_color8 保持 isNull
    }

    // 輸出用 Format_RGB32：不用調色盤，每個像素自己填 RGB。
    // 視覺上只有 8 種顏色，但格式仍是 32-bit 彩色圖。
    QImage out(m_width, m_height, QImage::Format_RGB32);

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const QRgb rgb = m_original.pixel(x, y);   // pixel() 任何格式都能用
            const int  r = qRed(rgb);
            const int  g = qGreen(rgb);
            const int  b = qBlue(rgb);
            const int  out_r = (r <= 127) ? 0 : 255;
            const int  out_g = (g <= 127) ? 0 : 255;
            const int  out_b = (b <= 127) ? 0 : 255;

            out.setPixel(x, y, qRgb(out_r,out_g,out_b));   // 自己填 RGB
        }
    }

    // 寫入快取 (m_color8 是 mutable)
    m_color8 = out;
}

void BitmapItem::createGray() const
{
    if (m_original.isNull()) {
        return;              // 沒有原圖 → 無法產生，m_color8 保持 isNull
    }

    // 輸出用 Format_RGB32：不用調色盤，每個像素自己填 RGB。
    // 視覺上只有 8 種顏色，但格式仍是 32-bit 彩色圖。
    QImage out(m_width, m_height, QImage::Format_RGB32);

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const QRgb rgb = m_original.pixel(x, y);   // pixel() 任何格式都能用
            const int  r = qRed(rgb);
            const int  g = qGreen(rgb);
            const int  b = qBlue(rgb);
            const int  out_r = (r+g+b)/3;

            out.setPixel(x, y, qRgb(out_r,out_r,out_r));   // 自己填 RGB
        }
    }

    // 寫入快取 (m_gray 是 mutable)
    m_gray = out;
}

void BitmapItem::createBinary() const
{
    if (m_original.isNull()) {
        return;              // 沒有原圖 → 無法產生，m_color8 保持 isNull
    }

    // 輸出用 Format_RGB32：不用調色盤，每個像素自己填 RGB。
    // 視覺上只有 8 種顏色，但格式仍是 32-bit 彩色圖。
    QImage out(m_width, m_height, QImage::Format_RGB32);

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const QRgb rgb = m_original.pixel(x, y);   // pixel() 任何格式都能用
            const int  r = qRed(rgb);
            const int  g = qGreen(rgb);
            const int  b = qBlue(rgb);
            const int  out_r = ((r+g+b)/3)<=127?0:255;

            out.setPixel(x, y, qRgb(out_r,out_r,out_r));   // 自己填 RGB
        }
    }

    // 寫入快取 (m_binary 是 mutable)
    m_binary = out;
}
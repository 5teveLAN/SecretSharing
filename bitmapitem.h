#ifndef BITMAPITEM_H
#define BITMAPITEM_H

#include <QString>
#include <QImage>

// BitmapItem
// 代表一張已讀入的 BMP 圖檔，並在建構時預先計算好
// 灰階 (gray)、二值 (binary)、8 色 (color8) 三種衍生版本。
class BitmapItem
{
public:
    // 建構子：傳入 BMP 檔案的完整路徑
    // 建構時會載入原圖
    explicit BitmapItem(const QString &filePath);

    // 載入是否成功（檔案不存在或不是有效圖片時為 false）
    bool isValid() const;

    // --- 基本資訊 ---
    QString getName()     const;   // 檔名，例如 "a.bmp"
    QString getFilePath() const;   // 完整路徑
    int     getWidth()    const;   // 像素寬
    int     getHeight()   const;   // 像素高

    // --- 取得各版本影像 ---
    QImage getOriginal() const;    // 原始 24-bit 彩色圖
    QImage getGray()     const;    // 灰階圖
    QImage getBinary()   const;    // 二值圖
    QImage getColor8()   const;    // 8 色圖

    // const function 不改物件的東西

private:
    // --- 私有輔助方法：產生衍生影像 ---
    // 直接計算並寫入對應的成員 (m_gray / m_binary / m_color8)
    // 只讀取 m_original、寫入 mutable 成員，故為 const
    void createGray() const;
    void createBinary() const;
    void create8Color() const;

    // --- 成員變數 ---
    QString m_name;         // 檔名
    QString m_filePath;     // 完整路徑
    int     m_width;        // 像素寬
    int     m_height;       // 像素高
    bool    m_valid;        // 是否載入成功
    
    // mutable 允許在 const 函式裡改
    mutable QImage  m_original;     // 原圖 (24-bit 彩色)
    mutable QImage  m_gray;         // 灰階
    mutable QImage  m_binary;       // 二值
    mutable QImage  m_color8;       // 8 色
};

#endif // BITMAPITEM_H

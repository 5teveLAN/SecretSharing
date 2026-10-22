#ifndef BITMAPITEM_H
#define BITMAPITEM_H

#include <QString>
#include <QImage>

#include <cstdint>
#include <functional>
#include <vector>

// 進度回報函式：done / total（目前以「原圖列數」為單位）
// 產生機密圖時會在每一列處理完後呼叫一次；傳空的 (ProgressFn{}) 代表不回報。
using ProgressFn = std::function<void(int done, int total)>;

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
    // 如果 m_xxx 為空， 才會去呼叫 createXXX()
    QImage getOriginal() const;    // 原始 24-bit 彩色圖
    QImage getGray()     const;    // 灰階圖
    QImage getBinary()   const;    // 二值圖
    QImage getColor8()   const;    // 8 色圖

    // --- 機密分享 (secret sharing) ---
    // 設定產生機密圖的參數；改變設定會清掉先前產生的 m_sharings 快取
    void setSSConfig(int n, int k, int ratioRow, int ratioCol);

    // 一次取得全部機密圖 (共 2 張)
    // m_shares 為空才呼叫 create2Shares() 建立並快取
    // 尚未載入成功（或參數不合法）時回傳空的 vector
    std::vector<QImage> getShares() const;

    // 重新產生機密圖並回報進度（一律重算，不等快取）
    // 每處理完原圖的一列就呼叫一次 progress(done, total)，
    //   done  : 已處理的列數 (1 .. total)
    //   total : 原圖總列數 (m_height)
    // progress 傳空的 (ProgressFn{}) 代表不回報進度。
    // 尚未載入成功（或參數不合法）時回傳空的 vector（此時不會呼叫 progress）。
    std::vector<QImage> regenerateShares(ProgressFn progress = {}) const;

    // const function 不改物件的東西

private:
    // --- 私有輔助方法：產生衍生影像 ---
    // 直接計算並寫入對應的成員 (m_gray / m_binary / m_color8)
    // 只讀取 m_original、寫入 mutable 成員，故為 const
    void createGray() const;
    void createBinary() const;
    void create8Color() const;

    // 產生機密圖（目前只支援 2 張 share）：
    //   1. 先用 buildEncodingMatrix() 建立編碼矩陣 C[a][s][i][j]
    //   2. 每個原圖像素擴張成 ratioRow x ratioCol 的一塊，整張圖因此放大
    //      ratioRow (垂直) x ratioCol (水平) 倍
    //   3. 每個原圖像素各自隨機挑一種可能性 a (0 .. m-1)，
    //      白 → 兩張 share 都取 C[a][0]；黑 → share0 取 C[a][0]、share1 取 C[a][1]
    // 每處理完一列呼叫一次 progress（可為空）
    // 結果寫入 m_shares (mutable)，故為 const
    void create2Shares(ProgressFn progress) const;

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

    // --- 機密分享的參數 ---
    int m_n        = 2;     // 機密圖張數 (secret 數)
    int m_k        = 1;     // 容錯張數
    int m_ratioRow = 1;     // 像素擴張量：垂直倍率 (1x2 → col 拉伸兩倍)
    int m_ratioCol = 2;     // 像素擴張量：水平倍率

    // mutable 允許在 const 函式裡改
    mutable std::vector<QImage> m_shares;   // 機密圖 (共 m_n 張)
};

// ===== 編碼矩陣 (encoding matrix) =====
// 4D：C[a][s][i][j]
//   a : 第幾種可能性 (0 .. C(m, m/2)-1)
//   s : 第幾張 share (0 : 第一張, 1 : 第一張的 bit 反轉)
//   i : 這組格子內的第 i 列 (0 .. ratioRow-1)
//   j : 這組格子內的第 j 行 (0 .. ratioCol-1)
// 值：1 = 黑, 0 = 白
//
// m = ratioRow * ratioCol，必須是偶數；不合法時回傳空矩陣。
// 可能性數 = m! / ((m/2)! (m/2)!)
// 純函式：只依賴 ratioRow / ratioCol，與任何 BitmapItem 物件狀態無關。
std::vector<std::vector<std::vector<std::vector<uint8_t>>>>
buildEncodingMatrix(int ratioRow, int ratioCol);

#endif // BITMAPITEM_H

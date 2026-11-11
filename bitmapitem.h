#ifndef BITMAPITEM_H
#define BITMAPITEM_H

#include <QString>
#include <QImage>

#include <cstdint>
#include <vector>

class Progress;     // 進度回報器（見 progress.h）；這裡只用到指標，前置宣告即可

class BitmapItem;

// ===== 版本目錄 (version catalog) =====
// 一個「版本」= 畫面上可以顯示的一種影像 + 它的名字。
//   label    : 給 UI 顯示的文字（pictureList 的列文字）
//   getImage : 取得該版本影像的成員函式指標（呼叫後才計算，會用快取）
//
// 為什麼放在 BitmapItem 裡而不是 UI？
//   這樣「有哪些版本」只有 BitmapItem 知道，
//   MainWindow 只是把 BitmapItem::versions() 原封不動列到 pictureList，
//   新增版本只要動 BitmapItem，UI 完全不用改。
struct BitmapItemVersion
{
    const char *label;                          // 顯示名稱，例如 "Gray"
    QImage (BitmapItem::*getImage)() const;     // 取圖的成員函式，例如 &BitmapItem::getGray
};

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

    // --- 取得 CMYK 各色版 (channel) ---
    // 如果 m_xxxChannel 為空，才會去呼叫 createXXXChannel()
    //
    // 為什麼叫 Channel 而不是版本？
    //   gray / binary / color8 是「另一種版本」；
    //   C / M / Y 是把圖「拆出來」的單一色版（印刷分色用），
    //   兩者語意不同，所以用 Channel 當後綴，也避免和 m_color8 混淆。
    //
    // 來源是 8 色圖 (m_color8)，不是原圖也不是二值圖：
    //   m_color8 的 R / G / B 各自只有 0 / 255，所以分出來的色版也只有黑 / 白，
    //   可以直接丟進現有黑白 secret sharing。
    // 取法（標準 CMYK 分色，不做 GCR / UCR）：
    //   C = 255 - R、M = 255 - G、Y = 255 - B（R/G/B 取自 m_color8）
    // 之後要接真正彩色 secret sharing 時，只要改 createXXXChannel() 的取法。
    QImage getCChannel() const;    // C 青版 (cyan)
    QImage getMChannel() const;    // M 洋紅版 (magenta)
    QImage getYChannel() const;    // Y 黃版 (yellow)

    // --- 版本目錄：pictureList 的內容由此決定 ---
    // 所有可顯示的版本，順序就是 UI 清單的順序（[0] = 原圖，預設選這一列）。
    //
    // 新增一個版本 = 只做兩件事：
    //   1. 在上面加一個 getXXX()（第一次呼叫才計算並快取）
    //   2. 在 bitmapitem.cpp 的 versions() 表格加一行 { "XXX", &BitmapItem::getXXX }
    // MainWindow / pictureList 不需要任何修改，清單會自動多一列。
    static const std::vector<BitmapItemVersion> &versions();

    // 版本數量（= versions().size()）方便 UI 直接拿來當迴圈上限
    static int versionCount();

    // 依版本索引取圖（索引 = 在 versions() 的位置，也就是 pictureList 的列號）
    // 索引越界 → 回傳空 QImage
    QImage getImageAt(int index) const;

    // --- 機密分享 (secret sharing) ---
    // 設定產生機密圖的參數
    // 改變設定會清掉先前產生的 m_sharings 快取
    void setSSConfig(int n, int k, int ratioRow, int ratioCol);

    // 一次取得全部機密圖 (共 2 張)
    //
    //   progress        : 進度回報器，可傳 nullptr（= 不回報進度）。
    //                     每算完原圖的一列就會呼叫一次 progress->report(已完成列數)，
    //                     total 由本函式透過 progress->setTotal() 設定（= 原圖列數）。
    //   forceRegenerate : false → m_shares 為空才計算（原本的行為，會用快取）
    //                     true  → 丟掉快取、重新亂數抽樣重算（每次結果都不同）
    //
    // 尚未載入成功（或參數不合法）時回傳空的 vector（此時不會呼叫 report）。
    std::vector<QImage> getShares(Progress *progress = nullptr,
                                  bool forceRegenerate = false) const;

    // const function 不改物件的東西

private:
    // --- 私有輔助方法：產生衍生影像 ---
    // 直接計算並寫入對應的成員 (m_gray / m_binary / m_color8)
    // 只讀取 m_original、寫入 mutable 成員，故為 const
    void createGray() const;
    void createBinary() const;
    void create8Color() const;

    // 產生 C / M / Y 色版，直接寫入對應的成員 (m_cChannel / m_mChannel / m_yChannel)
    // 資料來源是 m_color8（會透過 getColor8() 確保已建立）
    // 只讀取 m_original / m_color8、寫入 mutable 成員，故為 const
    void createCChannel() const;
    void createMChannel() const;
    void createYChannel() const;

    // 產生機密圖（目前只支援 2 張 share）：
    //   1. 先用 buildEncodingMatrix() 建立編碼矩陣 C[a][s][i][j]
    //   2. 每個原圖像素擴張成 ratioRow x ratioCol 的一塊，整張圖因此放大
    //      ratioRow (垂直) x ratioCol (水平) 倍
    //   3. 每個原圖像素各自隨機挑一種可能性 a (0 .. m-1)，
    //      白 → 兩張 share 都取 C[a][0]；黑 → share0 取 C[a][0]、share1 取 C[a][1]
    // 每算完一列呼叫一次 progress->report()（progress 可為 nullptr）
    // 結果寫入 m_shares (mutable)，故為 const
    void create2Shares(Progress *progress) const;

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

    // C / M / Y 三個色版（同樣第一次呼叫才建立並快取）
    mutable QImage  m_cChannel;     // C 青版
    mutable QImage  m_mChannel;     // M 洋紅版
    mutable QImage  m_yChannel;     // Y 黃版

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

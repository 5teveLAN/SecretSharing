#include "bitmapitem.h"
#include <QFileInfo>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <vector>

// ===== 編碼矩陣 (encoding matrix) =====
// 回傳 4D 矩陣 C[a][s][i][j]（定義與維度說明見 bitmapitem.h）
//
// 產生方式：
//   1. 以一維 flat 表示「這一組格子要填黑還是白」，長度 m = ratioRow * ratioCol
//   2. flat 先排成 {0,0,...,1,1}（遞增，恰有 m/2 個 1），
//      再用 next_permutation 列出所有不重複的排列 → 共 m! / ((m/2)! (m/2)!) 種
//   3. 每一種排列是一個「可能性 a」：
//        C[a][0][i][j] = flat[p]         (第一張 share)
//        C[a][1][i][j] = flat[p] ^ 1     (第二張 share = 第一張的 bit 反轉)
//      其中 p 是展平的一維位置，p → (i, j) 為 p / ratioCol, p % ratioCol
std::vector<std::vector<std::vector<std::vector<uint8_t>>>>
buildEncodingMatrix(int ratioRow, int ratioCol)
{
    std::vector<std::vector<std::vector<std::vector<uint8_t>>>> C;

    const int m = ratioRow * ratioCol;
    // 參數不合法 → 回傳空矩陣
    //   m 必須是偶數，否則「黑:白」無法各佔一半
    if (ratioRow < 1 || ratioCol < 1 || m % 2 != 0) {
        return C;
    }

    // 只用來跑 next_permutation 的一維暫存
    // 注意：next_permutation 從「最小排列（遞增）」一路走到「最大排列（遞減）」，
    //       所以初始值必須是 {0,0,...,1,1}（遞增），
    //       若排成 {1,1,...,0,0}（遞減）會被視為「已經是最後一種」而只得到 1 種。
    std::vector<uint8_t> flat(m, 0);
    std::fill(flat.begin() + m - m / 2, flat.end(), 1);   // 後半黑、前半白 → 遞增

    // 注意：next_permutation 在「最後一種排列」之後會回傳 false，
    //       所以不能用 do-while（會漏掉最後一種），要「先判斷再使用」
    bool more = true;
    while (more) {
        // 2 張 share，各自 ratioRow x ratioCol，先全部填 0 (白)
        std::vector<std::vector<std::vector<uint8_t>>> shares(
            2, std::vector<std::vector<uint8_t>>(
                   ratioRow, std::vector<uint8_t>(ratioCol, 0)));

        for (int p = 0; p < m; ++p) {
            const int i = p / ratioCol;   // 這組格子內的第 i 列
            const int j = p % ratioCol;   // 這組格子內的第 j 行

            shares[0][i][j] = flat[p];          // C[a][0][i][j]
            shares[1][i][j] = flat[p] ^ 1;      // C[a][1][i][j] = 第一張反轉
        }

        C.push_back(std::move(shares));

        more = std::next_permutation(flat.begin(), flat.end());
    }

    return C;
}

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

// ===== 機密分享 =====

// 設定參數：只有真的改變時才動作，避免無謂地丟掉快取。
// 因為機密圖是由這組參數算出來的，所以任何一個參數變了，
// 先前產生的 m_sharings 就必須作廢（清空 → 下次 getSharings() 重算）。
void BitmapItem::setSSConfig(int n, int k, int ratioRow, int ratioCol)
{
    // 防呆：n 至少要有 2 張才意義上能「分享」；
    //       擴張倍率必須 >= 1，否則影像會變成 0 寬 / 0 高。
    if (n        < 2) n        = 2;
    if (k        < 0) k        = 0;
    if (ratioRow < 1) ratioRow = 1;
    if (ratioCol < 1) ratioCol = 1;

    // 參數沒變 → 快取還有效，直接 return
    if (n == m_n && k == m_k
        && ratioRow == m_ratioRow && ratioCol == m_ratioCol) {
        return;
    }

    m_n        = n;
    m_k        = k;
    m_ratioRow = ratioRow;
    m_ratioCol = ratioCol;

    // 參數變了 → 舊的機密圖不再對應現在的設定，清掉
    m_shares.clear();
}

void BitmapItem::createShares() const
{
    if (m_original.isNull()) {
        return;              // 沒有原圖 → 無法產生，m_color8 保持 isNull
    }
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
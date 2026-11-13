#ifndef BITMAPITEM_H
#define BITMAPITEM_H

#include <QString>
#include <QImage>

#include <cstdint>
#include <vector>

class Progress;     // 進度回報器（見 progress.h）；這裡只用到指標，前置宣告即可

class BitmapItem;

// ===== 機密分享的來源模式 (share mode) =====
// 決定 createShares() 要「讀哪張圖」來判斷黑/白，以及「填什麼顏色」：
//   Binary  : 讀 m_binary，黑/白 (0,0,0) / (255,255,255)
//   Color8  : 特別模式，不套「黑/白 mask + 編碼矩陣」，而是把 C / M / Y 三個色版的
//             share 合併成一張 8 色 share（見 createColor8Shares()）
//   Cyan    : 讀 m_cChannel，填青墨 (  0,255,255) / 白
//   Magenta : 讀 m_mChannel，填洋紅 (255,  0,255) / 白
//   Yellow  : 讀 m_yChannel，填黃墨 (255,255,  0) / 白
//
// 每個模式各自有一份 share 快取，互不影響
// （見 m_Binary_shares / m_8Color_shares / m_C_shares / m_M_shares / m_Y_shares）。
enum class ShareMode
{
    Binary,
    Color8,
    Cyan,
    Magenta,
    Yellow,
};

// ===== 版本目錄 (version catalog) =====
// 一個「版本」= 畫面上可以顯示的一種影像 + 它的名字，
// 以及它「能不能拿來當 share 的來源」。
//   label       : 給 UI 顯示的文字（pictureList 的列文字）
//   getImage    : 取得該版本影像的成員函式指標（呼叫後才計算，會用快取）
//   sharePrefix : 這個版本當 share 來源時的名稱前綴（例："C" → "C_share0"）
//                 nullptr = 這個版本不支援生成 share（目前是 Original / Gray）
//   shareMode   : 對應的 ShareMode（sharePrefix 為 nullptr 時此值不採用）
//
// 為什麼放在 BitmapItem 裡而不是 UI？
//   這樣「有哪些版本」只有 BitmapItem 知道，
//   MainWindow 只是把 BitmapItem::versions() 原封不動列到 pictureList，
//   新增版本只要動 BitmapItem，UI 完全不用改。
//
// 為什麼 label 和 sharePrefix 要分開存？
//   label 是給人看的名稱（"C Channel"），sharePrefix 是給 shareList / 檔名用的
//   短前綴（"C"）。使用者要的是 C_share0 而不是 C_Channel_share0，
//   所以不能直接從 label 推出來，必須各自寫在表格裡。
struct BitmapItemVersion
{
    const char *label;                          // 顯示名稱，例如 "Gray"
    QImage (BitmapItem::*getImage)() const;     // 取圖的成員函式，例如 &BitmapItem::getGray
    const char *sharePrefix;                    // share 名稱前綴；nullptr = 不支援 share
    ShareMode   shareMode;                      // share 來源模式，例如 ShareMode::Cyan
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
    //   m_color8 的 R / G / B 各自只有 0 / 255，所以分出來的色版也只有兩種顏色，
    //   可以直接丟進現有黑白 secret sharing。
    // 取法（直接取該通道，不做反相）：
    //   C = (R, 255, 255)、M = (255, G, 255)、Y = (255, 255, B)（R/G/B 取自 m_color8）
    //   該通道為 0 → 有墨（青/洋紅/黃墨），為 255 → 沒有墨（白色）
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

    // --- 版本 → share 的對應（同樣以 versions() 為唯一來源）---
    // UI 只問這三個問題，不需要知道 Binary / C / M / Y 這些名字：
    //   1. 這個版本能不能生成 share？      supportsShares()
    //   2. 要交給 getShares() 哪個 mode？  shareModeOf()
    //   3. shareList 每一列要叫什麼？      sharePrefix()
    //
    // 索引越界（或該版本不支援 share）時的預設回傳：
    //   supportsShares() → false
    //   shareModeOf()    → ShareMode::Binary（呼叫前應先用 supportsShares() 確認）
    //   sharePrefix()    → 空 QString
    static bool       supportsShares(int versionIndex);
    static ShareMode  shareModeOf(int versionIndex);
    static QString    sharePrefix(int versionIndex);

    // --- 機密分享 (secret sharing) ---
    // 設定產生機密圖的參數
    // 改變設定會清掉先前產生的 m_sharings 快取
    void setSSConfig(int n, int k, int ratioRow, int ratioCol);

    // 一次取得某個模式的全部機密圖 (共 2 張)
    //
    //   mode            : 來源模式 (Binary / Color8 / Cyan / Magenta / Yellow)，
    //                     決定讀哪張圖、填什麼顏色。平常不寫就是 Binary。
    //   progress        : 進度回報器，可傳 nullptr（= 不回報進度）。
    //                     每算完原圖的一列就會呼叫一次 progress->report(已完成列數)，
    //                     total 由本函式透過 progress->setTotal() 設定（= 原圖列數）。
    //   forceRegenerate : false → 該模式的 share 快取為空才計算（會用快取）
    //                     true  → 丟掉快取、重新亂數抽樣重算（每次結果都不同）
    //
    // 每個模式各自快取：取 Cyan 不會動到 Binary 的快取，反之亦然。
    // 但 Color8 是由 C/M/Y 三色版合併而來，所以它和 C/M/Y 共用同一份 C/M/Y 快取，
    // 強制重算 (forceRegenerate = true) 時會連 C/M/Y 一起重抽，避免混到舊位元。
    // 尚未載入成功（或參數不合法）時回傳空的 vector（此時不會呼叫 report）。
    std::vector<QImage> getShares(ShareMode mode = ShareMode::Binary,
                                  Progress *progress = nullptr,
                                  bool forceRegenerate = false) const;

    // --- 疊圖 (superimposed)：把同一個模式的兩張 share 疊起來 ---
    // 用途：shareList 的「<prefix>_superimposed」那一列（Binary_superimposed / C_superimposed …）。
    //
    // 計算方法（逐像素，A = share0、B = share1，取同一個座標）：
    //   Binary / Cyan / Magenta / Yellow：
    //     (A == inkColor || B == inkColor) ? inkColor : 白 (255,255,255)
    //     inkColor 依模式而定，和 createShares() 填的墨色一致：
    //       Binary → 黑 (0,0,0)、Cyan → 青 (0,255,255)、
    //       Magenta → 洋紅 (255,0,255)、Yellow → 黃 (255,255,0)
    //     → 任一張 share 在該點是「墨」，疊起來就是「墨」；兩張都是白才是白。
    //   Color8：
    //     不能比 inkColor（share 有 8 種顏色），改成逐通道取 min（墨是 0、白是 255）：
    //       out = qRgb( min(A.R,B.R), min(A.G,B.G), min(A.B,B.B) )  ← 即「or」
    //     → 任一張在該通道有墨 → 有墨；還原出 SecretN (a,b,c)
    //     （見 createColor8Superimposed()）
    //
    // 前提：只有「該模式已經有 share」才算得出來。
    //   沒有 share（還沒按 Generate、或 col × row 為奇數）→ 回傳空 QImage，
    //   顯示端自然變成空白。本函式不會自己觸發 createShares()。
    //
    // 結果存在 m_X_superimposed（mutable，第一次算完就快取）；
    // setSSConfig() 改參數、getShares(forceRegenerate = true) 重抽時都會一併清掉。
    QImage getSuperimposed(ShareMode mode = ShareMode::Binary) const;

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
    // 取法：C = (R,255,255)、M = (255,G,255)、Y = (255,255,B)，不做反相
    // 只讀取 m_original / m_color8、寫入 mutable 成員，故為 const
    void createCChannel() const;
    void createMChannel() const;
    void createYChannel() const;

    // 產生疊圖：把該模式的兩張 share 逐像素疊起來，寫入 m_X_superimposed
    // 只看 getShares(mode) 的結果，不會自己生成 share
    //   若 share 不存在 / 少於 2 張 → 不做事，m_X_superimposed 保持 isNull
    // 只讀取 share、寫入 mutable 成員，故為 const
    void createSuperimposed(ShareMode mode) const;

    // 產生機密圖（目前只支援 2 張 share）：
    //   1. 先用 buildEncodingMatrix() 建立編碼矩陣 C[a][s][i][j]
    //   2. 每個原圖像素擴張成 ratioRow x ratioCol 的一塊，整張圖因此放大
    //      ratioRow (垂直) x ratioCol (水平) 倍
    //   3. 每個原圖像素各自隨機挑一種可能性 a (0 .. m-1)，
    //      白 → 兩張 share 都取 C[a][0]；黑 → share0 取 C[a][0]、share1 取 C[a][1]
    //
    // 通用形：同一套演算法，只差在「讀哪張圖判斷黑/白」與「填什麼顏色」：
    //   mode = Binary  → 讀 getBinary()，qRed(p) <= 127 當黑，黑填 (0,0,0)
    //   mode = Cyan    → 讀 getCChannel()，maskCyan()    R == 0 當有墨，填 (0,255,255)
    //   mode = Magenta → 讀 getMChannel()，maskMagenta() G == 0 當有墨，填 (255,0,255)
    //   mode = Yellow  → 讀 getYChannel()，maskYellow()  B == 0 當有墨，填 (255,255,0)
    //
    // mode = Color8 是特例：它不用自己算編碼矩陣，也不做「黑/白 mask」，
    //   而是把 C / M / Y 三個色版的 share 逐像素合併成一張 8 色 share，
    //   所以在這裡直接轉呼叫 createColor8Shares()（見下方說明）。
    //
    // 每算完一列呼叫一次 progress->report()（progress 可為 nullptr）
    // 結果寫入對應模式的 shares vector (mutable)，故為 const
    void createShares(ShareMode mode, Progress *progress) const;

    // --- 8 Color 專用：C / M / Y 三色版合併 ---
    // 為什麼要獨立一個子函式？
    //   8Color 的 share 就是「C share、M share、Y share 三張疊起來」：
    //     share[s](x,y) = ( C_share[s] 的 R, M_share[s] 的 G, Y_share[s] 的 B )
    //   每張色版只有「該通道 0 = 有墨」和「255 = 白」兩種值，
    //   三個通道各自獨立（可以同時有墨），合起來就是 8 色圖
    //   （2^3 = 8 種 R/G/B 組合）；疊起來（逐通道取 min = 「or」）則
    //   分別還原 a / b / c，合起來就是 SecretN (a,b,c)。
    //
    // 前提（很重要）：C / M / Y 三個模式必須「落在同一組座標、同一尺寸」。
    //   每個色版各自分享自己那個通道的值（C←a、M←b、Y←c），三個通道互不排斥；
    //   三張 share 由 createCMYKShares() 同一趟掃描產生，保證座標與尺寸一致。
    //   但「同一趟掃描」不等於「共用隨機數」：每個色版、每個像素各抽各的
    //   （aC / aM / aY），否則三色版花紋會逐格重合、等於共用同一組隨機位元。
    //
    // 只讀 C/M/Y 的 share、寫入 mutable 的 m_color8_shares，故為 const
    void createColor8Shares(Progress *progress) const;

    // 8Color 專用疊圖：把 C / M / Y 三張疊圖逐通道取 min 合成 8 色疊圖
    //   out = qRgb( min(R0,R1), min(G0,G1), min(B0,B1) )   ← 墨是 0、白是 255
    //   =「or」：任一張 share 在該通道有墨，結果就有墨
    // 只讀 share、寫入 mutable 的 m_color8_superimposed，故為 const
    void createColor8Superimposed() const;

    // 同一趟掃描產生 C / M / Y 三張色版的 share（共用座標與尺寸、共用一次進度回報），
    // 但「每個色版各自抽隨機數」（aC / aM / aY），三張花紋不會重合。
    //   createShares(Cyan / Magenta / Yellow) 只取其中一份，
    //   createShares(Color8) 則三份都拿來合併。
    // 寫入 m_C_shares / m_M_shares / m_Y_shares（mutable），故為 const
    void createCMYKShares(Progress *progress) const;

    // --- 各模式的「遮罩」判斷：把一個像素轉成 1 (有墨/黑) 或 0 (白/沒墨) ---
    // 為什麼要拆成四個小函式，而不是在 createShares() 裡用一個 bool 旗標切換？
    //   Binary 是「黑字白底」：黑 = 暗，所以 R <= 127 才算 1。
    //   C / M / Y 是「彩墨白底」：有墨時該通道是 0、沒墨時是 255，
    //   所以是「該通道 == 0 才算 1」，方向和 Binary 完全相反。
    //   各自一個函式後，createShares() 只要用成員函式指標挑一個，
    //   不用管比較方向，也就不會像 darkIsHigh 那樣寫反。
    // 注：Color8 不套遮罩（它是由 C/M/Y 合併而來，見 createColor8Shares()）。
    // 只讀傳入的 pixel、不碰成員，故為 const（static 也可，但這裡沿用成員函式指標的形式）
    int maskBinary(QRgb p) const;      // Binary：R <= 127 → 1（黑）
    int maskCyan(QRgb p) const;        // C 色版：R == 0 → 1（有青墨）
    int maskMagenta(QRgb p) const;     // M 色版：G == 0 → 1（有洋紅墨）
    int maskYellow(QRgb p) const;      // Y 色版：B == 0 → 1（有黃墨）

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
    int m_ratioRow = 2;     // 像素擴張量：垂直倍率 (2x2 → 水平、垂直都放大兩倍)
    int m_ratioCol = 2;     // 像素擴張量：水平倍率

    // mutable 允許在 const 函式裡改
    // 每個來源模式各自一份機密圖快取（各共 m_n 張），互不影響
    //
    // 命名依原本設計的注解：m_Binary_shares / m_8Color_shares / m_C_shares / m_M_shares / m_Y_shares。
    // 注：C++ 識別字不能以數字開頭，所以 8Color 寫成 m_8Color_shares 會編譯錯誤，
    //     因此保留語意但改成 m_color8_shares。
    mutable std::vector<QImage> m_Binary_shares;
    mutable std::vector<QImage> m_color8_shares;
    mutable std::vector<QImage> m_C_shares;
    mutable std::vector<QImage> m_M_shares;
    mutable std::vector<QImage> m_Y_shares;

    // 每個模式的「疊圖」快取（兩張 share 疊起來的結果），同樣第一次呼叫才計算
    // 命名跟在 share 快取後面，一看就知道是哪個模式的疊圖
    // 生命週期和同模式的 share 綁在一起：share 被清掉時這裡也要清（見 setSSConfig()）
    mutable QImage m_Binary_superimposed;
    mutable QImage m_color8_superimposed;
    mutable QImage m_C_superimposed;
    mutable QImage m_M_superimposed;
    mutable QImage m_Y_superimposed;
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

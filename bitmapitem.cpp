#include "bitmapitem.h"
#include "progress.h"

#include <QFileInfo>
#include <QRandomGenerator>
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

// C 色版：第一次呼叫才建立並快取 (m_cChannel 是 mutable)
QImage BitmapItem::getCChannel() const
{
    if (m_cChannel.isNull()) {
        createCChannel();
    }
    return m_cChannel;
}

// M 色版：第一次呼叫才建立並快取
QImage BitmapItem::getMChannel() const
{
    if (m_mChannel.isNull()) {
        createMChannel();
    }
    return m_mChannel;
}

// Y 色版：第一次呼叫才建立並快取
QImage BitmapItem::getYChannel() const
{
    if (m_yChannel.isNull()) {
        createYChannel();
    }
    return m_yChannel;
}

// 依模式取出對應的「疊圖」快取（五個模式互不影響）
// 寫法和 shareCacheOf() 一樣：把「挑哪一份」的 switch 集中在這裡。
// 注：這些成員都是 mutable，所以在 const 函式內取得可寫入引用是合法的。
// 放在檔案內（static）只是輔助函式，不需要動到 BitmapItem 的介面。
static QImage &superimposedOf(QImage &bin,
                              QImage &c8,
                              QImage &c,
                              QImage &m,
                              QImage &y,
                              ShareMode mode)
{
    switch (mode) {
    case ShareMode::Color8:  return c8;
    case ShareMode::Cyan:    return c;
    case ShareMode::Magenta: return m;
    case ShareMode::Yellow:  return y;
    case ShareMode::Binary:
    default:                 return bin;
    }
}

// 疊圖：第一次呼叫才建立並快取 (m_X_superimposed 是 mutable)
// 只拿「已經有的 share」來疊，不會自己去生成 share（沒 share 就回傳空圖）
QImage BitmapItem::getSuperimposed(ShareMode mode) const
{
    // 依模式挑出對應的疊圖快取；空的話才去算
    // （mutable 成員，所以在 const 函式裡取得可寫入引用是合法的）
    QImage &cache = superimposedOf(m_Binary_superimposed, m_color8_superimposed,
                                   m_C_superimposed, m_M_superimposed,
                                   m_Y_superimposed, mode);
    if (cache.isNull()) {
        createSuperimposed(mode);
    }
    return cache;
}

// ===== 版本目錄 (version catalog) =====
// 這裡是「畫面上能顯示哪些版本」的唯一來源 (single source of truth)。
// pictureList 的每一列 = 這個表格的一項，順序也一樣。
//
// 新增版本時：
//   1. 在 bitmapitem.h 宣告 getXXX() const（照下面 getGray() 的寫法：空才建立並快取）
//   2. 在 bitmapitem.cpp 實作 createXXX() + getXXX()
//   3. 在這裡加一行 { "顯示名稱", &BitmapItem::getXXX, "前綴", ShareMode::XXX }
// 不需要動 MainWindow。
//
// 欄位說明：
//   label        : pictureList 顯示的文字
//   getImage     : 該版本對應的 getXXX()
//   sharePrefix  : 這張圖當 share 來源時的名稱前綴（"Binary" → Binary_share0）；
//                  nullptr = 這個版本不能生成 share
//   shareMode    : 交給 getShares(mode) 的來源模式（sharePrefix 為 nullptr 時不使用）
//
// 為什麼 Original / Gray 是 nullptr？
//   ShareMode 目前只有 Binary / Color8 / Cyan / Magenta / Yellow 五種來源，
//   Original / Gray 沒有對應的 share 演算法（也還沒定義「哪裡算黑」），
//   所以 UI 會把它們的 shareList 清空、停用 Generate，而不是硬算一組錯的 share。
const std::vector<BitmapItemVersion> &BitmapItem::versions()
{
    // function static：第一次呼叫才建立，之後共用同一份（內容固定，不需加鎖）
    static const std::vector<BitmapItemVersion> table = {
        { "Original",  &BitmapItem::getOriginal,  nullptr,   ShareMode::Binary  },
        { "Gray",      &BitmapItem::getGray,      nullptr,   ShareMode::Binary  },
        { "Binary",    &BitmapItem::getBinary,    "Binary",  ShareMode::Binary  },
        { "8 Color",   &BitmapItem::getColor8,    "8Color",  ShareMode::Color8  },
        // C / M / Y 色版（印刷分色用，和上面的「版本」語意不同，但一樣可以顯示）
        { "C Channel", &BitmapItem::getCChannel,  "C",       ShareMode::Cyan    },
        { "M Channel", &BitmapItem::getMChannel,  "M",       ShareMode::Magenta },
        { "Y Channel", &BitmapItem::getYChannel,  "Y",       ShareMode::Yellow  },
    };
    return table;
}

int BitmapItem::versionCount()
{
    return static_cast<int>(versions().size());
}

// 依索引取圖：直接用成員函式指標呼叫，等於「這個版本對應的 getXXX()」
// 新增版本時這裡不用改（catalog 改了就會自動生效）
QImage BitmapItem::getImageAt(int index) const
{
    if (index < 0 || index >= versionCount()) {
        return QImage();        // 越界 → 空圖，顯示端會自然變成空白
    }
    return (this->*versions()[index].getImage)();
}

// ===== 版本 → share 的對應 =====
// 一樣只查 versions() 這張表，UI 不必知道任何模式名稱。
// 索引越界一律當成「不支援」，讓 UI 走「清空 shareList」那條路。

bool BitmapItem::supportsShares(int versionIndex)
{
    if (versionIndex < 0 || versionIndex >= versionCount()) {
        return false;
    }
    return versions()[versionIndex].sharePrefix != nullptr;
}

ShareMode BitmapItem::shareModeOf(int versionIndex)
{
    if (versionIndex < 0 || versionIndex >= versionCount()) {
        return ShareMode::Binary;       // 越界 → 預設值（呼叫端應先問 supportsShares）
    }
    return versions()[versionIndex].shareMode;
}

QString BitmapItem::sharePrefix(int versionIndex)
{
    if (versionIndex < 0 || versionIndex >= versionCount()) {
        return QString();               // 越界 → 空前綴
    }
    const char *prefix = versions()[versionIndex].sharePrefix;
    return prefix ? QString::fromUtf8(prefix) : QString();
}

// ===== 機密分享 =====

// 依模式取出對應的 share 快取 vector（五個模式互不影響）
// 四個 vector 都以引用傳入，回傳其中一個的引用，
// 把「挑哪一份」的 switch 集中在這裡。
// 注：這些成員都是 mutable，所以在 const 函式內取得可寫入引用是合法的。
static std::vector<QImage> &shareCacheOf(std::vector<QImage> &bin,
                                        std::vector<QImage> &c8,
                                        std::vector<QImage> &c,
                                        std::vector<QImage> &m,
                                        std::vector<QImage> &y,
                                        ShareMode mode)
{
    switch (mode) {
    case ShareMode::Color8:  return c8;
    case ShareMode::Cyan:    return c;
    case ShareMode::Magenta: return m;
    case ShareMode::Yellow:  return y;
    case ShareMode::Binary:
    default:                 return bin;
    }
}

std::vector<QImage> BitmapItem::getShares(ShareMode mode, Progress *progress,
                                          bool forceRegenerate) const
{
    // mutable 成員可以在 const 函式裡改，所以取得可寫入的引用
    std::vector<QImage> &cache = shareCacheOf(m_Binary_shares, m_color8_shares,
                                              m_C_shares, m_M_shares, m_Y_shares, mode);

    // Color8 的 share 是 C / M / Y 三色版合併出來的 → 只要它「其中一邊」要重抽，
    // 另一邊就必須跟著作廢，否則會把新舊兩組隨機位元混在一起（疊圖就還原不出 SecretN）。
    //   forceRegenerate = true 且 mode = Color8   → C / M / Y 一起重抽
    //   forceRegenerate = true 且 mode = C/M/Y    → Color8 一起重抽
    //   forceRegenerate = true 且 mode = Binary   → 與 Color8 無關，照舊
    // （Binary 是獨立的一份，不受影響。）
    const bool regenerateColor8 = forceRegenerate && mode == ShareMode::Color8;
    const bool regenerateCmy    = forceRegenerate
                                  && (mode == ShareMode::Cyan
                                      || mode == ShareMode::Magenta
                                      || mode == ShareMode::Yellow);

    if (forceRegenerate) {
        cache.clear();           // 丟掉該模式的舊快取 → 強制重抽

        // share 重抽了 → 同模式的疊圖也不再對應，一起作廢（下次 getSuperimposed() 重算）
        superimposedOf(m_Binary_superimposed, m_color8_superimposed,
                       m_C_superimposed, m_M_superimposed, m_Y_superimposed, mode) = QImage();

        // 上面的「連動清快取」：Color8 ↔ C/M/Y 互為來源，必須一起作廢
        if (regenerateColor8) {
            m_C_shares.clear();
            m_M_shares.clear();
            m_Y_shares.clear();
            m_C_superimposed = QImage();
            m_M_superimposed = QImage();
            m_Y_superimposed = QImage();
        } else if (regenerateCmy) {
            m_color8_shares.clear();
            m_color8_superimposed = QImage();
        }
    }
    if (cache.empty()) {
        createShares(mode, progress);   // progress 可為 nullptr → 不回報進度
    }
    return cache;
}

// 設定參數：只有真的改變時才動作，避免無謂地丟掉快取。
// 因為機密圖是由這組參數算出來的，所以任何一個參數變了，
// 先前產生的五份 m_XXX_shares 都必須作廢（清空 → 下次 getShares() 重算）。
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

    // 參數變了 → 五個模式的機密圖都不再對應現在的設定，全部清掉
    m_Binary_shares.clear();
    m_color8_shares.clear();
    m_C_shares.clear();
    m_M_shares.clear();
    m_Y_shares.clear();

    // 疊圖是由 share 疊出來的 → share 作廢，疊圖也一起作廢
    m_Binary_superimposed = QImage();
    m_color8_superimposed = QImage();
    m_C_superimposed      = QImage();
    m_M_superimposed      = QImage();
    m_Y_superimposed      = QImage();
}

// ===== 各模式的遮罩判斷（1 = 有墨/黑，0 = 白/沒墨）=====
// 每個函式只負責「這個像素算不算 1」，比較方向寫在各函式裡，
// createShares() 只需依模式挑一個函式指標，不必再管方向。
//
// 兩類語意完全不同，所以刻意不共用：
//   Binary / Color8 : 黑字白底 → 越暗越是墨      → qRed <= 127 → 1
//   C / M / Y       : 彩墨白底 → 該通道 0 是有墨 → 該通道 == 0 → 1
//   （注意 C 的有墨是 R = 0，和 Binary 的「R 很小才是黑」剛好反過來，
//     因為色版的兩個通道固定 255，只有「有墨」時該通道才會變 0）

int BitmapItem::maskBinary(QRgb p) const
{
    return (qRed(p) <= 127) ? 1 : 0;      // 灰階近似：R 很暗 → 黑
}

int BitmapItem::maskCyan(QRgb p) const
{
    return (qRed(p) == 0) ? 1 : 0;        // (0,255,255) 青墨 → 有墨
}

int BitmapItem::maskMagenta(QRgb p) const
{
    return (qGreen(p) == 0) ? 1 : 0;      // (255,0,255) 洋紅 → 有墨
}

int BitmapItem::maskYellow(QRgb p) const
{
    return (qBlue(p) == 0) ? 1 : 0;       // (255,255,0) 黃墨 → 有墨
}
void BitmapItem::createShares(ShareMode mode, Progress *progress) const
{
    if (m_original.isNull()) {
        return;              // 沒有原圖 → 無法產生，該模式的 shares 保持空
    }

    // ===== 特例：8 Color =====
    // 8Color 不自己算編碼矩陣，也不做「黑/白 mask」：
    //   它就是 C / M / Y 三個色版的 share 合併成一張（每通道 0/255 → 8 種顏色）。
    // 因此直接跳到專用子函式，下面「mask + C[a][s][i][j]」那條路完全不適用。
    if (mode == ShareMode::Color8) {
        createColor8Shares(progress);
        return;
    }

    // ===== C / M / Y：同一趟迴圈算完，但每個色版各自抽隨機數 =====
    // 為什麼放在一起算？
    //   8Color 的 share 是由這三張合併而來，三張必須落在同一組座標上（尺寸相同）。
    //   一起算只是共用一次掃描與一次進度回報，不代表共用隨機數：
    //   createCMYKShares() 內每個色版都各自抽 aC / aM / aY（三色版互不重合）。
    //   只取這次要的那一份。
    if (mode == ShareMode::Cyan || mode == ShareMode::Magenta
        || mode == ShareMode::Yellow) {
        createCMYKShares(progress);   // 一次算完 C / M / Y 三份（各有自己的亂數）
        return;                       // 三份都已寫進各自的快取，呼叫端自己挑要的那份
    }

    // ===== Binary：黑字白底，走原本的「遮罩 + 編碼矩陣」流程 =====
    // 1. 先建立編碼矩陣 C[a][s][i][j]
    //    參數不合法（ratioRow / ratioCol < 1 或 m 為奇數）→ 回傳空矩陣
    const auto C = buildEncodingMatrix(m_ratioRow, m_ratioCol);
    if (C.empty()) {
        return;
    }

    // 可能性數量 P = m! / ((m/2)! (m/2)!) = C(m, m/2)（m 格中挑 m/2 格填黑）
    //   m=2 → 2 種、m=4 → 6 種；就是編碼矩陣的第一維大小
    const int P = static_cast<int>(C.size());

    // 2. 先建立「原圖 ratioRow x ratioCol 倍大」的新圖（2 張 share）
    //    每個原圖像素會擴張成 ratioRow x ratioCol 的一塊
    std::vector<QImage> shares(
        2, QImage(m_width * m_ratioCol, m_height * m_ratioRow,
                  QImage::Format_RGB32));

    // Binary：讀 getBinary()，maskBinary() R <= 127 當黑，黑填 (0,0,0)
    //
    //   為什麼只有 Binary 走這裡？
    //     Binary 是「黑字白底」：黑 = 暗 → R <= 127 當 1。
    //     C / M / Y 是「彩墨白底」：有墨 → 該通道為 0、其餘通道 255，
    //     方向相反，而且要三色版一起掃描、各自抽隨機數（見 createCMYKShares()），
    //     所以各自走各自的路徑。
    //     Color8 更是完全不同：它是 C/M/Y 的合併（見 createColor8Shares()）。
    const QImage source = getBinary();             // 已快取，沒有就現算
    const QRgb inkColor = qRgb(0, 0, 0);           // v == 1 要填的顏色（黑）
    const QRgb white    = qRgb(255, 255, 255);     // v == 0 要填的顏色（白）

    QRandomGenerator *rng = QRandomGenerator::global();

    // 進度單位 = 原圖列數；total 只在這裡設一次，UI 端不必自己猜
    if (progress) {
        progress->setTotal(m_height);
    }

    // 3. 跑原圖每個像素；新圖上以 (ratioRow, ratioCol) 為間隔，填滿對應的一塊
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            // 判斷「1 = 黑, 0 = 白」
            const int v = maskBinary(source.pixel(x, y));

            // 這個像素隨機挑一種可能性 a (0 .. P-1)
            //   P = m! / ((m/2)! (m/2)!)，例如 m=4 → 6 種
            // 每個像素都重抽 → 相鄰像素樣式不同，不會變成固定棋盤格
            const int a = rng->bounded(P);

            // 填滿這塊 ratioRow x ratioCol 的格子
            for (int s = 0; s < 2; ++s) {
                // 白 (v=0) → 兩張 share 都取 C[a][0]
                // 黑 (v=1) → share0 取 C[a][0]、share1 取 C[a][1]（反轉）
                // 只有「黑 + share1」才用反轉那一張，其餘都是 C[a][0]
                const int t = v ? s : 0;

                for (int i = 0; i < m_ratioRow; ++i) {
                    for (int j = 0; j < m_ratioCol; ++j) {
                        const QRgb rgb = C[a][t][i][j]
                                       ? inkColor   // 1 = 黑/有墨 → 填模式對應的顏色
                                       : white;     // 0 = 白/沒墨
                        shares[s].setPixel(x * m_ratioCol + j,
                                           y * m_ratioRow + i, rgb);
                    }
                }
            }
        }

        // 這一列算完了 → 回報進度（done 從 1 數到 m_height）
        if (progress) {
            progress->report(y + 1);
        }
    }

    // 寫入 Binary 的快取 vector（mutable 成員）
    m_Binary_shares = std::move(shares);
}

// ===== C / M / Y 三色版（同一趟迴圈算完，但各色版「各自抽隨機數」）=====
// 為什麼三個色版放在同一趟迴圈算？
//   8Color 的 share 就是「C share + M share + Y share」三張合併起來，
//   三張必須落在同一組座標上、尺寸相同，合併才有意義。
//   同一趟迴圈只是為了「共用一次掃描 + 一次進度回報」，
//   不代表三個色版共用隨機數（見下面的 aC / aM / aY）。
//
// 為什麼隨機數必須分開？
//   若三色版共用同一個亂數 a，三張 share 的花紋會逐格完全重合，
//   任一張 share 上就同時看得到 C、M、Y 三個色版的位置，
//   等於三個色版用了同一組隨機位元 → 失去機密分享的獨立性。
//   因此每個色版、每個原圖像素都各自抽一次 bounded(P)：
//     aC 給 C、aM 給 M、aY 給 Y，彼此無關。
//   每個色版內部仍是「兩張 share 在同一格互補」（C[a][0] / C[a][1]），
//   所以單色版疊圖仍完全正確，8 Color 的合成也不受影響。
//
// 每張色版都只填「該通道」，負責 SecretN 的其中一個維度：
//   C share = (0,255,255) 青墨 / (255,255,255) 白   ← R 通道（a）
//   M share = (255,0,255) 洋紅 / (255,255,255) 白   ← G 通道（b）
//   Y share = (255,255,0) 黃墨 / (255,255,255) 白   ← B 通道（c）
//
// 也就是說：這是「三個通道各自做一次 (2,2) 黑白分享」，
//   疊圖時每格只會剩下「某一張 share 有墨的那一半」→ 該色版的墨色，
//   遠看就還原出該色版的 SecretN。
// 三個維度互相獨立，所以同一個位置可以三個通道同時有墨（疊圖就是深色）。
void BitmapItem::createCMYKShares(Progress *progress) const
{
    if (m_original.isNull()) {
        return;              // 沒有原圖 → 三張色版 share 保持空
    }

    // 1. 建立編碼矩陣：三色版共用同一組 C[a][s][i][j]
    //    參數不合法（ratioRow / ratioCol < 1 或 m 為奇數）→ 空矩陣
    const auto C = buildEncodingMatrix(m_ratioRow, m_ratioCol);
    if (C.empty()) {
        return;
    }
    const int P = static_cast<int>(C.size());

    // 2. 三張色版、各 2 張 share，尺寸都是原圖的 ratio 倍大
    const QSize size(m_width * m_ratioCol, m_height * m_ratioRow);
    const QImage blank(size, QImage::Format_RGB32);
    std::vector<QImage> cShares(2, blank);   // C 色版
    std::vector<QImage> mShares(2, blank);   // M 色版
    std::vector<QImage> yShares(2, blank);   // Y 色版

    const QRgb white = qRgb(255, 255, 255);   // 沒墨（三張色版的「白」都一樣）
    const QRgb cInk  = qRgb(0, 255, 255);     // 青墨
    const QRgb mInk  = qRgb(255, 0, 255);     // 洋紅
    const QRgb yInk  = qRgb(255, 255, 0);     // 黃墨

    // 讀哪張色版圖來判斷「這個原圖像素屬於哪個色版」：
    //   C 色版 = (R,255,255) → maskCyan()    R == 0 有墨
    //   M 色版 = (255,G,255) → maskMagenta() G == 0 有墨
    //   Y 色版 = (255,255,B) → maskYellow()  B == 0 有墨
    const QImage cSource = getCChannel();
    const QImage mSource = getMChannel();
    const QImage ySource = getYChannel();

    QRandomGenerator *rng = QRandomGenerator::global();

    if (progress) {
        progress->setTotal(m_height);
    }

    // 3. 逐像素：三個色版「各自」做一次 (2,2) 的 0/1 分享
    //    關鍵：每個色版只負責自己那個通道的值，彼此互不排斥
    //      C share 的 R ← maskCyan()    （a 這個維度）
    //      M share 的 G ← maskMagenta() （b 這個維度）
    //      Y share 的 B ← maskYellow()  （c 這個維度）
    //    三個色版各自抽隨機數（aC / aM / aY），所以三張的花紋互不相關；
    //    單看 C 的 share，無法推出 M / Y 的圖案。
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            // 這個原圖像素在三個色版各自「有沒有墨」：
            //   C 色版 = (R,255,255) → maskCyan()    R == 0
            //   M 色版 = (255,G,255) → maskMagenta() G == 0
            //   Y 色版 = (255,255,B) → maskYellow()  B == 0
            // 三者互相獨立（可以同時有墨，疊起來就是深色）
            const int vC = maskCyan(cSource.pixel(x, y));
            const int vM = maskMagenta(mSource.pixel(x, y));
            const int vY = maskYellow(ySource.pixel(x, y));

            // 這個像素「每個色版各自」隨機挑一種可能性 a（0 .. P-1）
            //
            // 為什麼不是三色版共用一個 a？
            //   共用一個 a 的話，三張 share 的花紋會「逐格完全重合」，
            //   也就是任一張 share 上都能同時看到 C、M、Y 三種圖案，
            //   等於三個色版用了同一組隨機數（失去機密分享的獨立性）。
            //   所以這裡各抽各的：aC / aM / aY 互相獨立，
            //   三色版的花紋不再同步，單看任一張 share 也拆不出另一個色版的圖案。
            //
            // 為什麼這樣 8 Color 還是能還原？
            //   8 Color 疊圖是「每格只留下一種色版的顏色」（見 createColor8Superimposed()），
            //   不是「三個通道各自還原 a/b/c」；
            //   每格留下哪個色版，取決於該格三個色版各自抽到什麼，
            //   三個色版互相獨立反而正是 8 色圖要的機率分佈。
            const int aC = rng->bounded(P);
            const int aM = rng->bounded(P);
            const int aY = rng->bounded(P);

            // 填滿這塊 ratioRow x ratioCol 的格子
            for (int s = 0; s < 2; ++s) {
                // 規則和 Binary 完全一樣：
                //   v = 0（白/沒墨）→ 兩張 share 都取 C[a][0]
                //   v = 1（有墨）   → share0 取 C[a][0]、share1 取 C[a][1]（反轉）
                // tX 是「這個色版在這張 share 上要用哪一個 bit 版本」：
                //   tC 用在 C 的 C[aC]，tM 用在 M 的 C[aM]，tY 用在 Y 的 C[aY]
                const int tC = vC ? s : 0;
                const int tM = vM ? s : 0;
                const int tY = vY ? s : 0;

                for (int i = 0; i < m_ratioRow; ++i) {
                    for (int j = 0; j < m_ratioCol; ++j) {
                        // 每個色版各自看「自己抽到的那一組」C[a][t][i][j]：
                        //   1 = 有墨 → 填該色版的墨色；0 = 白
                        const QRgb crgb = C[aC][tC][i][j] ? cInk : white;   // C：R = 0
                        const QRgb mrgb = C[aM][tM][i][j] ? mInk : white;   // M：G = 0
                        const QRgb yrgb = C[aY][tY][i][j] ? yInk : white;   // Y：B = 0

                        const int px = x * m_ratioCol + j;
                        const int py = y * m_ratioRow + i;
                        cShares[s].setPixel(px, py, crgb);
                        mShares[s].setPixel(px, py, mrgb);
                        yShares[s].setPixel(px, py, yrgb);
                    }
                }
            }
        }

        // 這一列算完了 → 回報進度（三色版一起算，所以只回報一次）
        if (progress) {
            progress->report(y + 1);
        }
    }

    // 4. 三份一起寫回快取（createShares(Cyan/Magenta/Yellow) 只取自己要的那一份，
    //    createShares(Color8) 三份都拿來合併）
    m_C_shares = std::move(cShares);
    m_M_shares = std::move(mShares);
    m_Y_shares = std::move(yShares);
}

// ===== 8 Color：C / M / Y 三色版合併 =====
// 8Color 的 share 不需要自己的編碼矩陣，它就是三張色版 share 逐像素合併：
//   share[s](px,py) = ( C_share[s] 的 R, M_share[s] 的 G, Y_share[s] 的 B )
// 也就是 (a, b, c) 三個通道各自拿一張色版的結果，合起來就是 8 色圖。
//
// 疊圖時逐通道取 min（墨 0、白 255 → 「or」）。8 色圖每一格只可能是
// 「某一個色版有墨（例如只有 C → (0,255,255)）」或「三個都有墨 + 白」，
// 而不是三個色版各自獨立還原 → 見 createColor8Superimposed()。
void BitmapItem::createColor8Shares(Progress *progress) const
{
    if (m_original.isNull()) {
        return;              // 沒有原圖 → m_color8_shares 保持空
    }

    // 1. 確保 C / M / Y 三色版的 share 都已存在
    //    （沒有就現算；三張同一次掃描產生，但每個色版各自抽隨機數，
    //      見 createCMYKShares()）
    const std::vector<QImage> cShares = getShares(ShareMode::Cyan, progress);
    const std::vector<QImage> mShares = getShares(ShareMode::Magenta, progress);
    const std::vector<QImage> yShares = getShares(ShareMode::Yellow, progress);
    if (cShares.size() < 2 || mShares.size() < 2 || yShares.size() < 2) {
        return;              // 參數不合法（例如 col × row 為奇數）→ 三色版是空的
    }

    // 2. 逐像素合併：C 取 R、M 取 G、Y 取 B（三張尺寸相同，以 C 為準）
    //
    // 為什麼 C / M / Y 三色版可以各自抽隨機數，合併後的 8 色圖仍然正確？
    //   8 色圖在疊圖時不是「三個通道各自還原 a/b/c」，
    //   而是「每一格最後只留下一種顏色」（見 createColor8Superimposed()）。
    //   格子裡留下哪一種顏色，取決於「哪一個色版剛好抽到『互補』的那一組」，
    //   機率由三個色版各自的墨量決定 —— 那正是 8 色圖要的比例。
    //   因此三張 share 的花紋彼此獨立並不影響 8 色圖的統計特性。
    //
    // 為什麼一定要「三張一起算」？
    //   若只重抽其中一個色版（例如只按 C 的 Generate），
    //   另外兩個色版還是舊的那一批 → 合成出來的 8 色 share 會混到舊位元。
    //   getShares(Color8) 已把「重抽任一色版就作廢 8Color」寫進連動清表，
    //   所以這裡可以放心直接取三份快取。
    std::vector<QImage> shares(
        2, QImage(cShares[0].width(), cShares[0].height(), QImage::Format_RGB32));

    for (int s = 0; s < 2; ++s) {
        const QImage &cs = cShares[s];
        const QImage &ms = mShares[s];
        const QImage &ys = yShares[s];

        for (int py = 0; py < cs.height(); ++py) {
            for (int px = 0; px < cs.width(); ++px) {
                shares[s].setPixel(px, py,
                                   qRgb(qRed(cs.pixel(px, py)),     // C 色版的 R
                                        qGreen(ms.pixel(px, py)),   // M 色版的 G
                                        qBlue(ys.pixel(px, py))));  // Y 色版的 B
            }
        }
    }

    // 3. 寫回 8Color 的快取（mutable 成員）
    m_color8_shares = std::move(shares);
}


// ===== 疊圖 (superimposed) =====
// 把同一個模式的兩張 share 疊起來，得到一張「疊合後」的影像
// （shareList 的 <prefix>_superimposed 那一列）：
//
//   A = share0 在該點的顏色、B = share1 在該點的顏色
//   (A == inkColor || B == inkColor) ? inkColor : 白 (255,255,255)
//
//   也就是「任一張有墨 → 有墨；兩張都白 → 白」。
//   注意 A、B 不是用遮罩函式判斷，而是直接比對顏色：
//   createShares() 只會填 inkColor(墨) 和 white(白) 兩種顏色，
//   所以「等於 inkColor」就等於「這張 share 在該點有墨」，
//   不需要、也不應該再套一次 maskBinary() 那類門檻判斷。
//
// 前提：只疊「已經存在的 share」
//   會用 getShares(mode) 取得 share，但本函式不會讓它重新生成：
//     - 還沒按 Generate       → share 是空的 → 回傳空圖（顯示端空白）
//     - col × row 為奇數     → 編碼矩陣是空的 → share 是空的 → 回傳空圖
//   這種情況下 m_X_superimposed 保持 isNull，下次呼叫會再試一次。
//
// 為什麼要開一個快取成員？
//   疊圖是「兩張 share 逐像素比較」算出來的，share 不變答案就不變，
//   所以算一次存起來（mutable），切換清單選取時不必重算。
void BitmapItem::createSuperimposed(ShareMode mode) const
{
    // 8 Color 是特例：share 有 8 種顏色，不能比對 inkColor，
    // 要改成「三個通道分別取 min（墨 0、白 255，即 or）」→ 見 createColor8Superimposed()
    if (mode == ShareMode::Color8) {
        createColor8Superimposed();
        return;
    }

    // 取得該模式的 share（已快取；沒有 share 時是空的，不會觸發生成）
    const std::vector<QImage> shares = getShares(mode);
    if (shares.size() < 2 || shares[0].isNull() || shares[1].isNull()) {
        return;              // 沒有兩張 share → 疊不起來，快取保持 isNull
    }

    const QImage &s0 = shares[0];      // A：第 1 張 share
    const QImage &s1 = shares[1];      // B：第 2 張 share

    // 依模式決定「什麼顏色算有墨」，與 createShares() 填的 inkColor 完全一致：
    //   Binary → 黑 (0,0,0)
    //   Cyan   → 青墨 (0,255,255)
    //   Magenta→ 洋紅 (255,0,255)
    //   Yellow → 黃墨 (255,255,0)
    // （Color8 已在上面轉走）
    QRgb inkColor = qRgb(0, 0, 0);
    switch (mode) {
    case ShareMode::Cyan:    inkColor = qRgb(0, 255, 255);    break;
    case ShareMode::Magenta: inkColor = qRgb(255, 0, 255);    break;
    case ShareMode::Yellow:  inkColor = qRgb(255, 255, 0);    break;
    case ShareMode::Color8:
    case ShareMode::Binary:
    default:                 inkColor = qRgb(0, 0, 0);        break;
    }

    const QRgb white = qRgb(255, 255, 255);   // 兩張都沒有墨 → 白

    // 兩張 share 尺寸相同（同一次 createShares() 產生），以第 1 張為準
    QImage out(s0.width(), s0.height(), QImage::Format_RGB32);

    for (int y = 0; y < s0.height(); ++y) {
        for (int x = 0; x < s0.width(); ++x) {
            const QRgb a = s0.pixel(x, y);      // A
            const QRgb b = s1.pixel(x, y);      // B

            // (A == inkColor || B == inkColor) ? inkColor : 白
            const bool hasInk = (a == inkColor) || (b == inkColor);
            out.setPixel(x, y, hasInk ? inkColor : white);
        }
    }

    // 寫入對應模式的疊圖快取（五個模式互不影響，mutable 成員）
    superimposedOf(m_Binary_superimposed, m_color8_superimposed,
                   m_C_superimposed, m_M_superimposed, m_Y_superimposed, mode) = out;
}

// ===== 8 Color 的疊圖 =====
// 8Color 的 share 每個通道各自代表一個色版（C 看 R、M 看 G、Y 看 B），
// 所以疊圖不能比對「整顆像素的顏色」，要逐通道取 min：
//
//   墨 = 0、白 = 255 → min 就等於「or」
//   out = qRgb( min(A.R, B.R), min(A.G, B.G), min(A.B, B.B) )
//
// 為什麼這樣會還原出 SecretN？
//   同一塊格子裡，某個位置一定是「一張 share 有墨、另一張是白」，
//   所以每個位置疊完只會剩下「某一個色版」（例如 (0,255,255) 青）。
//   整塊 ratioRow × ratioCol 疊完 → 遠看就是 SecretN 的 (a,b,c)。
void BitmapItem::createColor8Superimposed() const
{
    // 取得 8Color 的 share（已快取；沒有 share 時是空的，不會觸發生成）
    const std::vector<QImage> shares = getShares(ShareMode::Color8);
    if (shares.size() < 2 || shares[0].isNull() || shares[1].isNull()) {
        return;              // 沒有兩張 share → 疊不起來，快取保持 isNull
    }

    const QImage &s0 = shares[0];      // A：第 1 張 share
    const QImage &s1 = shares[1];      // B：第 2 張 share

    // 兩張 share 尺寸相同（同一次 createColor8Shares() 產生），以第 1 張為準
    QImage out(s0.width(), s0.height(), QImage::Format_RGB32);

    for (int y = 0; y < s0.height(); ++y) {
        for (int x = 0; x < s0.width(); ++x) {
            const QRgb a = s0.pixel(x, y);
            const QRgb b = s1.pixel(x, y);

            // 以 RGB 數值視角（光 255、墨 0）：逐通道取 min，等同於邏輯「AND」（兩者皆透光才顯色）
            const int r = std::min(qRed(a),   qRed(b));
            const int g = std::min(qGreen(a), qGreen(b));
            const int bl = std::min(qBlue(a), qBlue(b));

            out.setPixel(x, y, qRgb(r, g, bl));
        }
    }

    // 寫入 8Color 的疊圖快取（mutable 成員）
    m_color8_superimposed = out;
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
        return;              // 沒有原圖 → 無法產生，m_binary 保持 isNull
    }

    // 輸出用 Format_RGB32：不用調色盤，每個像素自己填 RGB。
    // 視覺上只有黑白兩色，但格式仍是 32-bit 彩色圖。
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

// ===== C / M / Y 色版 =====
// 來源是 8 色圖 m_color8（不是 m_original，也不是 m_binary）：
//   create8Color() 已把每個像素的 R / G / B 各自以 127 為門檻二值化成 0 或 255，
//   所以每像素只有 8 種顏色，且 m_color8 本身就已經是很單純的色版來源。
//
// 取法：直接取對應通道的值，另外兩個通道固定 255（不做反相）：
//   C = (R, 255, 255)、M = (255, G, 255)、Y = (255, 255, B)
//   因為 m_color8 的 R/G/B 只有 0 / 255，所以每張色版也只有兩種顏色：
//     該通道為 0   → 該色版「有墨」   → (0,255,255) / (255,0,255) / (255,255,0)
//     該通道為 255 → 該色版「沒有墨」 → 整張白 (255,255,255)
//   判斷「有墨 / 沒墨」時直接看對應通道是否為 0 即可。
//
// 三張色版都是白底 + 單一墨色，可以直接丟進現有的黑白 secret sharing
// （createShares() 再把「有墨」對應成填色、「沒墨」對應成白色）。

void BitmapItem::createCChannel() const
{
    if (m_original.isNull()) {
        return;              // 沒有原圖 → 無法產生，m_cChannel 保持 isNull
    }

    // 來源是 8 色圖；還沒算過就現算（會寫入 m_color8 快取）
    const QImage color8 = getColor8();

    // 輸出用 Format_RGB32：不用調色盤，每個像素自己填 RGB。
    QImage out(m_width, m_height, QImage::Format_RGB32);

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const QRgb rgb8 = color8.pixel(x, y);
            const int  c = qRed(rgb8);                 // 青版 = 直接取 R (0 或 255)

            out.setPixel(x, y, qRgb(c, 255, 255));      // 青墨 (0,255,255)，沒有墨時全白
        }
    }

    // 寫入快取 (m_cChannel 是 mutable)
    m_cChannel = out;
}

void BitmapItem::createMChannel() const
{
    if (m_original.isNull()) {
        return;              // 沒有原圖 → 無法產生，m_mChannel 保持 isNull
    }

    // 來源是 8 色圖；還沒算過就現算（會寫入 m_color8 快取）
    const QImage color8 = getColor8();

    // 輸出用 Format_RGB32：不用調色盤，每個像素自己填 RGB。
    QImage out(m_width, m_height, QImage::Format_RGB32);

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const QRgb rgb8 = color8.pixel(x, y);
            const int  m = qGreen(rgb8);               // 洋紅版 = 直接取 G (0 或 255)

            out.setPixel(x, y, qRgb(255, m, 255));      // 洋紅墨 (255,0,255)，沒有墨時全白
        }
    }

    // 寫入快取 (m_mChannel 是 mutable)
    m_mChannel = out;
}

void BitmapItem::createYChannel() const
{
    if (m_original.isNull()) {
        return;              // 沒有原圖 → 無法產生，m_yChannel 保持 isNull
    }

    // 來源是 8 色圖；還沒算過就現算（會寫入 m_color8 快取）
    const QImage color8 = getColor8();

    // 輸出用 Format_RGB32：不用調色盤，每個像素自己填 RGB。
    QImage out(m_width, m_height, QImage::Format_RGB32);

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const QRgb rgb8 = color8.pixel(x, y);
            // 變數名用 yv：y 已經被迴圈變數（列座標）用掉了
            const int  yv = qBlue(rgb8);               // 黃版 = 直接取 B (0 或 255)

            out.setPixel(x, y, qRgb(255, 255, yv));     // 黃墨 (255,255,0)，沒有墨時全白
        }
    }

    // 寫入快取 (m_yChannel 是 mutable)
    m_yChannel = out;
}

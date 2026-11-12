// main_cli.cpp
// CLI 測試程式：用 BitmapItem 載入輸入的 BMP，
// 並輸出 gray / binary / color8 三種衍生檔案，以及 2 張機密分享 (share0 / share1)。
//
// 用法：
//   ./SecretSharingCLI input.bmp
//   ./SecretSharingCLI input.bmp --ss <ratioCol> <ratioRow>
//   ./SecretSharingCLI --dump-matrix <ratioCol> <ratioRow>
//
// 會在「輸入檔所在目錄」產生六個 PNG：
//   gray.png         (灰階)
//   binary.png       (二值)
//   color8.png       (8 色)
//   share0.png       (第 1 張機密分享)
//   share1.png       (第 2 張機密分享)
//   share2.png       (疊圖：share0 與 share1 疊起來，等同 GUI 的 <prefix>_superimposed)
//
// --ss 指定機密分享的像素擴張量，參數順序為 <ratioCol> <ratioRow>（水平，垂直），
// 預設為 BitmapItem 的預設值 2 x 1（水平 2、垂直 1）。
// --share-mode 指定 share 的來源模式：binary（預設）/ color8 / c / m / y。
// --dump-matrix 則只印出編碼矩陣 C[a][s][i][j] 的所有可能性（2D 樣式），不讀圖檔。
//
// 用 QCoreApplication：CLI 只要有 QtCore + QtGui，不需要視窗系統。

#include "bitmapitem.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QString>
#include <QTextStream>

#include <vector>

namespace {

// 印出編碼矩陣：每一種可能性 a 印成一組 2D 樣式，兩張 share 並排
//   例：ratioCol=2, ratioRow=2
//     a=0  share0    share1
//          00  11
//          11  00
void dumpEncodingMatrix(QTextStream &out, int ratioCol, int ratioRow)
{
    const auto C = buildEncodingMatrix(ratioRow, ratioCol);

    out << "ratio: " << ratioCol << " x " << ratioRow
        << "   (m = " << ratioRow * ratioCol << ")\n"
        << "patterns: " << C.size() << "\n\n";

    if (C.empty()) {
        out << QStringLiteral("(empty matrix: ratioRow/ratioCol 必須 >= 1，且 ratioRow*ratioCol 必須是偶數)\n");
        return;
    }

    // 每一張 share 在畫面上佔的欄寬：數字 ratioCol 欄 + 前方縮排
    const QString indent = QStringLiteral("  ");
    const QString gap    = QStringLiteral("    ");

    for (std::size_t a = 0; a < C.size(); ++a) {
        out << "a=" << int(a) << "\n";

        // 標題列：每張 share 一個標籤，寬度與下方數字對齊
        QStringList titles;
        for (std::size_t s = 0; s < C[a].size(); ++s) {
            QString label = QStringLiteral("share %1").arg(int(s));
            label = label.leftJustified(ratioCol, QLatin1Char(' '));
            titles << label;
        }
        out << indent << titles.join(gap) << "\n";

        // 逐列印出：share 並排
        for (int i = 0; i < ratioRow; ++i) {
            QStringList cols;
            for (std::size_t s = 0; s < C[a].size(); ++s) {
                QString bits;
                for (int j = 0; j < ratioCol; ++j)
                    bits += (C[a][s][i][j] ? QLatin1Char('1') : QLatin1Char('0'));
                cols << bits;
            }
            out << indent << cols.join(gap) << "\n";
        }
        out << "\n";
    }
}

// 字串 -> ShareMode；不認識就回傳 false（讓 main 印錯誤）
bool parseShareMode(const QString &text, ShareMode &mode)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("binary"))  { mode = ShareMode::Binary;  return true; }
    if (t == QLatin1String("color8"))  { mode = ShareMode::Color8;  return true; }
    if (t == QLatin1String("c") || t == QLatin1String("cyan"))    { mode = ShareMode::Cyan;    return true; }
    if (t == QLatin1String("m") || t == QLatin1String("magenta")) { mode = ShareMode::Magenta; return true; }
    if (t == QLatin1String("y") || t == QLatin1String("yellow"))  { mode = ShareMode::Yellow;  return true; }
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    // Qt5 的 QTextStream 預設用系統 locale（在 UTF-8 終端機會變亂碼），
    // 明確指定 UTF-8 才能正確輸出中文。（Qt6 預設就是 UTF-8，不需設定）
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
    err.setCodec("UTF-8");
#endif

    const QStringList args = QCoreApplication::arguments();

    // --- --dump-matrix <ratioCol> <ratioRow> ---
    if (args.size() >= 2 && args.at(1) == QLatin1String("--dump-matrix")) {
        if (args.size() < 4) {
            err << "Usage: " << QFileInfo(args.value(0)).fileName()
                << " --dump-matrix <ratioCol> <ratioRow>\n";
            return 2;
        }

        bool okCol = false;
        bool okRow = false;
        const int ratioCol = args.at(2).toInt(&okCol);
        const int ratioRow = args.at(3).toInt(&okRow);
        if (!okRow || !okCol) {
            err << "Error: ratioCol / ratioRow 必須是整數\n";
            return 2;
        }

        dumpEncodingMatrix(out, ratioCol, ratioRow);
        return 0;
    }

    if (args.size() < 2) {
        err << "Usage: " << QFileInfo(args.value(0)).fileName()
            << " <input.bmp> [--ss <ratioCol> <ratioRow>]\n"
            << "       " << QFileInfo(args.value(0)).fileName()
            << " --dump-matrix <ratioCol> <ratioRow>\n";
        return 2;
    }

    const QString inPath = args.at(1);
    BitmapItem item(inPath);

    if (!item.isValid()) {
        err << "Error: cannot load image: " << inPath << "\n";
        return 1;
    }

    // --- 可選的 --ss <ratioCol> <ratioRow>：指定機密分享的像素擴張量 ---
    const int ssPos = args.indexOf(QLatin1String("--ss"));
    if (ssPos >= 0) {
        if (ssPos + 2 >= args.size()) {
            err << "Usage: " << QFileInfo(args.value(0)).fileName()
                << " <input.bmp> --ss <ratioCol> <ratioRow>\n";
            return 2;
        }

        bool okCol = false;
        bool okRow = false;
        const int ratioCol = args.at(ssPos + 1).toInt(&okCol);
        const int ratioRow = args.at(ssPos + 2).toInt(&okRow);
        if (!okRow || !okCol) {
            err << "Error: ratioCol / ratioRow 必須是整數\n";
            return 2;
        }

        // n = 2, k = 1（目前只支援 2 張 share）
        item.setSSConfig(2, 1, ratioRow, ratioCol);
    }

    // --- 可選的 --share-mode <mode>：指定 share 的來源模式 ---
    ShareMode shareMode = ShareMode::Binary;   // 預設：行為與改動前相同
    const int smPos = args.indexOf(QLatin1String("--share-mode"));
    if (smPos >= 0) {
        if (smPos + 1 >= args.size() || !parseShareMode(args.at(smPos + 1), shareMode)) {
            err << "Error: --share-mode 只接受 binary / color8 / c / m / y\n";
            return 2;
        }
    }

    out << "Loaded: " << item.getName() << "\n"
        << "Path:   " << item.getFilePath() << "\n"
        << "Size:   " << item.getWidth() << "x" << item.getHeight() << "\n";

    // 輸出目錄 = 輸入檔所在目錄（相對路徑時 QFileInfo 會保留空目錄 → 用 ".")
    QString dir = QFileInfo(inPath).absolutePath();
    if (dir.isEmpty())
        dir = QDir::currentPath();

    // 機密分享（2 張）：只計算一次，兩張一起輸出
    const std::vector<QImage> shares = item.getShares(shareMode);

    const struct {
        const char *fileName;
        QImage      image;
    } outputs[] = {
        { "gray.png",   item.getGray()   },
        { "binary.png", item.getBinary() },
        { "color8.png", item.getColor8() },
        { "share0.png", shares.size() > 0 ? shares[0] : QImage() },
        { "share1.png", shares.size() > 1 ? shares[1] : QImage() },
        // share2.png = 疊圖（GUI 的 <prefix>_superimposed）：
        //   (share0 == inkColor || share1 == inkColor) ? inkColor : 白
        // 用 getSuperimposed() 算，和 GUI 走同一條路徑，方便對照
        { "share2.png", item.getSuperimposed(shareMode) },
    };

    bool ok = true;
    for (const auto &o : outputs) {
        const QString path = dir + QLatin1Char('/') + QLatin1String(o.fileName);

        if (o.image.isNull()) {
            err << "Warning: image is null, skip: " << o.fileName << "\n";
            ok = false;
            continue;
        }

        if (!o.image.save(path)) {
            err << "Error: failed to save: " << path << "\n";
            ok = false;
            continue;
        }
        out << "Saved:  " << path << "  (" << o.image.width() << "x"
            << o.image.height() << ")\n";
    }

    return ok ? 0 : 1;
}

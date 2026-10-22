// main_cli.cpp
// CLI 測試程式：用 BitmapItem 載入輸入的 BMP，
// 並輸出 gray / binary / color8 三種衍生檔案。
//
// 用法：
//   ./SecretSharingCLI input.bmp
//   ./SecretSharingCLI --dump-matrix <ratioRow> <ratioCol>
//
// 會在「輸入檔所在目錄」產生三個 PNG：
//   gray.png    (灰階)
//   binary.png  (二值)
//   color8.png  (8 色)
//
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

namespace {

// 印出編碼矩陣：每一種可能性 a 印成一組 2D 樣式，兩張 share 並排
//   例：ratioRow=2, ratioCol=2
//     a=0  share0    share1
//          00  11
//          11  00
void dumpEncodingMatrix(QTextStream &out, int ratioRow, int ratioCol)
{
    const auto C = buildEncodingMatrix(ratioRow, ratioCol);

    out << "ratio: " << ratioRow << " x " << ratioCol
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

    // --- --dump-matrix <ratioRow> <ratioCol> ---
    if (args.size() >= 2 && args.at(1) == QLatin1String("--dump-matrix")) {
        if (args.size() < 4) {
            err << "Usage: " << QFileInfo(args.value(0)).fileName()
                << " --dump-matrix <ratioRow> <ratioCol>\n";
            return 2;
        }

        bool okRow = false;
        bool okCol = false;
        const int ratioRow = args.at(2).toInt(&okRow);
        const int ratioCol = args.at(3).toInt(&okCol);
        if (!okRow || !okCol) {
            err << "Error: ratioRow / ratioCol 必須是整數\n";
            return 2;
        }

        dumpEncodingMatrix(out, ratioRow, ratioCol);
        return 0;
    }

    if (args.size() < 2) {
        err << "Usage: " << QFileInfo(args.value(0)).fileName()
            << " <input.bmp>\n"
            << "       " << QFileInfo(args.value(0)).fileName()
            << " --dump-matrix <ratioRow> <ratioCol>\n";
        return 2;
    }

    const QString inPath = args.at(1);
    BitmapItem item(inPath);

    if (!item.isValid()) {
        err << "Error: cannot load image: " << inPath << "\n";
        return 1;
    }

    out << "Loaded: " << item.getName() << "\n"
        << "Path:   " << item.getFilePath() << "\n"
        << "Size:   " << item.getWidth() << "x" << item.getHeight() << "\n";

    // 輸出目錄 = 輸入檔所在目錄（相對路徑時 QFileInfo 會保留空目錄 → 用 ".")
    QString dir = QFileInfo(inPath).absolutePath();
    if (dir.isEmpty())
        dir = QDir::currentPath();

    const struct {
        const char *fileName;
        QImage      image;
    } outputs[] = {
        { "gray.png",   item.getGray()   },
        { "binary.png", item.getBinary() },
        { "color8.png", item.getColor8() },
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

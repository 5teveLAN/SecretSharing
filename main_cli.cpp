// main_cli.cpp
// CLI 測試程式：用 BitmapItem 載入輸入的 BMP，
// 並輸出 gray / binary / color8 三種衍生檔案。
//
// 用法：
//   ./SecretSharingCLI input.bmp
//
// 會在「輸入檔所在目錄」產生三個 PNG：
//   gray.png    (灰階)
//   binary.png  (二值)
//   color8.png  (8 色)
//
// 用 QCoreApplication：CLI 只要有 QtCore + QtGui，不需要視窗系統。

#include "bitmapitem.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QString>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 2) {
        err << "Usage: " << QFileInfo(args.value(0)).fileName()
            << " <input.bmp>\n";
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

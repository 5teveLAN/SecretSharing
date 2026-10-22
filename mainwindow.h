#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>

#include "bitmapitem.h"

class QGraphicsPixmapItem;
class QResizeEvent;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // --- auto-connect：名稱必須是 on_<控件名>_<signal名> ---

    // 左側：載入照片按鈕（一次載入一張）
    void on_loadButton_clicked();

    // 左側：在清單點選某個版本 → 呼叫對應的 getXXX() 並顯示
    void on_pictureList_itemSelectionChanged();

    // 左側：在 Shares 清單點選某一張 share → 顯示第 m_shareIndex 張
    void on_shareList_itemSelectionChanged();

    // 左側：生成機密圖（依 col / row 編輯格的值）
    void on_generateButton_clicked();

private:
    // 依 m_viewMode（+ m_shareIndex）取得對應影像並畫到右側
    void showCurrentImage();

    // 把顯示中的圖片依右側可視範圍等比縮放 (fitInView)
    void fitImageToView();

    // 視窗 / 分割線尺寸改變時，讓圖片跟著放大縮小
    void resizeEvent(QResizeEvent *event) override;

    // 建立清單內容：列固定 4 個版本（建構子呼叫一次即可）
    void initList();

    // 建立 Shares 清單：固定 kShareCount 列（建構子呼叫一次即可）
    void initShareList();

    // 依 m_viewMode 把清單選取切到對應那一列
    void selectRow();

    // 依 m_shareIndex 把 Shares 清單選取切到對應那一列
    void selectShareRow();

    // 依 col / row 編輯格重新產生機密圖，並切到第 0 張
    void generateShares();

    // 沒有可用的機密圖 → 重設 share 的選取狀態
    void clearShares();

    // 目前這張圖的各個版本（pictureList 每一列各代表一種；Share 由 shareList 決定第幾張）
    enum class ViewMode { Original, Gray, Binary, Color8, Share };

    // 目前固定 2 張 share（與 BitmapItem::create2Shares() 一致）
    static constexpr int kShareCount = 2;

    Ui::MainWindow *ui;

    // 右側用 QGraphicsView + QGraphicsPixmapItem 顯示圖片：
    // scene 固定只放一個 pixmap item，縮放交給 view 的 transform 處理。
    QGraphicsScene      *m_scene      = nullptr;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;

    // 代表哪一張 bmp 檔
    BitmapItem *m_current = nullptr;
    // 代表那張 bmp 檔的 哪一個版本
    // 兩者結合可以拿到 QImage
    ViewMode    m_viewMode = ViewMode::Original;

    // 只有 m_viewMode == Share 時有意義：第幾張 share (0 .. kShareCount-1)
    int         m_shareIndex = 0;
};

#endif // MAINWINDOW_H

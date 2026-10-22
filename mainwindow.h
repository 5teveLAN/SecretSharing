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

private:
    // 依 m_viewMode 取得對應影像並畫到右側
    void showCurrentImage();

    // 把顯示中的圖片依右側可視範圍等比縮放 (fitInView)
    void fitImageToView();

    // 視窗 / 分割線尺寸改變時，讓圖片跟著放大縮小
    void resizeEvent(QResizeEvent *event) override;

    // 建立清單內容：列固定 4 個版本（建構子呼叫一次即可）
    void initList();

    // 依 m_viewMode 把清單選取切到對應那一列
    void selectRow();

    // 目前這張圖的各個版本（清單裡的每一列各代表一種）
    enum class ViewMode { Original, Gray, Binary, Color8 };

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
};

#endif // MAINWINDOW_H

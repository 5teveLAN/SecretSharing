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

    // 建立清單內容：列數與文字全部來自 BitmapItem::versions()（建構子呼叫一次）
    void initList();

    // 建立 / 重建 Shares 清單（依目前選的版本決定文字與列數）
    //   支援 share 的版本 → kShareCount 列：Binary_share0 / C_share0 …
    //   不支援 share 的版本（Original / Gray）→ 清單清空
    // 注意：可重複呼叫（切換版本時會再叫一次）。
    void initShareList();

    // 目前選的版本能不能生成 share（Original / Gray → false）
    bool currentVersionSupportsShares() const;

    // 依 m_viewMode 把清單選取切到對應那一列
    void selectRow();

    // 依版本索引找出 pictureList 的列號（找不到回傳 -1）
    // 列號與版本索引分開處理，不再假設兩者相等
    int rowOfVersion(int versionIndex) const;

    // 依 m_shareIndex 把 Shares 清單選取切到對應那一列
    void selectShareRow();

    // 依 col / row 編輯格重新產生機密圖，並切到第 0 張
    void generateShares();

    // 沒有可用的機密圖 → 重設 share 的選取狀態
    void clearShares();

    // 進度條回到「尚未生成」的狀態
    void resetProgress();

    // 目前這張圖的各個版本（pictureList 每一列各代表一種；Share 由 shareList 決定第幾張）
    //
    // 注意：版本本身不再寫在這裡 —— 有哪幾種版本由 BitmapItem::versions() 決定，
    //       pictureList 直接照那張表建立，新增版本時這個檔案不用改。
    enum class ViewMode { Version, Share };

    // 目前固定 2 張 share（與 BitmapItem::createShares() 一致）
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
    ViewMode    m_viewMode = ViewMode::Version;

    // 只有 m_viewMode == Version 時有意義：
    // 第幾個版本 = BitmapItem::versions() 的索引 = pictureList 的列號
    int         m_versionIndex = 0;

    // 目前選的版本對應的 share 來源模式
    //   → 決定 shareList 的文字（Binary_share0 / C_share0 …）
    //   → 決定 generateShares() / showCurrentImage() 要跟 getShares() 要哪一種
    // 由 m_versionIndex 透過 BitmapItem::shareModeOf() 同步，不讓 UI 自己判斷。
    ShareMode   m_shareMode = ShareMode::Binary;

    // 只有 m_viewMode == Share 時有意義：第幾張 share (0 .. kShareCount-1)
    int         m_shareIndex = 0;
};

#endif // MAINWINDOW_H

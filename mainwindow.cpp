#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QListWidgetItem>
#include <QPixmap>
#include <QResizeEvent>

// 註：按鈕 / 清單的連接由 setupUi() 內的 connectSlotsByName() 自動完成，
//     只要 slot 名稱符合 on_<控件名>_<signal名> 即可，不需手寫 connect()。
//
// 設計：
//   一次只載入「一張」圖片 → m_current。
//   清單內容固定 4 個版本 (原圖 / 灰階 / 二值 / 8色)，建構子建立一次即可。
//   點的版本 → 對 m_current 呼叫對應的 getXXX()（首次會計算並快取），
//   然後把該版本畫到右側。
//
//   右側用 QGraphicsView + QGraphicsPixmapItem：
//   scene 只放一個 pixmap item，縮放交給 view 的 transform，
//   所以視窗放大縮小時只要重跑 fitInView() 圖片就會跟著變大變小。

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 右側：建立 scene 與唯一的 pixmap item
    m_scene = new QGraphicsScene(this);
    m_pixmapItem = m_scene->addPixmap(QPixmap());
    ui->imageView->setScene(m_scene);

    // 左側面板預設窄一點（寫在程式裡，Qt5 / Qt6 都一致），
    // 剩下的寬度全部留給右側圖片。
    ui->splitter->setSizes(QList<int>{ 180, 720 });
    ui->splitter->setStretchFactor(0, 0);   // 左側不跟著視窗放大
    ui->splitter->setStretchFactor(1, 1);   // 右側吃掉所有多餘空間

    initList();     // 清單只建立一次（4 個固定版本）
}

MainWindow::~MainWindow()
{
    delete m_current;   // 目前載入的圖由 MainWindow 擁有
    m_current = nullptr;

    delete ui;
}

// ===== 左側：載入照片（一次一張） =====
void MainWindow::on_loadButton_clicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Load Picture"),
        QString(),
        tr("Images (*.bmp *.png *.jpg *.jpeg)"));

    if (path.isEmpty()) {
        return;             // 使用者取消
    }

    BitmapItem *item = new BitmapItem(path);
    if (!item->isValid()) {
        delete item;        // 載入失敗 → 丟掉，保留原本的圖
        return;
    }

    // 載入成功 → 換掉舊圖
    delete m_current;
    m_current = item;

    m_viewMode = ViewMode::Original;    // 預設先看原圖
    selectRow();                        // 清單選取切到「原圖」那一列
    showCurrentImage();                 // 畫右側
}

// ===== 左側：在清單點選某個版本 → 呼叫對應的 getXXX() =====
void MainWindow::on_pictureList_itemSelectionChanged()
{
    QListWidgetItem *row = ui->pictureList->currentItem();
    if (!row || !m_current) {
        return;
    }

    // 每一列用 Qt::UserRole 偷存它是哪個版本 (ViewMode 的整數值)
    m_viewMode = static_cast<ViewMode>(row->data(Qt::UserRole).toInt());

    // 依版本呼叫對應的 getXXX()（第一次會計算並快取），再顯示
    showCurrentImage();
}

// ===== 建立清單內容：固定 4 個版本（只做一次） =====
void MainWindow::initList()
{
    const struct {
        const char *label;
        ViewMode    mode;
    } entries[] = {
        { "Original", ViewMode::Original },
        { "Gray",     ViewMode::Gray     },
        { "Binary",   ViewMode::Binary   },
        { "8 Color",  ViewMode::Color8   },
    };

    for (int i = 0; i < 4; ++i) {
        QListWidgetItem *row = new QListWidgetItem(entries[i].label);
        row->setData(Qt::UserRole, static_cast<int>(entries[i].mode));
        ui->pictureList->addItem(row);
    }
}

// ===== 依 m_viewMode 把清單選取切到對應那一列 =====
void MainWindow::selectRow()
{
    // 切換選取會觸發 itemSelectionChanged；這裡只想要「選取」，
    // 由呼叫端自行決定何時重畫，所以先把訊號擋掉。
    ui->pictureList->blockSignals(true);
    ui->pictureList->setCurrentRow(static_cast<int>(m_viewMode));
    ui->pictureList->blockSignals(false);
}

// ===== 依 m_viewMode 呼叫 getXXX() 並把影像放到右側 =====
void MainWindow::showCurrentImage()
{
    if (!m_current) {
        m_pixmapItem->setPixmap(QPixmap());
        m_scene->setSceneRect(m_pixmapItem->boundingRect());
        fitImageToView();
        return;
    }

    // 依版本「呼叫對應的 getXXX()」取得影像
    QImage image;
    switch (m_viewMode) {
    case ViewMode::Gray:   image = m_current->getGray();     break;
    case ViewMode::Binary: image = m_current->getBinary();   break;
    case ViewMode::Color8: image = m_current->getColor8();   break;
    case ViewMode::Original:
    default:               image = m_current->getOriginal(); break;
    }

    // 換上圖片（原尺寸），實際顯示大小由 fitImageToView() 決定
    m_pixmapItem->setPixmap(QPixmap::fromImage(image));

    // scene 範圍跟著圖片大小，fitInView() 才有正確的基準
    m_scene->setSceneRect(m_pixmapItem->boundingRect());

    fitImageToView();
}

// ===== 把圖片依右側可視範圍等比縮放 =====
// 用 QGraphicsView 內建的 fitInView()：圖片會隨 view 大小自己放大縮小，
// 不會變形（KeepAspectRatio），也不會出現捲軸。
void MainWindow::fitImageToView()
{
    if (!m_pixmapItem || m_pixmapItem->pixmap().isNull()) {
        ui->imageView->resetTransform();
        return;
    }

    ui->imageView->resetTransform();
    ui->imageView->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
}

// ===== 視窗（含分隔線）尺寸改變 → 圖片跟著縮放 =====
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // 注意：fitInView() 會改變 view 的捲軸，讓 view 的最小尺寸變動，
    //       因此延到 event loop 下一輪再縮放，避免遞迴 / 佈局抖動。
    QMetaObject::invokeMethod(ui->imageView,
                              [this]() { fitImageToView(); },
                              Qt::QueuedConnection);
}

#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "progress.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QListWidgetItem>
#include <QPixmap>
#include <QResizeEvent>
#include <QStatusBar>

#include <vector>

// 註：按鈕 / 清單的連接由 setupUi() 內的 connectSlotsByName() 自動完成，
//     只要 slot 名稱符合 on_<控件名>_<signal名> 即可，不需手寫 connect()。
//
// 設計：
//   一次只載入「一張」圖片 → m_current。
//   清單內容 = BitmapItem::versions()（原圖 / 灰階 / 二值 / 8色 / C,M,Y 色版…），
//   列數與文字都由 BitmapItem 決定，這裡只是照抄，不寫死任何版本名稱。
//   點某一列 → 用列號當版本索引 → m_current->getImageAt(索引)
//   （等同呼叫該版本對應的 getXXX()，首次會計算並快取），然後畫到右側。
//   ** 新增版本只要改 BitmapItem，這個檔案不用動。 **
//
//   Shares 清單的文字跟著「目前選的版本」變：
//     選 Binary    → Binary_share0 / Binary_share1
//     選 C Channel → C_share0 / C_share1
//     選 Original / Gray → 清空（這兩個版本沒有對應的 share 演算法）+ Generate 停用
//   文字一律由 BitmapItem::sharePrefix() 提供，這裡不寫死任何模式名稱；
//   切換版本時 initShareList() 會重建清單。
//
//   Shares 清單列數固定 kShareCount (2)，切版本時重建成對應名稱。
//   右側畫面只由 (m_viewMode, m_versionIndex, m_shareIndex) 決定：
//     m_viewMode == Share   → 取 getShares(m_shareMode)[m_shareIndex]
//                             （m_shareMode 由 m_versionIndex 透過
//                               BitmapItem::shareModeOf() 同步）
//     m_viewMode == Version → 取 getImageAt(m_versionIndex)
//   所以 scene 永遠只有一個 pixmap item，不需要為了「疊圖」多加 item。
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
    ui->splitter->setSizes(QList<int>{ 200, 700 });
    ui->splitter->setStretchFactor(0, 0);   // 左側不跟著視窗放大
    ui->splitter->setStretchFactor(1, 1);   // 右側吃掉所有多餘空間

    // 兩個清單平均分掉左側剩下的高度，按鈕 / 編輯格 / 進度條維持原本大小
    ui->leftLayout->setStretch(0, 0);   // loadButton
    ui->leftLayout->setStretch(1, 1);   // pictureList
    ui->leftLayout->setStretch(2, 0);   // ssConfigWidget
    ui->leftLayout->setStretch(3, 0);   // generateButton
    ui->leftLayout->setStretch(4, 1);   // shareList
    ui->leftLayout->setStretch(5, 0);   // shareProgress

    initList();         // 清單內容來自 BitmapItem::versions()（版本增減不需要改這裡）
    initShareList();    // Shares 清單依「目前已選的版本」命名（Original → 清空）
    clearShares();      // 一開始沒有圖 → share 清單沒有選取
    resetProgress();    // 進度條：尚未生成

    // 一開始選的是 versions()[0]（Original）→ 不支援 share → Generate 先停用
    ui->generateButton->setEnabled(currentVersionSupportsShares());
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

    // 新圖還沒有產生 shares（舊圖的 shares 也跟著作廢）→ 重設 share 的狀態
    clearShares();

    m_viewMode = ViewMode::Version;     // 預設先看第 0 個版本（versions()[0] = 原圖）
    m_versionIndex = 0;
    m_shareMode = BitmapItem::shareModeOf(m_versionIndex);  // 版本 0 = Original → 不支援 share
    initShareList();                    // shareList 跟著版本重命名（Original → 清空）
    ui->generateButton->setEnabled(currentVersionSupportsShares());
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

    // 每一列用 Qt::UserRole 偷存它是「第幾個版本」（= BitmapItem::versions() 的索引）
    m_versionIndex = row->data(Qt::UserRole).toInt();
    m_viewMode     = ViewMode::Version;

    // 版本換了 → share 的來源模式也跟著換（Original / Gray → 不支援）
    m_shareMode = BitmapItem::shareModeOf(m_versionIndex);

    // shareList 依新版本重新命名：Binary_share0 / C_share0 …
    // （不支援 share 的版本 → 清單清空，Generate 也一併停用）
    initShareList();
    ui->generateButton->setEnabled(currentVersionSupportsShares());

    // 舊版版本生成的 share 已經不對應目前畫面 → 清掉選取與進度條
    // （clearShares() 內部會擋訊號把 shareList 選取設成 -1，
    //   所以不需要在這裡另外寫 blockSignals 區塊：兩個清單仍然是互斥的）
    clearShares();

    if (!currentVersionSupportsShares()) {
        statusBar()->showMessage(
            tr("%1 版本不支援生成 share（請選 Binary / 8 Color / C / M / Y）")
                .arg(QString::fromUtf8(BitmapItem::versions()[m_versionIndex].label)));
    } else {
        statusBar()->clearMessage();
    }

    // 依版本呼叫對應的 getXXX()（第一次會計算並快取），再顯示
    showCurrentImage();
}

// ===== 左側：在 Shares 清單點選某一張 share =====
void MainWindow::on_shareList_itemSelectionChanged()
{
    QListWidgetItem *row = ui->shareList->currentItem();
    if (!row || !m_current) {
        return;
    }

    // 每一列用 Qt::UserRole 存「第幾張 share」
    m_shareIndex = row->data(Qt::UserRole).toInt();
    m_viewMode   = ViewMode::Share;

    // 兩個清單互斥：選了 share 就不要同時選著版本（同上，先擋訊號）
    ui->pictureList->blockSignals(true);
    ui->pictureList->clearSelection();
    ui->pictureList->setCurrentRow(-1);
    ui->pictureList->blockSignals(false);

    showCurrentImage();
}

// ===== 左側：生成 Shares 按鈕 =====
void MainWindow::on_generateButton_clicked()
{
    generateShares();
}

// ===== 建立清單內容：完全由 BitmapItem::versions() 決定 =====
// 這裡刻意不寫死任何版本名稱：
//   BitmapItem 新增一個版本 → 清單自動多一列，這個函式不用改。
//
// 注意：setupUi() 已經呼叫過 connectSlotsByName()，slot 早就連上了；
//       addItem() / clear() 會讓 Qt 自動改選取而觸發 itemSelectionChanged。
//       建表期間先 blockSignals，避免 slot 在「清單還不完整」時就跑，
//       把 m_versionIndex 蓋成錯的值（列號與版本索引因此對不上）。
void MainWindow::initList()
{
    const std::vector<BitmapItemVersion> &versions = BitmapItem::versions();

    ui->pictureList->blockSignals(true);

    for (int i = 0; i < static_cast<int>(versions.size()); ++i) {
        QListWidgetItem *row = new QListWidgetItem(
            QString::fromUtf8(versions[i].label));       // 標題由 BitmapItem 提供
        row->setData(Qt::UserRole, i);                   // 偷存「第幾個版本」
        ui->pictureList->addItem(row);
    }

    ui->pictureList->blockSignals(false);
}

// ===== 依版本索引找出 pictureList 的列號 =====
// 不再假設「列號 == 版本索引」：列號是清單位置，版本索引是 UserRole 存的值。
// 目前 initList() 兩者相同，但這裡用查表方式，之後清單若改順序 / 過濾也不會錯位。
int MainWindow::rowOfVersion(int versionIndex) const
{
    for (int row = 0; row < ui->pictureList->count(); ++row) {
        if (ui->pictureList->item(row)->data(Qt::UserRole).toInt() == versionIndex) {
            return row;
        }
    }
    return -1;      // 找不到 → 清掉選取
}

// ===== 依 m_viewMode / m_versionIndex 把清單選取切到對應那一列 =====
void MainWindow::selectRow()
{
    // 切換選取會觸發 itemSelectionChanged；這裡只想要「選取」，
    // 由呼叫端自行決定何時重畫，所以先把訊號擋掉。
    // （m_viewMode 為 Share 時 pictureList 沒有對應的列 → 選取清空）
    const int row = (m_viewMode == ViewMode::Share) ? -1 : rowOfVersion(m_versionIndex);

    ui->pictureList->blockSignals(true);
    ui->pictureList->setCurrentRow(row);
    ui->pictureList->blockSignals(false);
}

// ===== 建立 / 重建 Shares 清單（依目前選的版本）=====
// 文字一律由 BitmapItem 提供（sharePrefix()），這裡不寫死任何模式名稱：
//   Binary    → Binary_share0 / Binary_share1
//   C Channel → C_share0 / C_share1
//   Original  → 清空（不支援 share）
//
// 可重複呼叫：切換版本時會再叫一次，所以先 clear() 再重建。
// 建表期間先 blockSignals，理由同 initList()（避免 slot 在清單不完整時就跑）。
void MainWindow::initShareList()
{
    ui->shareList->blockSignals(true);
    ui->shareList->clear();

    if (currentVersionSupportsShares()) {
        const QString prefix = BitmapItem::sharePrefix(m_versionIndex);

        for (int i = 0; i < kShareCount; ++i) {
            QListWidgetItem *row = new QListWidgetItem(
                QStringLiteral("%1_share%2").arg(prefix).arg(i));
            row->setData(Qt::UserRole, i);      // 偷存「第幾張 share」
            ui->shareList->addItem(row);
        }
    }
    // 不支援 share → 清單保持空的（Generate 由呼叫端停用）

    ui->shareList->blockSignals(false);
}

// ===== 目前選的版本能不能生成 share =====
// 沒有載入圖片時一律 false（Generate 應該停用）。
bool MainWindow::currentVersionSupportsShares() const
{
    return m_current && BitmapItem::supportsShares(m_versionIndex);
}

// ===== 依 m_shareIndex 把 Shares 清單選取切到對應那一列 =====
void MainWindow::selectShareRow()
{
    // 同 selectRow()：只改選取，不觸發重畫
    ui->shareList->blockSignals(true);
    ui->shareList->setCurrentRow(m_shareIndex);
    ui->shareList->blockSignals(false);
}

// ===== 依 col / row 編輯格重新產生機密圖 =====
// 注意：createShares() 是同步計算（逐像素），大圖會讓 UI 暫時凍結，
//       因此每算完一列就透過 Progress 的 callback 呼叫 processEvents()，
//       讓進度條有機會重繪（processEvents() 只出現在這裡，不會污染 BitmapItem）。
void MainWindow::generateShares()
{
    if (!m_current) {
        return;             // 還沒載入圖片 → 不做事
    }
    if (!currentVersionSupportsShares()) {
        // Original / Gray 沒有對應的 share 演算法 → 直接返回（按鈕本來就停用，這裡是保險）
        return;
    }

    const int col = ui->ratioColSpin->value();
    const int row = ui->ratioRowSpin->value();

    // 目前版本對應的 share 模式（Binary / C / M / Y …）與清單前綴（"C" 等）
    const ShareMode mode = m_shareMode;
    const QString   prefix = BitmapItem::sharePrefix(m_versionIndex);

    // --- 進度條：以「原圖列數」為單位 ---
    // 先 disable 按鈕，避免 processEvents() 期間使用者又按一次而重入
    ui->generateButton->setEnabled(false);
    ui->shareProgress->setEnabled(true);
    ui->shareProgress->setRange(0, 100);
    ui->shareProgress->setValue(0);
    ui->shareProgress->setFormat(tr("Generating %1… %p%").arg(prefix));
    QCoreApplication::processEvents();      // 讓 0% 先顯示出來

    // 參數變了會清掉五個模式的快取；forceRegenerate = true 一律重算
    m_current->setSSConfig(kShareCount, 1, row, col);

    // 進度回報器：GUI 只負責「收到進度要做什麼」，不知道誰在算
    //   total 由 BitmapItem 透過 setTotal() 給（= 原圖列數）
    Progress progress([this](int done, int total) {
        ui->shareProgress->setMaximum(total);
        ui->shareProgress->setValue(done);
        if (done < total) {
            QCoreApplication::processEvents();   // 邊算邊更新畫面
        }
    });

    const std::vector<QImage> shares =
        m_current->getShares(mode, &progress, /*forceRegenerate=*/true);

    // 生成結束 → 只有「目前版本仍支援 share」才把按鈕開回來
    ui->generateButton->setEnabled(currentVersionSupportsShares());

    if (shares.empty()) {
        // col * row 為奇數（或參數不合法）→ 編碼矩陣是空的，產生不出 share
        clearShares();
        m_viewMode = ViewMode::Share;   // 切到 Share 模式，但沒有圖 → 右側空白
        selectRow();
        showCurrentImage();

        ui->shareProgress->setEnabled(true);
        ui->shareProgress->setRange(0, 100);
        ui->shareProgress->setValue(0);
        ui->shareProgress->setFormat(tr("col × row 必須是偶數"));

        statusBar()->showMessage(
            tr("col × row 必須是偶數（目前 %1 × %2）").arg(col).arg(row));
        return;
    }

    statusBar()->clearMessage();

    // 進度條收尾（最後一列的 callback 已把 value 設到 maximum）
    ui->shareProgress->setFormat(tr("Done: %1… col %2 × row %3")
                                     .arg(prefix).arg(col).arg(row));

    // 生成後直接切到第 0 張 share
    m_viewMode   = ViewMode::Share;
    m_shareIndex = 0;
    selectRow();                // pictureList 清掉選取（Share 不在 pictureList 裡）
    selectShareRow();           // shareList 選到第 0 張（Binary_share0 / C_share0 …）
    showCurrentImage();
}

// ===== 進度條：回到「尚未生成」的狀態 =====
void MainWindow::resetProgress()
{
    ui->shareProgress->setRange(0, 100);
    ui->shareProgress->setValue(0);
    ui->shareProgress->setFormat(tr("No shares"));
    ui->shareProgress->setEnabled(false);
}

// ===== 沒有可用的機密圖 → 重設 share 的選取狀態 =====
void MainWindow::clearShares()
{
    m_shareIndex = 0;

    ui->shareList->blockSignals(true);
    ui->shareList->setCurrentRow(-1);
    ui->shareList->blockSignals(false);

    resetProgress();        // 換圖 / 生成失敗 → 進度條回到「尚未生成」
}

// ===== 依 m_viewMode（+ m_shareIndex）呼叫 getXXX() 並把影像放到右側 =====
void MainWindow::showCurrentImage()
{
    if (!m_current) {
        m_pixmapItem->setPixmap(QPixmap());
        m_scene->setSceneRect(m_pixmapItem->boundingRect());
        fitImageToView();
        return;
    }

    // 依版本「呼叫對應的 getXXX()」取得影像
    //   版本索引 → BitmapItem::getImageAt() 內部用成員函式指標呼叫對應的 getXXX()，
    //   所以這裡不再需要為每個版本寫一個 case（新增版本不用改這個函式）。
    QImage image;
    switch (m_viewMode) {
    case ViewMode::Version:
        image = m_current->getImageAt(m_versionIndex);
        break;
    case ViewMode::Share: {
        // 機密圖一次產生全部（已快取），依 m_shareIndex 挑第幾張
        // 注意：這裡要用 m_shareMode（= 目前選的版本），不能寫死 Binary，
        //       否則選了 C Channel 時右側仍會顯示 Binary 的 share。
        const std::vector<QImage> shares = m_current->getShares(m_shareMode);
        if (m_shareIndex >= 0 && m_shareIndex < static_cast<int>(shares.size())) {
            image = shares[m_shareIndex];
        }
        // 沒有 share（例如 col*row 為奇數）→ image 保持 isNull → 右側空白
        break;
    }
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

# Changelog

SecretSharing 的所有版本變更。格式參考 [Keep a Changelog](https://keepachangelog.com/zh-TW/1.1.0/)，
版本號遵循 [Semantic Versioning](https://semver.org/lang/zh-TW/)。

## [Unreleased]

- （尚無）

## [v0.3.2] - 2026-11-13

### 修正

- 修正 8 色 share 的花紋規律問題：C / M / Y 三個色版改為各自抽取隨機數（`aC` / `aM` / `aY`），
  不再共用同一組隨機位元，避免三色版花紋逐格重合
- 仍保留「同一趟掃描」產生 C / M / Y 三張 share，確保座標與尺寸一致、只回報一次進度
- 更新 8 色合併相關註解，說明「同一趟掃描 ≠ 共用隨機數」

## [v0.3.1] - 2026-11-12

### 變更

- 機密分享預設像素擴張量由 col=1 / row=2 改為 **col=2 / row=2**
- 同步調整 GUI（`ratioColSpin` / `ratioRowSpin`）、`BitmapItem` 預設值與 CLI 說明

## [v0.3.0] - 2026-11-12

### 新增

- 完成 **8 色 share 疊圖**：C / M / Y 的 share 疊圖逐通道取 min 合成 8 色 share 疊圖
  （遵循減法混色原則，可還原 SecretN (a,b,c)）
- GUI 的 Shares 清單會依目前選的版本顯示對應的 `Binary` / `C` / `M` / `Y` / `8Color` share
- CLI 新增 `--share-mode <binary|color8|c|m|y>` 參數，可直接輸出指定模式的 share
- CLI 的 `share2.png` 改走 `getSuperimposed()`，與 GUI 的 `<prefix>_superimposed` 同一條路徑

### 變更

- `create2Shares()` 改為通用形 `createShares(ShareMode)`，支援 Binary / Cyan / Magenta / Yellow
- 每個來源模式各自快取 share 與疊圖（`m_C_shares` / `m_M_shares` / `m_Y_shares` …），互不影響
- `ShareMode` 列舉型別與 `getShares()` / `getSuperimposed()` 改為帶 mode 參數

### 修正

- 修正 RGB → C/M/Y 色版取法：改為直接取該通道 `C=(R,255,255)`、`M=(255,G,255)`、`Y=(255,255,B)`，
  不再做反相，該通道為 0 代表有墨、255 代表白色
- 修正一個 C / M / Y 色版（channel）的 bug

### 重構

- ViewMode 重構：版本（version）資訊移到 `BitmapItem::versions()`，GUI 清單不再寫死版本名稱
- `MainWindow` 只透過 `supportsShares()` / `shareModeOf()` / `sharePrefix()` 查詢版本資訊

## [v0.2.2] - 2026-11-06

### 新增

- 新增 `Progress` 進度回報器（`progress.h`）：演算法端只要回報，UI 端自行決定怎麼顯示
- 新增 clangd 設定，支援 VSCodium 閱讀程式碼

### 變更

- 改寫進度條，提升可讀性與維護性

## [v0.2.1] - 2026-10-22

### 新增

- GUI 新增進度條，產生 share 時顯示進度

## [v0.2.0] - 2026-10-22

### 新增

- 完成 v0.2 GUI：載入圖片、版本清單、產生 share、於右側顯示 share
- 完成 shares = 2 的機密分享編碼矩陣，CLI 可測試（`--dump-matrix`）

## [v0.1.1] - 2026-10-16

### 修正

- 修正 Windows / MSVC 靜態建置（`CMAKE_MSVC_RUNTIME_LIBRARY` 改為 `/MT`）

## [v0.1.0] - 2026-10-15

### 新增

- 初版發佈
- `BitmapItem` 圖片類別：載入 BMP，產生灰階 (gray)、二值 (binary)、8 色 (color8) 三種版本
- CLI 測試程式 `SecretSharingCLI`
- 完成 GUI，圖片隨視窗等比縮放（`QGraphicsView` + `fitInView`）

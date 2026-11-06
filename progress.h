#ifndef PROGRESS_H
#define PROGRESS_H

#include <functional>
#include <utility>

// Progress
// 「進度回報器」：只負責記錄 / 轉發進度，完全不知道誰在算、也不知道要怎麼顯示。
//
// 為什麼要獨立成一個型別（而不是直接傳 std::function）？
//   1. 演算法端只認得 Progress，不知道 Qt、不知道進度條、不知道 processEvents()。
//   2. 「有沒有 callback」的判斷收在 report() 裡一處，演算法迴圈不用到處寫 if。
//   3. total 由演算法設定（setTotal），UI 端不必自己去猜總量。
//   4. 之後要加「取消」「階段名稱」「剩餘時間」都只改這個檔案。
//
// 用法（GUI）：
//   Progress p([this](int done, int total) {
//       ui->shareProgress->setValue(done);
//   });
//   item.getShares(&p, /*forceRegenerate=*/true);
//
// 用法（CLI / 不需要進度）：
//   item.getShares();        // 傳 nullptr → 完全不做任何事
class Progress
{
public:
    // callback 的型別：做事的時候會收到「已完成量」與「總量」兩個 int
    // （std::move 只是省一次複製，拿掉也完全正確）
    using Callback = std::function<void(int done, int total)>;

    // 不給 callback 就建一個「空」的 Progress：report() 什麼都不做
    explicit Progress(Callback cb = {})
        : m_cb(std::move(cb))
    {
    }

    virtual ~Progress() = default;

    // 設定總量（例如原圖列數）；<= 0 代表「不知道總量」
    void setTotal(int total) { m_total = total; }

    int total() const { return m_total; }
    int done()  const { return m_done; }

    // 回報目前完成量
    //   沒給 callback → 只記錄，不做事
    //   有給 callback → 記錄後呼叫它（也就是 UI 端傳進來的 lambda）
    void report(int done)
    {
        m_done = done;
        if (m_cb) {
            m_cb(m_done, m_total);
        }
    }

    // 是否已經完成（有設 total 且 done >= total）
    bool isFinished() const { return m_total > 0 && m_done >= m_total; }

    // 取消旗標：演算法可以定期檢查，決定要不要提早結束。
    // 預設永不取消；需要取消時再覆寫這個函式（目前 GUI 沒用到，先留介面）。
    virtual bool isCanceled() const { return false; }

private:
    Callback m_cb;      // 盒子：存著「進度發生時要做什麼」，可以是空的
    int m_total = 0;    // 總量，由 setTotal() 設定
    int m_done  = 0;    // 目前完成量
};

#endif // PROGRESS_H

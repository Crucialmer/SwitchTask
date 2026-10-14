#ifndef WIN_SWITCHER_UPDATEDIALOG_H
#define WIN_SWITCHER_UPDATEDIALOG_H

#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QNetworkAccessManager>
#include <QVersionNumber>

QT_BEGIN_NAMESPACE

namespace Ui {
    class UpdateDialog;
}

QT_END_NAMESPACE

class UpdateDialog final : public QDialog {
    Q_OBJECT

public:
    explicit UpdateDialog(QWidget* parent = nullptr);
    ~UpdateDialog() override;
    static void verifyUpdate(const QCoreApplication& app);
    /// 版本字符串解析（去 v 前缀 / 多余尾随 .0，保留至多 3 段）：更新对话框与静默检查（UpdateChecker）共用同一语义
    static QVersionNumber normalizeVersion(const QString& ver);

private:
    void applyTheme(); // 更新说明区随深浅色模式着色
    void fetchGithubReleaseInfo();
    void download(const QString& url, const QString& savePath);
    QString writeBat(const QString& sourceDir, const QString& targetDir) const;
    static QString toLocalTime(const QString& isoTime);

signals:
    void downloadSucceed(const QString& filePath);

protected:
    void showEvent(QShowEvent*) override;

private:
    Ui::UpdateDialog* ui;
    QNetworkAccessManager manager;
    static constexpr auto Owner = "Crucialmer";
    static constexpr auto Repo = "SwitchTask"; // 自更新检查指向本仓库的 Release
    const QVersionNumber version = normalizeVersion(QApplication::applicationVersion());

    struct ReleaseInfo {
        QVersionNumber ver;
        QString description;
        QString downloadUrl;
        QString digest; // 资产 sha256（API 提供时用于下载后校验；旧资产可能为空）
        QString publishTime;
    } relInfo;

    struct {
        bool blocked = false; // 重定向到非白名单主机（abort 后 finished 里给出准确报错）
        QString digest; // 本次下载对应的资产 sha256 摘要快照（下载期间重查更新不覆盖）
        QVersionNumber targetVer; // 本次下载的目标版本快照（同上：批处理自证参数须用本包对应的目标版本）
        QNetworkReply* reply = nullptr;
        QFile file;
    } downloadStatus;

    struct {
        QString fileName;
        const QString extractDir = "_extract";
    } archive;

    qint64 lastFetchMs = 0; // 更新信息节流：上次成功获取的时间戳（毫秒）
};


#endif //WIN_SWITCHER_UPDATEDIALOG_H

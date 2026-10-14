#ifndef WIN_SWITCHER_UPDATECHECKER_H
#define WIN_SWITCHER_UPDATECHECKER_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

/// 静默更新检查（用户裁决 B+D）：
///  - 启动后延迟首查一次（此后每 24h 复查）
///  - 发现“比当前版本新、且本会话尚未提醒过”的版本 → newVersionFound（托盘气泡提醒，点击进入更新对话框）
///  - 静默纪律：失败只记日志不打扰；仅只读 GitHub API（releases/latest），不下载任何内容；
///    下载与安装仍由用户在更新对话框点击 Update 后才发生（不静默替换文件）
class UpdateChecker : public QObject {
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);

    /// 启动巡检：延迟首查 + 24h 周期（由内部定时器触发；只应调用一次）
    void start();

signals:
    /// 发现新版本（参数为解析后的版本号文本，如 "1.3"）
    void newVersionFound(const QString& version);

private:
    void checkOnce();

    QNetworkAccessManager manager;
    QTimer periodicTimer;           // 24h 复查
    QSet<QString> notifiedVersions; // 本会话已提醒版本（防 24h 复查重复打扰；重启会话后会再提醒一次，符合“重启后提示”预期）
    bool checking = false;          // 单飞：慢网络时上一请求未返回不叠加新请求
};

#endif //WIN_SWITCHER_UPDATECHECKER_H

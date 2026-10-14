#include "utils/UpdateChecker.h"
#include "UpdateDialog.h" // 复用 normalizeVersion（与更新对话框同一套版本解析/比较语义）
#include <QApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace {
    constexpr int kFirstCheckDelayMs = 10 * 1000;             // 启动延迟：让托盘/钩子先就绪，避免与启动高峰叠加
    constexpr int kPeriodicIntervalMs = 24 * 60 * 60 * 1000;  // D：每 24h 复查
}

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {
    periodicTimer.setInterval(kPeriodicIntervalMs);
    connect(&periodicTimer, &QTimer::timeout, this, &UpdateChecker::checkOnce);
}

void UpdateChecker::start() {
    QTimer::singleShot(kFirstCheckDelayMs, this, &UpdateChecker::checkOnce);
    periodicTimer.start();
}

void UpdateChecker::checkOnce() {
    if (checking) {
        return; // 慢网络：不叠加请求（24h 复查会在下个周期再来）
    }
    if (QApplication::applicationVersion().isEmpty()) {
        return; // 版本未知：无比较基准（与更新对话框同一前提）
    }
    const auto current = UpdateDialog::normalizeVersion(QApplication::applicationVersion()).normalized();
    if (current.isNull()) {
        qWarning() << "UpdateChecker: unparsable application version" << QApplication::applicationVersion();
        return;
    }

    checking = true;
    // 与更新对话框相同的端点与重定向策略（api.github.com 正常不重定向，属防御）
    const QString url = QStringLiteral("https://api.github.com/repos/Crucialmer/SwitchTask/releases/latest");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = manager.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, current] {
        reply->deleteLater();
        checking = false;
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "UpdateChecker: fetch failed" << reply->errorString(); // 静默纪律：只记日志
            return;
        }
        const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        const auto tag = obj["tag_name"].toString();
        const auto remote = UpdateDialog::normalizeVersion(tag).normalized();
        if (remote.isNull()) {
            qWarning() << "UpdateChecker: cannot parse release tag" << tag;
            return;
        }
        if (!(remote > current)) { // 归一化比较：与更新对话框/自证/CI 校验同一纪律
            qDebug() << "UpdateChecker: up to date" << tag;
            return;
        }
        const auto verStr = remote.toString();
        if (notifiedVersions.contains(verStr)) {
            qDebug() << "UpdateChecker: already notified this session" << verStr;
            return; // 本会话已提醒过该版本：24h 复查不重复打扰
        }
        notifiedVersions.insert(verStr);
        qInfo() << "UpdateChecker: new version available" << verStr;
        emit newVersionFound(verStr);
    });
}

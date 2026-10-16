// You may need to build the project (run Qt uic code generator) to get "ui_UpdateDialog.h" resolved

#include "UpdateDialog.h"
#include <QCommandLineParser>
#include "ui_UpdateDialog.h"
#include <QDebug>
#include <QNetworkReply>
#include <QSslSocket> // 显式包含：用于启动时打印 SSL 库版本/可用性（勿依赖传递包含）
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include "utils/SystemTray.h"
#include "utils/Theme.h"
#include "utils/MacTitleBar.h"
#include "utils/setWindowBlur.h"
#include "utils/Util.h"
#include <QStyleOption>
#include <QPainter>
#include "QtCore/private/qzipreader_p.h"

/// 更新链路白名单：仅放行 https + GitHub 及其资源 CDN 主机（防重定向到任意站点）
static bool isAllowedGithubUrl(const QUrl& url) {
    if (url.scheme() != QStringLiteral("https"))
        return false;
    const auto host = url.host();
    return host == QStringLiteral("github.com")
           || host == QStringLiteral("api.github.com")
           || host.endsWith(QStringLiteral(".githubusercontent.com"));
}

/// 手动逐条目解压 zip（替代 QZipReader::extractAll）。
/// 起因：Qt 6.8 的 extractAll 对【根级文件条目】处理有缺陷——实测会把根级文件（如 Qt6Core.dll、
/// SwitchTask.exe）创建为空目录并整体返回失败（嵌套文件正常），导致更新包解压失败、自动更新不可用。
/// 逐条目 API（fileInfoList/fileData）行为正常：逐条建父目录、写文件，返回是否全部成功。
static bool extractZipTo(const QString& zipPath, const QString& destDir) {
    QZipReader reader(zipPath);
    if (!reader.isReadable()) {
        qWarning() << "#extractZipTo: zip not readable:" << zipPath;
        return false;
    }
    const auto infos = reader.fileInfoList();
    if (infos.isEmpty()) {
        qWarning() << "#extractZipTo: no entries:" << zipPath;
        return false;
    }
    int files = 0;
    const auto destRoot = QDir(destDir).absolutePath();
    for (const auto& fi: infos) {
        // zip-slip 防护：拒绝绝对路径与越界条目（本仓库发布包均为相对路径；上游失陷时把写面限制在目标目录内）
        const auto cleanRel = QDir::cleanPath(fi.filePath);
        if (QDir::isAbsolutePath(fi.filePath) || cleanRel.startsWith(QStringLiteral("..")) || cleanRel.contains(':')) {
            qWarning() << "#extractZipTo: unsafe entry rejected:" << fi.filePath;
            return false;
        }
        const auto dest = QDir(destRoot).absoluteFilePath(cleanRel);
        if (!dest.startsWith(destRoot + '/')) { // 双保险：规范化后仍须落在目标目录内
            qWarning() << "#extractZipTo: dest escapes target dir:" << dest;
            return false;
        }
        if (fi.isDir) {
            if (!QDir().mkpath(dest)) {
                qWarning() << "#extractZipTo: mkpath failed:" << dest;
                return false;
            }
            continue;
        }
        if (!QDir().mkpath(QFileInfo(dest).path())) { // 先建父目录（zip 可能不含目录条目）
            qWarning() << "#extractZipTo: mkpath parent failed:" << dest;
            return false;
        }
        const auto data = reader.fileData(fi.filePath);
        if (data.size() != fi.size) { // 解压后长度必须与条目声明一致（损坏/截断即失败）
            qWarning() << "#extractZipTo: entry size mismatch:" << fi.filePath << data.size() << fi.size;
            return false;
        }
        QFile out(dest);
        if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()) {
            qWarning() << "#extractZipTo: write failed:" << dest;
            return false;
        }
        ++files;
    }
    qDebug() << "#extractZipTo: extracted files:" << files << "dir:" << destDir;
    return files > 0;
}

UpdateDialog::UpdateDialog(QWidget* parent) : QDialog(parent), ui(new Ui::UpdateDialog) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("macWindowFrame");
    setFixedSize(500, 460);

    ui->setupUi(this);
    setWindowTitle("SwitchTask 检查更新");

    m_titleBar = new MacTitleBar(QStringLiteral("SwitchTask 检查更新"), this);
    ui->verticalLayout->insertWidget(0, m_titleBar);
    ui->verticalLayout->setContentsMargins(18, 0, 18, 14);
    ui->verticalLayout->setSpacing(8);

    // macOS 26 专属 48px 应用图标展示
    ui->label_icon->setPixmap(QIcon(":/img/icon.ico").pixmap(48, 48));
    ui->label_icon->setFixedSize(48, 48);

    ui->label_appName->setObjectName("label_appName");
    ui->label_badge->setObjectName("badge_gray");
    ui->label_ver->setObjectName("label_dim");
    ui->label_notesHeader->setObjectName("label_section");
    ui->label_notesTag->setObjectName("label_dim");
    ui->label_progress->setObjectName("label_dim");

    ui->btn_update->setText(QStringLiteral("立即更新"));
    ui->btn_update->setObjectName("btn_primary");
    ui->btn_recheck->setText(QStringLiteral("重新检查"));
    ui->btn_recheck->setObjectName("btn_secondary");
    ui->btn_online->setText(QStringLiteral("在 GitHub 查看"));
    ui->btn_online->setObjectName("btn_online");

    qDebug() << QSslSocket::sslLibraryBuildVersionString() << QSslSocket::supportsSsl();

    manager.setTransferTimeout(10000); // 10s -> Operation canceled
    ui->progressWidget->hide();
    applyTheme();
    ui->btn_update->setEnabled(false);
    ui->btn_online->setEnabled(false);

    connect(ui->btn_online, &QPushButton::clicked, this, [this] {
        const QString url = relInfo.ver.isNull()
            ? QStringLiteral("https://github.com/Crucialmer/SwitchTask/releases")
            : QStringLiteral("https://github.com/Crucialmer/SwitchTask/releases/tag/v%1").arg(relInfo.ver.toString());
        QDesktopServices::openUrl(QUrl(url));
    });

    connect(ui->btn_recheck, &QPushButton::clicked, this, &UpdateDialog::fetchGithubReleaseInfo);

    connect(ui->btn_update, &QPushButton::clicked, this, [this] {
        if (ui->btn_update->text() == QStringLiteral("完成") || ui->btn_update->text() == QStringLiteral("好")) {
            close();
            return;
        }
        auto url = relInfo.downloadUrl;
        if (url.isEmpty()) {
            ui->textBrowser->setMarkdown(QStringLiteral("### 无法下载❎\n\n当前 Release 未提供可下载的资产文件。"));
            return;
        }
        const auto expectedPrefix = QStringLiteral("https://github.com/%1/%2/releases/download/").arg(Owner, Repo);
        if (!url.startsWith(expectedPrefix)) {
            qWarning() << "#Download blocked, unexpected URL:" << url;
            ui->textBrowser->setMarkdown(QStringLiteral("### 下载被阻止❎\n\n检测到非法的下载地址来源。"));
            return;
        }
        ui->btn_update->setEnabled(false);
        ui->btn_recheck->setEnabled(false);
        ui->progressWidget->show();
        ui->label_progress->setText(QStringLiteral("正在连接下载服务器..."));
        ui->progressBar->setValue(0);
        archive.fileName = QUrl(url).fileName();
        download(url, qApp->applicationDirPath() + '/' + archive.fileName);
    });

    connect(this, &UpdateDialog::downloadSucceed, this, [this](const QString& filePath) {
        qDebug() << "Download succeed" << filePath;
        if (filePath.endsWith(".zip", Qt::CaseInsensitive)) {
            auto relDir = QDir(qApp->applicationDirPath() + '/' + archive.extractDir);
            relDir.removeRecursively();
            if (extractZipTo(filePath, relDir.absolutePath()) && relDir.exists()) {
                auto entryInfos = relDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
                if (entryInfos.size() == 1 && entryInfos.first().isDir()) {
                    relDir.cd(entryInfos.first().fileName());
                }
                qDebug() << "release extract dir:" << relDir.absolutePath();
                const auto batPath = writeBat(relDir.absolutePath(), qApp->applicationDirPath());
                if (batPath.isEmpty() || !QDesktopServices::openUrl(QUrl::fromLocalFile(batPath))) {
                    qWarning() << "#Failed to start updater bat:" << batPath;
                    ui->textBrowser->setMarkdown(QStringLiteral("### 启动更新失败❎\n\n未能启动批处理更新程序，请稍后重试或前往 GitHub 手动下载。"));
                    ui->btn_update->setEnabled(true);
                    ui->btn_recheck->setEnabled(true);
                    return;
                }
                qApp->quit();
            } else {
                qWarning() << "Extract failed" << filePath;
                ui->textBrowser->setMarkdown(QStringLiteral("### 解压失败❎\n\n更新包解压过程异常，文件可能损坏。"));
                ui->btn_update->setEnabled(true);
                ui->btn_recheck->setEnabled(true);
            }
        } else {
            qWarning() << "Unsupported file type" << filePath;
            ui->textBrowser->setMarkdown(QStringLiteral("### 不支持的文件格式❎"));
            ui->btn_update->setEnabled(true);
            ui->btn_recheck->setEnabled(true);
        }
    });

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
        applyTheme();
    });
}

UpdateDialog::~UpdateDialog() {
    delete ui;
    qDebug() << "UpdateDialog destroyed";
}

void UpdateDialog::fetchGithubReleaseInfo() {
    ui->label_badge->setText(QStringLiteral("检查中..."));
    ui->label_badge->setObjectName("badge_gray");
    ui->label_ver->setText(QStringLiteral("正在连接 GitHub 获取最新版本信息..."));
    ui->label_notesTag->setText(QStringLiteral("Checking"));
    ui->textBrowser->setMarkdown(QStringLiteral("### 正在获取 Release 变更日志...\n\n请稍候，SwitchTask 正在从 GitHub 获取最新的发布记录。"));
    ui->progressWidget->hide();
    ui->btn_update->setText(QStringLiteral("立即更新"));
    ui->btn_update->setEnabled(false);
    ui->btn_online->setEnabled(false);
    applyTheme();

    const QString ApiUrl = QString("https://api.github.com/repos/%1/%2/releases/latest").arg(Owner, Repo);
    QNetworkRequest request(ApiUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = manager.get(request);
    connect(reply, &QNetworkReply::redirected, this, [reply](const QUrl& redirectUrl) {
        if (!isAllowedGithubUrl(redirectUrl)) {
            qWarning() << "#Fetch redirect to disallowed host:" << redirectUrl.toString();
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to fetch update info" << reply->errorString();
            ui->label_badge->setText(QStringLiteral("获取失败"));
            ui->label_badge->setObjectName("badge_gray");
            ui->label_ver->setText(QStringLiteral("网络异常或 GitHub API 限流"));
            ui->label_notesTag->setText(QStringLiteral("Error"));
            ui->textBrowser->setMarkdown(QStringLiteral(
                "### ⚠️ 检查更新失败\n\n"
                "无法从 GitHub 获取最新版本信息：**%1**\n\n"
                "- 请检查您的网络连接与代理配置\n"
                "- 您可以点击下方「重新检查」重试，或点击「在 GitHub 查看」前往发布页手动下载。")
                .arg(reply->errorString()));
            ui->btn_update->setEnabled(false);
            ui->btn_online->setEnabled(true);
            applyTheme();
            relInfo.downloadUrl.clear();
            relInfo.digest.clear();
            return;
        }
        lastFetchMs = QDateTime::currentMSecsSinceEpoch();
        const auto data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        const auto obj = doc.object();

        relInfo.ver = normalizeVersion(obj["tag_name"].toString());
        relInfo.description = obj["body"].toString();
        relInfo.publishTime = toLocalTime(obj["published_at"].toString());
        if (relInfo.ver.isNull()) {
            qWarning() << "Cannot parse release tag:" << obj["tag_name"].toString();
            ui->label_badge->setText(QStringLiteral("解析失败"));
            ui->label_badge->setObjectName("badge_gray");
            ui->label_ver->setText(QStringLiteral("无法识别的 Release 标签"));
            ui->textBrowser->setMarkdown(QStringLiteral("### 无法识别 Release 标签❎\n\n%1").arg(obj["tag_name"].toString()));
            ui->btn_update->setEnabled(false);
            ui->btn_online->setEnabled(true);
            applyTheme();
            relInfo.downloadUrl.clear();
            relInfo.digest.clear();
            return;
        }

        relInfo.downloadUrl.clear();
        relInfo.digest.clear();
        if (const auto assets = obj["assets"].toArray(); !assets.isEmpty()) {
            const auto pick = [this](const QJsonObject& asset) {
                relInfo.downloadUrl = asset["browser_download_url"].toString();
                relInfo.digest = asset["digest"].toString();
            };
            for (const auto& asset: assets) {
                if (asset.toObject()["name"].toString() == QStringLiteral("SwitchTask.zip")) {
                    pick(asset.toObject());
                    break;
                }
            }
            if (relInfo.downloadUrl.isEmpty()) {
                for (const auto& asset: assets) {
                    if (asset.toObject()["name"].toString().endsWith(".zip", Qt::CaseInsensitive)) {
                        pick(asset.toObject());
                        break;
                    }
                }
            }
            if (relInfo.downloadUrl.isEmpty()) {
                pick(assets.first().toObject());
            }
        }

        qDebug() << "Update info fetched" << relInfo.ver << relInfo.downloadUrl;

        bool needUpdate = relInfo.ver.normalized() > version.normalized();
        if (needUpdate) {
            ui->label_badge->setText(QStringLiteral("发现新版本"));
            ui->label_badge->setObjectName("badge_blue");
            ui->label_ver->setText(QStringLiteral("当前安装 v%1 ➔ 官方最新 v%2 (%3)")
                .arg(version.toString(), relInfo.ver.toString(), relInfo.publishTime));
            ui->label_notesTag->setText(QString("v%1").arg(relInfo.ver.toString()));
            ui->textBrowser->setMarkdown(relInfo.description.trimmed().isEmpty()
                ? QStringLiteral("### SwitchTask v%1 已发布\n\n请点击下方「立即更新」进行自动更新。").arg(relInfo.ver.toString())
                : relInfo.description);
            ui->btn_update->setText(QStringLiteral("立即更新"));
            ui->btn_update->setEnabled(!downloadStatus.reply);
        } else {
            ui->label_badge->setText(QStringLiteral("已是最新"));
            ui->label_badge->setObjectName("badge_green");
            ui->label_ver->setText(QStringLiteral("当前安装：v%1 · 官方最新稳定版").arg(version.toString()));
            ui->label_notesTag->setText(QString("v%1 (最新)").arg(version.toString()));
            ui->textBrowser->setMarkdown(QStringLiteral(
                "### 🎉 您的 SwitchTask 已经是最新的稳定版本！\n\n"
                "当前安装的 **v%1** 为官方最新发布版本，暂无可用更新。\n\n"
                "- **运行状态**：快捷切换就绪，功能全部正常\n"
                "- **发布时间**：%2\n"
                "- **静默守护**：系统后台将在启动时及每 24 小时自动复查，有新版本时将自动提醒您")
                .arg(version.toString(), relInfo.publishTime));
            ui->btn_update->setText(QStringLiteral("完成"));
            ui->btn_update->setEnabled(true);
        }
        applyTheme();
        ui->btn_online->setEnabled(true);
    });
}

void UpdateDialog::download(const QString& url, const QString& savePath) {
    QNetworkRequest request(url);
    // 限制只允许 https，避免重定向到 http（逐跳主机白名单见 redirected 连接）
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = manager.get(request);
    downloadStatus.blocked = false;
    downloadStatus.digest = relInfo.digest; // 快照：下载期间即使发生重新检查更新，也按本次下载对应的资产摘要校验
    downloadStatus.targetVer = relInfo.ver; // 目标版本快照：批处理自证参数同此纪律
    ui->btn_recheck->setEnabled(false); // 下载期间禁止“检查更新”重入（避免覆盖下载状态/共享槽位）
    // 重定向逐跳校验：仅放行 GitHub 及其资源 CDN（发布资产正常 302 到 *.githubusercontent.com）
    connect(reply, &QNetworkReply::redirected, this, [this, reply](const QUrl& redirectUrl) {
        if (!isAllowedGithubUrl(redirectUrl)) {
            qWarning() << "#Download redirect to disallowed host:" << redirectUrl.toString();
            downloadStatus.blocked = true;
            reply->abort();
        }
    });
    ui->progressWidget->show();
    ui->label_progress->setText(QStringLiteral("正在连接下载服务器..."));
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(0); // unknown size

    QFile::remove(savePath);
    downloadStatus.file.setFileName(savePath);
    if (!downloadStatus.file.open(QIODevice::WriteOnly)) { // 检查打开是否成功（Program Files 不可写场景）
        qWarning() << "Failed to open file for writing:" << savePath;
        ui->textBrowser->setMarkdown(QStringLiteral("### 无法写入文件❎\n\n%1").arg(savePath));
        ui->btn_update->setEnabled(true);
        ui->progressWidget->hide();
        ui->btn_recheck->setEnabled(true);
        reply->abort();
        reply->deleteLater();
        return;
    }
    downloadStatus.reply = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        const auto data = reply->readAll();
        if (downloadStatus.file.write(data) != data.size()) // 检查写入返回值，磁盘满时及时失败
            qWarning() << "Write incomplete:" << downloadStatus.file.fileName();
    });

    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
        qDebug() << "Download progress" << bytesReceived << bytesTotal;
        ui->progressBar->setMaximum(bytesTotal > 0 ? bytesTotal : 0);
        ui->progressBar->setValue(bytesReceived);
        const auto toMB = [](qint64 b) {
            return QString::number(static_cast<double>(b) / (1024.0 * 1024.0), 'f', 1) + " MB";
        };
        int pct = bytesTotal > 0 ? static_cast<int>(bytesReceived * 100 / bytesTotal) : 0;
        ui->label_progress->setText(QStringLiteral("正在下载更新包... %1 / %2 (%3%)")
            .arg(toMB(bytesReceived), toMB(bytesTotal), QString::number(pct)));
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        downloadStatus.reply = nullptr;
        downloadStatus.file.close();
        ui->progressWidget->hide();
        ui->btn_update->setEnabled(true);
        ui->btn_recheck->setEnabled(true);
        if (downloadStatus.blocked) {
            QFile::remove(downloadStatus.file.fileName());
            ui->textBrowser->setMarkdown(QStringLiteral("### 下载被阻止❎\n\n重定向至非白名单主机。"));
            return;
        }
        const auto httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || (httpStatus != 0 && httpStatus != 200)) {
            qWarning() << "Download failed" << reply->errorString() << httpStatus;
            QFile::remove(downloadStatus.file.fileName());
            ui->textBrowser->setMarkdown(QStringLiteral("### 下载失败❎ [%1]\n\n%2")
                .arg(reply->url().host(), reply->errorString()));
        } else {
            if (downloadStatus.digest.startsWith(QStringLiteral("sha256:"))) {
                QFile verifyFile(downloadStatus.file.fileName());
                QString actual;
                if (verifyFile.open(QIODevice::ReadOnly)) {
                    QCryptographicHash hash(QCryptographicHash::Sha256);
                    if (hash.addData(&verifyFile))
                        actual = QStringLiteral("sha256:") + QString::fromLatin1(hash.result().toHex());
                    verifyFile.close();
                }
                if (actual.compare(downloadStatus.digest, Qt::CaseInsensitive) != 0) {
                    qWarning() << "#Hash mismatch:" << actual << downloadStatus.digest;
                    ui->textBrowser->setMarkdown(QStringLiteral("### 文件校验损坏❎\n\nsha256 校验不匹配，已自动清理临时文件。"));
                    QFile::remove(downloadStatus.file.fileName());
                    return;
                }
                qDebug() << "#Hash verified" << actual;
            } else if (!downloadStatus.digest.isEmpty()) {
                qWarning() << "#Unknown digest format, skipped:" << downloadStatus.digest;
            }
            ui->textBrowser->setMarkdown(QStringLiteral("### 下载成功✅\n\n正在解压更新包并启动自动替换批处理..."));
            emit downloadSucceed(downloadStatus.file.fileName());
        }
    });
}

QString UpdateDialog::writeBat(const QString& sourceDir, const QString& targetDir) const {
    const auto appDir = qApp->applicationDirPath();
    // bat 由 cmd.exe 按系统代码页(ANSI/OEM)解析——为避免中文乱码，bat 内容保持纯 ASCII：
    // 所有路径改用 %~dp0（=bat 所在目录）在运行时展开，cmd 内部以 Unicode 处理，不受代码页影响
    const auto relSource = QDir::toNativeSeparators(QDir(appDir).relativeFilePath(sourceDir));
    auto relTarget = QDir::toNativeSeparators(QDir(appDir).relativeFilePath(targetDir));
    if (relTarget.isEmpty())
        relTarget = "."; // targetDir==程序目录时相对路径为空，用 "."（即 "%~dp0."）
    QString bat;
    bat += "@timeout /t 1 /NOBREAK\r\n";
    bat += "@cd /d \"%~dp0\"\r\n"; //切换到bat目录，否则为qt的exe目录
    bat += "@echo ##Copying files, please wait......\r\n";
    bat += QString("xcopy \"%~dp0%1\" \"%~dp0%2\" /E /H /Y\r\n").arg(relSource, relTarget);
    // xcopy 失败（返回码 >=1）时中止：不删除更新素材（zip/_extract 保留供重试）、不重启应用
    bat += "@if errorlevel 1 goto :copyFailed\r\n";
    bat += "@echo -------------------------------------------------------\r\n";
    bat += "@echo ##Update SUCCESSFUL(Maybe)\r\n";
    bat += "@echo -------------------------------------------------------\r\n";
    // bat += "@pause\r\n";
    bat += QString("@del \"%~dp0%1\"\r\n").arg(archive.fileName);
    bat += QString("@rd /S /Q \"%~dp0%1\"\r\n").arg(archive.extractDir);
    bat += QString("@start \"\" \"%~dp0%1\" --verify-update \"%2->%3\"\r\n").arg(QFileInfo(qApp->applicationFilePath()).fileName(),
                                                                                 version.toString(), downloadStatus.targetVer.toString());
    bat += "@del \"%~f0\" & exit /b 0\r\n"; // 自删与退出合并为一行：文件删除后不再需要读取后续行
    bat += ":copyFailed\r\n";
    bat += "@echo ##Copy FAILED - update aborted. Update files kept for retry.\r\n";
    bat += "@timeout /t 5 /NOBREAK\r\n";
    QFile file(appDir + "/copy.bat");
    if (!file.open(QFile::WriteOnly)) {
        qWarning() << "Failed to open bat for writing:" << file.fileName();
        return {};
    }
    const QByteArray batData = bat.toLocal8Bit(); // 系统代码页（中文=GBK），cmd 按此解析
    if (file.write(batData) != batData.size()) {
        qWarning() << "Failed to write bat file:" << file.fileName();
        return {};
    }
    file.close();
    return file.fileName();
}

QVersionNumber UpdateDialog::normalizeVersion(const QString& ver) {
    auto v = ver;
    if (v.startsWith('v')) v.removeFirst();
    while (v.count('.') > 2 && v.endsWith(".0")) // 移除多余的0，保留3位
        v.chop(2);
    return QVersionNumber::fromString(v);
}

QString UpdateDialog::toLocalTime(const QString& isoTime) {
    const auto dateTime = QDateTime::fromString(isoTime, Qt::ISODate);
    return dateTime.toLocalTime().toString("yyyy.MM.dd HH:mm");
}

void UpdateDialog::verifyUpdate(const QCoreApplication& app) {
    QCommandLineParser parser;
    // 用于验证更新成功与否，应由更新程序(.bat)调用，用法：`--verify-update 1.0->2.0`
    QCommandLineOption versionUpdate("verify-update", "verify update, e.g. `1.0->2.0`", "versionChange");
    // the `valueName` needs to be set if the option expects a value.
    parser.addOption(versionUpdate);
    parser.parse(app.arguments()); // parse 而非 process：未知参数不应让程序在启动时直接退出（GUI 程序无控制台，“双击没反应”难排查）
    if (parser.isSet(versionUpdate)) {
        // Qt 好像不支持`-v 1.0.1 1.0.2`这样多values，只能`-v 1.0.1 -v 1.0.2`
        auto versions = parser.value(versionUpdate).split("->");
        if (versions.size() == 2) {
            auto vFrom = QVersionNumber::fromString(versions.first());
            auto vTo = QVersionNumber::fromString(versions.last());
            qDebug() << "vFrom" << vFrom << "vTo" << vTo << "compare" << QVersionNumber::compare(vFrom, vTo);

            auto vCur = QVersionNumber::fromString(QCoreApplication::applicationVersion());
            if (vCur.normalized() == vTo.normalized()) {
                qDebug() << "Update success";
                sysTray.showMessage("Verify Update", "Update Success to v" + vTo.toString());
            } else if (vCur.normalized() == vFrom.normalized()) {
                qWarning() << "Update failed, version not change" << vFrom << vTo << vCur;
                sysTray.showMessage("Verify Update", "Update failed, version not change", QSystemTrayIcon::Critical);
            } else {
                qWarning() << "Update may failed, version changed but not to target" << vFrom << vTo << vCur;
                sysTray.showMessage("Verify Update", "Update may failed, version changed but not to target", QSystemTrayIcon::Warning);
            }
        } else
            qWarning() << "Invalid version change" << parser.value(versionUpdate);
    }
}

void UpdateDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    setWindowBlur(reinterpret_cast<HWND>(winId()));
    Util::setWindowRoundCorner(reinterpret_cast<HWND>(winId()));
    applyTheme(); // 同一实例会被复用：每次显示时同步当前主题
    MacTitleBar::positionBottomRight(this, 24);

    if (downloadStatus.reply)
        return;
    // 节流：距上次成功获取不足 10 分钟直接复用现有内容（匿名 API 限流 60 次/小时/IP）；
    // 「Check again」按钮不受限（直接调用 fetch，保留强制刷新路径）
    constexpr qint64 CacheMs = 10 * 60 * 1000;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (lastFetchMs != 0 && now >= lastFetchMs && now - lastFetchMs < CacheMs) // now>=lastFetch 防时钟回拨把窗口无限拉长
        qDebug() << "#Skip fetch (cached), age(ms):" << (now - lastFetchMs);
    else
        fetchGithubReleaseInfo();
}

void UpdateDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void UpdateDialog::applyTheme() {
    setStyleSheet(Theme::macDialogStyleSheet());
}


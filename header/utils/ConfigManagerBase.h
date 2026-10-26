#ifndef WIN_SWITCHER_CONFIGMANAGERBASE_H
#define WIN_SWITCHER_CONFIGMANAGERBASE_H

#include <QSettings>
#include <QProcess>
#include <QStandardPaths>
#include <QProcessEnvironment>

class ConfigManagerBase : public QObject {
    Q_OBJECT

protected:
    QSettings settings;
    QProcess proc_editor;

public:
    ConfigManagerBase(const ConfigManagerBase&) = delete;
    ConfigManagerBase& operator=(const ConfigManagerBase&) = delete;

    QVariant get(QAnyStringView key, const QVariant& defaultValue = QVariant()) {
        return settings.value(key, defaultValue);
    }

    void set(QAnyStringView key, const QVariant& value) {
        settings.setValue(key, value);
        settings.sync(); // 立即刷盘，避免崩溃丢失最近修改（对自启动等关键设置尤其重要）
    }

    /// Writes any unsaved changes to permanent storage, and reloads any settings that have been changed in the meantime by another application.
    void sync() {
        // This function is called automatically from QSettings's destructor and by the event loop at regular intervals,
        // so you normally don't need to call it yourself.
        settings.sync();
    }

    /// Edit config file with `notepad.exe` & emit `configEdited` signal when finished
    void editConfigFile() {
        if (proc_editor.state() == QProcess::Running) {
            qDebug() << "Editor is running";
            return;
        }

        QFile file(settings.fileName());
        if (!file.exists()) { // ensure exists
            qDebug() << "#Creating config file" << settings.fileName();
            if (file.open(QIODevice::WriteOnly))
                file.close();
            else // 打开失败仅补诊断日志；行为与成功路径一致（仍交由编辑器打开该路径）
                qWarning() << "Failed to create config file:" << settings.fileName();
        }

        qDebug() << "#Editing config file" << settings.fileName();
        // QProcess 只能同时连接一个进程
        // 显式定位系统绝对路径，彻底防御当前程序目录下的伪装 notepad.exe/notepad.bat 劫持
        const QString sysRoot = QProcessEnvironment::systemEnvironment().value("SystemRoot", "C:\\Windows");
        const QString notepadPath = QStandardPaths::findExecutable("notepad.exe", {sysRoot + "\\System32", "C:\\Windows\\System32"});
        const QString exeToRun = notepadPath.isEmpty() ? (sysRoot + "\\System32\\notepad.exe") : notepadPath;
        proc_editor.start(exeToRun, {settings.fileName()});
    }

protected:
    explicit ConfigManagerBase(const QString& filename) : settings(filename, QSettings::IniFormat) {
        connect(&proc_editor, &QProcess::finished, this, [this] {
            qDebug() << "#Config file edit finished";
            sync(); // reload
            emit configEdited();
        });
    }

signals:
    void configEdited(); // edit via `Settings` action, not other ways
};


#endif //WIN_SWITCHER_CONFIGMANAGERBASE_H

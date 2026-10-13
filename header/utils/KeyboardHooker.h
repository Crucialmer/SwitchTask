#ifndef WIN_SWITCHER_KEYBOARDHOOKER_H
#define WIN_SWITCHER_KEYBOARDHOOKER_H

#include <Windows.h>
#include <QWidget>
#include <QPointer>

class KeyboardHooker {
public:
    explicit KeyboardHooker(QWidget* _receiver);
    KeyboardHooker(const KeyboardHooker&) = delete;
    KeyboardHooker& operator=(const KeyboardHooker&) = delete;
    ~KeyboardHooker(); // RAII
    inline static QPointer<QWidget> receiver = nullptr; // QPointer：Widget 若被销毁会自动置空，回调不会悬垂访问

private:
    HHOOK h_keyboard = nullptr;
};


#endif //WIN_SWITCHER_KEYBOARDHOOKER_H

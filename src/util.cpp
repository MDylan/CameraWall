// util.cpp
#include "util.h"
#include "language.h"
#include <QMessageBox>
#include <QApplication>
#include <QAbstractButton>

namespace Util
{

    bool askOkCancel(QWidget *parent,
                     const QString &titleKey, const QString &titleFallback,
                     const QString &textKey, const QString &textFallback)
    {
        QMessageBox box(parent);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(Language::instance().t(titleKey, titleFallback));
        box.setText(Language::instance().t(textKey, textFallback));
        box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel);

        if (auto *bOk = box.button(QMessageBox::Ok))
            bOk->setText(Language::instance().t("btn.ok", "OK"));
        if (auto *bCancel = box.button(QMessageBox::Cancel))
            bCancel->setText(Language::instance().t("btn.cancel", "Mégse"));

        return box.exec() == QMessageBox::Ok;
    }

} // namespace Util

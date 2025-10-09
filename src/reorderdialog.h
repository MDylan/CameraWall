#pragma once
#include <QDialog>
#include <QList>
#include <QString>

class QListWidget;

class ReorderDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ReorderDialog(const QList<QString> &names, QWidget *parent = nullptr);

    // Az ÚJ sorrend az EREDETI indexekkel (new-to-old):
    // pl. [2,0,1] = először a 2-es eredeti elem, aztán a 0., majd az 1.
    QList<int> order() const;

private:
    QListWidget *list{};
};

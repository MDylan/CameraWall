#include "reorderdialog.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QDialogButtonBox>

ReorderDialog::ReorderDialog(const QList<QString> &names, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Kamerák sorrendje"));
    auto *lay = new QVBoxLayout(this);

    list = new QListWidget(this);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setDragEnabled(true);
    list->setAcceptDrops(true);

    // feltöltés: a UserRole-ba eltároljuk az EREDETI indexet
    for (int i = 0; i < names.size(); ++i)
    {
        auto *it = new QListWidgetItem(names[i], list);
        it->setData(Qt::UserRole, i);
    }

    lay->addWidget(list);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(btns);
}

QList<int> ReorderDialog::order() const
{
    QList<int> out;
    out.reserve(list->count());
    // az aktuális (vizuális) sorrendben végigmegyünk,
    // és összegyűjtjük az EREDETI indexeket
    for (int row = 0; row < list->count(); ++row)
    {
        const auto *it = list->item(row);
        out.push_back(it->data(Qt::UserRole).toInt());
    }
    return out;
}

#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QListWidget>

#include "../Bridge.hpp"

class MenuItemDelegate : public QStyledItemDelegate {
    public:
        // inherit the base constructor
        using QStyledItemDelegate::QStyledItemDelegate;

        QSize sizeHint(const QStyleOptionViewItem& opt,
                       const QModelIndex& idx) const override;
        void paint(QPainter *p,
                   const QStyleOptionViewItem& opt,
                   const QModelIndex& idx) const override;
};

class Sidebar : public QWidget {
    Q_OBJECT
    public:
        Sidebar(Bridge& bridge, QWidget *parent = nullptr);
    signals:
        void sessionsListRequested();
    private slots:
        void renderSessionList(std::vector<Uuid> list);
    private:
        Bridge& bridge;
        QVBoxLayout *root = nullptr;
        QListWidget *menu_list = nullptr;

};
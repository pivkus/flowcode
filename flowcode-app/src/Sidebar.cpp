#include <QPainter>
#include <QPushButton>
#include <QString>
#include <QSignalBlocker>

#include <Sidebar.hpp>
#include <Theme.hpp>

namespace {
    constexpr int menuItemPadding = 10; // px
    constexpr int menuItemBottomGap = 4;
}

QSize MenuItemDelegate::sizeHint(const QStyleOptionViewItem& opt,
                                 const QModelIndex& idx) const {

    int height = opt.fontMetrics.height() + 2*menuItemPadding + menuItemBottomGap;
    // "width" can be 0 since QListView will stretches the rows to the sidebars viewport
    return { 0, height};
}

void MenuItemDelegate::paint(QPainter *p,
                             const QStyleOptionViewItem& opt,
                             const QModelIndex& idx) const {
    bool is_hovered = (opt.state & QStyle::State_MouseOver) ||
                      (opt.state & QStyle::State_Selected);

    auto to_fill = opt.rect.adjusted(0, 0, 0, -menuItemBottomGap);
    paintBackground(*p, to_fill, is_hovered ? theme.hover : theme.surface);

    auto to_write = to_fill.adjusted(menuItemPadding, 0, -menuItemPadding, 0);
    // adds "..." to the right if the text is too long to fit in width
    auto text = opt.fontMetrics.elidedText(
        idx.data(Qt::DisplayRole).toString(),
        Qt::ElideRight,
        to_write.width()
    );

    p->setPen(theme.text);
    p->drawText(to_write, Qt::AlignLeft | Qt::AlignVCenter, text);

}

Sidebar::Sidebar(Bridge& bridge, QWidget *parent)
: QWidget(parent), bridge(bridge)
{
    connect(this, &Sidebar::sessionsListRequested, &bridge, &Bridge::sessionsListRequested);
    connect(&bridge, &Bridge::sessionListReceived, this, &Sidebar::renderSessionList);
    connect(&bridge, &Bridge::sessionNamed, this, &Sidebar::nameSession);


    // Draw the sidebar surface independently of the session background.
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("sidebar");
    setStyleSheet(QString("QWidget#sidebar { background-color: %1; }")
                      .arg(theme.surface.name()));

    setFixedWidth(300);

    root = new QVBoxLayout(this);

    menuList = new QListWidget(this);
    auto listPalette = menuList->palette();
    listPalette.setColor(QPalette::Base, theme.surface);
    menuList->setPalette(listPalette);
    menuList->setMouseTracking(true);
    menuList->setItemDelegate(new MenuItemDelegate(menuList));

    // Load sessions when the user changes the current item.
    connect(menuList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *cur){
        if (!cur) return;
        // Load the Uuid from the list item
        auto id = cur->data(Qt::UserRole).value<Uuid>();
        emit this->sessionSelected(id);
    });

    auto *create_button = new QPushButton("Create Session", this);
    create_button->setStyleSheet("padding: 10px;");
    connect(create_button, &QPushButton::clicked, this, &Sidebar::newSession);

    root->addWidget(create_button, 1);
    root->addWidget(menuList, 9);

    setLayout(root);

    emit sessionsListRequested();
}

QListWidgetItem* Sidebar::addSession(Uuid id, QString name){
    // this is temporary until I add a proper name to sessions
    if (name.isEmpty()) name = "Unnamed Session";

    auto *item = new QListWidgetItem(name, menuList);
    sessionItems[id] = item;
    // Each menu bar item will store its uuid value
    item->setData(Qt::UserRole, QVariant::fromValue(id));
    return item;
}

void Sidebar::addSelectSession(Uuid id){
    auto *item = addSession(id);
    // addSession inserts the item to the end, move to the front
    menuList->takeItem(menuList->row(item));
    menuList->insertItem(0, item);
    menuList->setCurrentItem(item);
}

void Sidebar::renderSessionList(std::vector<Uuid> list, std::vector<std::string> names){
    // Populating saved sessions must not automatically open the first one.
    const QSignalBlocker blocker(menuList);
    menuList->clear();
    sessionItems.clear();

    for (size_t i = 0; i < list.size(); ++i) {
        addSession(list[i], QString::fromStdString(names[i]));

    }
}

void Sidebar::nameSession(Uuid id, const QString& name){
    if (auto it = sessionItems.find(id); it != sessionItems.end()){
        it->second->setText(name);
    }
}

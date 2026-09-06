#include "Sidebar.hpp"

#include <QPushButton>
#include <QString>


namespace {
    constexpr int menuItemPadding = 6; // px
    constexpr int menuItemBottomGap = 4;
    // TODO: Hardcoding colors for now, Qt gets pallete colors from the OS somehow so it's not
    // consistant across platforms - figure out how to deal with colors and define a custom pallete
    const QColor menuItemColor{"#3c3c3c"};
    const QColor menuItemTextColor{"#e1e1e1"};
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

    auto to_fill = opt.rect.adjusted(0, 0, 0, -menuItemBottomGap);
    p->fillRect(to_fill, menuItemColor);
    
    auto to_write = to_fill.adjusted(menuItemPadding, 0, -menuItemPadding, 0);
    // adds "..." to the right if the text is too long to fit in width
    auto text = opt.fontMetrics.elidedText(
        idx.data(Qt::DisplayRole).toString(),
        Qt::ElideRight,
        to_write.width()
    );

    p->setPen(menuItemTextColor);
    p->drawText(to_write, Qt::AlignLeft | Qt::AlignVCenter, text);

}

Sidebar::Sidebar(Bridge& bridge, QWidget *parent)
: QWidget(parent), bridge(bridge)
{
    connect(this, &Sidebar::sessionsListRequested, &bridge, &Bridge::sessionsListRequested);
    connect(&bridge, &Bridge::sessionListReceived, this, &Sidebar::renderSessionList);

    
    // this attribute is needed to draw the backgrounf using style sheets
    setAttribute(Qt::WA_StyledBackground, true);
    
    setFixedWidth(200);
    // setStyleSheet("Sidebar { background: #d0d0d0; }");

    root = new QVBoxLayout(this);

    menu_list = new QListWidget(this);
    menu_list->setItemDelegate(new MenuItemDelegate(menu_list));

    // Qt keeps track of "current" item, changes it on click, on insertion of first item it will became
    // current and get loaded - this is good right now
    connect(menu_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *cur){
        if (!cur) return;
        // Load the Uuid from the list item
        auto id = cur->data(Qt::UserRole).value<Uuid>();
        emit this->sessionSelected(id);
    });
    
    auto *create_button = new QPushButton("Create Session", this);
    connect(create_button, &QPushButton::clicked, this, &Sidebar::newSession);

    root->addWidget(create_button, 1);
    root->addWidget(menu_list, 9);

    setLayout(root);

    emit sessionsListRequested();
}

QListWidgetItem* Sidebar::addSession(Uuid id){
    // this is temporary until I add a proper name to sessions
    std::string str_uuid = id.to_string();
    auto name = QString::fromStdString(str_uuid.substr(str_uuid.length() - 6));

    auto *item = new QListWidgetItem(name, menu_list);
    // Each menu bar item will store its uuid value
    item->setData(Qt::UserRole, QVariant::fromValue(id));
    return item;
}

void Sidebar::addSelectSession(Uuid id){ 
    auto *item = addSession(id);
    menu_list->setCurrentItem(item);
}

void Sidebar::renderSessionList(std::vector<Uuid> list){

    for (const auto& uuid : list){ addSession(uuid); }
}
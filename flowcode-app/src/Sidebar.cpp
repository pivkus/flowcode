#include <QPainter>
#include <QPushButton>
#include <QString>

#include <Sidebar.hpp>
#include <Theme.hpp>

namespace {
    constexpr int menuItemPadding = 6; // px
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

    
    // this attribute is needed to draw the backgrounf using style sheets
    setAttribute(Qt::WA_StyledBackground, true);
    
    setFixedWidth(200);

    root = new QVBoxLayout(this);

    menuList = new QListWidget(this);
    menuList->setMouseTracking(true);
    menuList->setItemDelegate(new MenuItemDelegate(menuList));

    // Qt keeps track of "current" item, changes it on click, on insertion of first item it will became
    // current and get loaded - this is good right now
    connect(menuList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *cur){
        if (!cur) return;
        // Load the Uuid from the list item
        auto id = cur->data(Qt::UserRole).value<Uuid>();
        emit this->sessionSelected(id);
    });
    
    auto *create_button = new QPushButton("Create Session", this);
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
    menuList->setCurrentItem(item);
}

void Sidebar::renderSessionList(std::vector<Uuid> list){
    menuList->clear();
    sessionItems.clear();

    for (const auto& uuid : list){ addSession(uuid); }
}

void Sidebar::nameSession(Uuid id, const QString& name){
    if (auto it = sessionItems.find(id); it != sessionItems.end()){
        it->second->setText(name);
    }
}

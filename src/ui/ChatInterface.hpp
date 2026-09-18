#pragma once

#include <utility>
#include <map>

#include <QWidget>
#include <QPointer>
#include <QString>
#include <QVBoxLayout>

#include "../Bridge.hpp"

class QLabel;
class QSvgRenderer;

class OutputTextBlock : public QWidget {
    Q_OBJECT
    public:
        OutputTextBlock(QWidget *parent = nullptr);
        void append(const QString &text);
    private:
        QLabel *label = nullptr;
};



class ReasoningBlock : public QWidget {
    Q_OBJECT
    public:
        explicit ReasoningBlock(QWidget *parent = nullptr);
        void append(const QString &text);
        QSize sizeHint() const override;
    protected:
        void paintEvent(QPaintEvent *event) override;
    private:
        QString reasoning_text;
        QColor bg_color;
        QSvgRenderer *thinking_icon = nullptr;

        static constexpr int horizontal_pad = 10;
        static constexpr int vertical_pad = 8;
        static constexpr int text_gap = 10;
        static constexpr int corner_radius = 8;
};



class UserpromptBlock : public QWidget {
    Q_OBJECT
    public:
        UserpromptBlock(const QString &prompt, QWidget *parent = nullptr);
    private:
        QLabel *label = nullptr;
};

class ErrorBlock : public QWidget {
    Q_OBJECT
    public:
        ErrorBlock(const QString &msg, QWidget *parent = nullptr);
    private:
        QLabel *label = nullptr;
};

class ToolInfoBlock : public QWidget {
    Q_OBJECT
    public:
        ToolInfoBlock(QWidget *parent = nullptr);
        void start(const QString &name, ToolCallId tcid);
        void finish(ToolCallId tcid, bool status);
    private:
        QLabel *label = nullptr;

};

class ChatInterface : public QWidget {
    Q_OBJECT
    public:
        ChatInterface(QWidget *parent = nullptr);

        void appendText(const QString &text);
        void appendReasoning(const QString &text);
        void appendToolStarted(const QString &name, ToolCallId tcid);
        void appendToolFinished(ToolCallId tcid, bool status);

        void appendError(const QString &error);
        void appendPrompt(const QString &prompt);

        void finishTurn();
    private:
        template<class T, class... Args>
        T* addBlock(Args&&... args){
            auto *block = new T(std::forward<Args>(args)..., this);
            // Insert before the trailing stretch.
            feed_layout->insertWidget(feed_layout->count() - 1, block);
            current_block = block;
            return block;
        }

        template<class T>
        T* ensureCurrentBlock(){
            if (auto *block = qobject_cast<T*>(current_block.data())) return block;
            return addBlock<T>();
        }

        QVBoxLayout* feed_layout = nullptr;
        QPointer<QWidget> current_block;
        std::map<ToolCallId, QPointer<ToolInfoBlock>> active_tools;
};

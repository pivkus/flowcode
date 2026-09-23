#pragma once

#include <utility>
#include <map>

#include <QWidget>
#include <QPointer>
#include <QString>
#include <QVBoxLayout>
#include <QMap>

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
        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;
        int heightForWidth(int width) const override;
    protected:
        void paintEvent(QPaintEvent *event) override;
    private:
        QString prompt_text;

        static constexpr int horizontal_pad = 10;
        static constexpr int vertical_pad = 8;
        static constexpr int corner_radius = 8;
        static constexpr int bubble_width_percent = 66;

        static constexpr int text_flags =
                Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap;
};

class ErrorBlock : public QWidget {
    Q_OBJECT
    public:
        ErrorBlock(const QString &msg, QWidget *parent = nullptr);
        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;
        int heightForWidth(int width) const override;
    protected:
        void paintEvent(QPaintEvent *event) override;
    private:
        QString errmsg;
        int layoutText(int width, QPainter *painter = nullptr) const;

        static constexpr int horizontal_pad = 10;
        static constexpr int vertical_pad = 8;
        static constexpr int corner_radius = 8;

        static constexpr int text_flags =
                Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap;
};

// iterate all in block - name, store tcid (for start, finish), start/finish state, finish status
// map tcid -> name, ordered map tcid -> opt<finish_state>
class ToolInfoBlock : public QWidget {
    Q_OBJECT
    public:
        ToolInfoBlock(QWidget *parent = nullptr);
        void start(const QString &name, ToolCallId tcid);
        void finish(ToolCallId tcid, bool status);

        QSize minimumSizeHint() const override;
        QSize sizeHint() const override;
    protected:
        void paintEvent(QPaintEvent *event) override;
    private:


        enum class Status {
            IN_PROGRESS,
            SUCCEEDED,
            FAILED
        };
        std::map<ToolCallId, Status> state;
        std::map<ToolCallId, QString> named;
        QSvgRenderer *tool_icon = nullptr;

        static constexpr int horizontal_pad = 10;
        static constexpr int vertical_pad = 8;
        static constexpr int icon_gap = 5;

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

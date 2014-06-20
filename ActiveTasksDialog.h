#pragma once

#include <unordered_map>

#include <QDialog>

class QTreeWidgetItem;
class QProgressBar;
class QTreeWidget;
class QCheckBox;

namespace Evernus
{
    class ActiveTasksDialog
        : public QDialog
    {
        Q_OBJECT

    public:
        explicit ActiveTasksDialog(QWidget *parent = nullptr);
        virtual ~ActiveTasksDialog() = default;

    public slots:
        void addNewTaskInfo(quint32 taskId, const QString &description);
        void addNewSubTaskInfo(quint32 taskId, quint32 parentTask, const QString &description);
        void setTaskStatus(quint32 taskId, const QString &error);

    private slots:
        void autoCloseSave(bool enabled);

    private:
        struct SubTaskInfo
        {
            size_t mCount = 0;
            bool mError = false;
        };

        QTreeWidget *mTaskWidget = nullptr;
        QProgressBar *mTotalProgressWidget = nullptr;
        QCheckBox *mAutoCloseBtn = nullptr;

        std::unordered_map<quint32, QTreeWidgetItem *> mTaskItems;
        std::unordered_map<quint32, SubTaskInfo> mSubTaskInfo;

        bool mHadError = false;

        void fillTaskItem(quint32 taskId, QTreeWidgetItem *item, const QString &description);
    };
}

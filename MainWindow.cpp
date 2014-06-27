#include <QCoreApplication>
#include <QCloseEvent>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QSettings>
#include <QMenuBar>
#include <QLabel>
#include <QDebug>

#include "CharacterManagerDialog.h"
#include "ActiveTasksDialog.h"
#include "PreferencesDialog.h"
#include "CharacterWidget.h"
#include "MenuBarWidget.h"
#include "Repository.h"

#include "MainWindow.h"

namespace Evernus
{
    const QString MainWindow::settingsMaximizedKey = "mainWindow/maximized";
    const QString MainWindow::settingsPosKey = "mainWindow/pos";
    const QString MainWindow::settingsSizeKey = "mainWindow/size";

    MainWindow::MainWindow(const Repository<Character> &characterRepository,
                           const Repository<Key> &keyRepository,
                           APIManager &apiManager,
                           QWidget *parent,
                           Qt::WindowFlags flags)
        : QMainWindow{parent, flags}
        , mCharacterRepository{characterRepository}
        , mKeyRepository{keyRepository}
        , mApiManager{apiManager}
    {
        readSettings();
        createMenu();
        createMainView();
        createStatusBar();

        setWindowIcon(QIcon{":/images/main-icon.png"});
    }

    void MainWindow::showAsSaved()
    {
        if (mShowMaximized)
            showMaximized();
        else
            show();
    }

    void MainWindow::showCharacterManagement()
    {
        if (mCharacterManagerDialog == nullptr)
        {
            mCharacterManagerDialog = new CharacterManagerDialog{mCharacterRepository, mKeyRepository, this};
            connect(mCharacterManagerDialog, &CharacterManagerDialog::refreshCharacters, this, &MainWindow::refreshCharacters);
            connect(this, &MainWindow::charactersChanged, mCharacterManagerDialog, &CharacterManagerDialog::updateCharacters);
            connect(mCharacterManagerDialog, &CharacterManagerDialog::charactersChanged,
                    mMenuWidget, &MenuBarWidget::refreshCharacters);
        }

        mCharacterManagerDialog->exec();
    }

    void MainWindow::showPreferences()
    {
        PreferencesDialog dlg{this};
        dlg.exec();
    }

    void MainWindow::showAbout()
    {
        QMessageBox::about(this,
                           tr("About Evernus"),
                           QString{tr("Evernus %1\nCreated by Pete Butcher\nAll donations are welcome :)")}
                               .arg(QCoreApplication::applicationVersion()));
    }

    void MainWindow::showError(const QString &info)
    {
        qCritical() << info;
        QMessageBox::warning(this, tr("Error"), info);
    }

    void MainWindow::addNewTaskInfo(quint32 taskId, const QString &description)
    {
        if (mActiveTasksDialog == nullptr)
        {
            mActiveTasksDialog = new ActiveTasksDialog{this};
            connect(this, &MainWindow::newTaskInfoAdded, mActiveTasksDialog, &ActiveTasksDialog::addNewTaskInfo);
            connect(this, &MainWindow::newSubTaskInfoAdded, mActiveTasksDialog, &ActiveTasksDialog::addNewSubTaskInfo);
            connect(this, &MainWindow::taskStatusChanged, mActiveTasksDialog, &ActiveTasksDialog::setTaskStatus);
            mActiveTasksDialog->setModal(true);
            mActiveTasksDialog->show();
        }
        else
        {
            mActiveTasksDialog->show();
            mActiveTasksDialog->activateWindow();
        }

        emit newTaskInfoAdded(taskId, description);
    }

    void MainWindow::updateStatus()
    {
        if (mCurrentCharacterId != Character::invalidId)
        {
            const auto character = mCharacterRepository.find(mCurrentCharacterId);
            QLocale locale;

            mStatusWalletLabel->setText(QString{tr("Wallet: <strong>%1</strong>")}
                .arg(locale.toCurrencyString(character.getISK() / 100., "ISK")));
        }
        else
        {
            mStatusWalletLabel->setText(QString{});
        }
    }

    void MainWindow::setCharacter(Character::IdType id)
    {
        mCurrentCharacterId = id;
        updateStatus();
    }

    void MainWindow::closeEvent(QCloseEvent *event)
    {
        writeSettings();
        event->accept();
    }

    void MainWindow::readSettings()
    {
        QSettings settings;

        const auto pos = settings.value(settingsPosKey).toPoint();
        const auto size = settings.value(settingsSizeKey, QSize{600, 400}).toSize();

        resize(size);
        move(pos);

        mShowMaximized = settings.value(settingsMaximizedKey, false).toBool();
    }

    void MainWindow::writeSettings()
    {
        QSettings settings;
        settings.setValue(settingsPosKey, pos());
        settings.setValue(settingsSizeKey, size());
        settings.setValue(settingsMaximizedKey, isMaximized());
    }

    void MainWindow::createMenu()
    {
        auto bar = menuBar();

        auto fileMenu = bar->addMenu(tr("&File"));
        fileMenu->addAction(tr("&Manage characters..."), this, SLOT(showCharacterManagement()));
        fileMenu->addAction(tr("&Preferences..."), this, SLOT(showPreferences()), Qt::CTRL + Qt::Key_O);
        fileMenu->addSeparator();
        fileMenu->addAction(tr("E&xit"), this, SLOT(close()));

        auto helpMenu = bar->addMenu(tr("&Help"));
        helpMenu->addAction(tr("&About..."), this, SLOT(showAbout()));

        mMenuWidget = new MenuBarWidget{mCharacterRepository, this};
        bar->setCornerWidget(mMenuWidget);
        connect(this, &MainWindow::charactersChanged, mMenuWidget, &MenuBarWidget::refreshCharacters);
        connect(mMenuWidget, &MenuBarWidget::currentCharacterChanged, this, &MainWindow::setCharacter);
    }

    void MainWindow::createMainView()
    {
        auto tabs = new QTabWidget{this};
        setCentralWidget(tabs);

        auto charTab = new CharacterWidget{mCharacterRepository, this};
        tabs->addTab(charTab, tr("Character"));
        connect(mMenuWidget, &MenuBarWidget::currentCharacterChanged, charTab, &CharacterWidget::setCharacter);
    }

    void MainWindow::createStatusBar()
    {
        mStatusWalletLabel = new QLabel{this};
        statusBar()->addPermanentWidget(mStatusWalletLabel);
    }
}

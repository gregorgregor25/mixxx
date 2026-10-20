#pragma once

#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QHash>
#include <QRect>
#include <QStringList>
#include <memory>

#include "control/controlpushbutton.h"
#include "preferences/constants.h"
#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/ui_dlgpreferencesdlg.h"
#include "preferences/settingsmanager.h"
#include "preferences/usersettings.h"

class ControllerManager;
class DlgPrefControllers;
class DlgPrefSound;
class EffectsManager;
class Library;
class SoundManager;
class VinylControlManager;

namespace mixxx {
class ScreensaverManager;
namespace skin {
class SkinLoader;
} // namespace skin
} // namespace mixxx

class DlgPreferences : public QDialog, public Ui::DlgPreferencesDlg {
    Q_OBJECT
  public:
    struct PreferencesPage {
        PreferencesPage() {
        }
        PreferencesPage(DlgPreferencePage* pDlg, QTreeWidgetItem* pTreeItem)
                : pDlg(pDlg),
                  pTreeItem(pTreeItem) {
        }

        DlgPreferencePage* pDlg;
        QTreeWidgetItem* pTreeItem;
        QString iconFile;
    };

    DlgPreferences(
            std::shared_ptr<mixxx::ScreensaverManager> pScreensaverManager,
            std::shared_ptr<mixxx::skin::SkinLoader> pSkinLoader,
            std::shared_ptr<SoundManager> pSoundManager,
            std::shared_ptr<ControllerManager> pControllerManager,
            std::shared_ptr<VinylControlManager> pVCManager,
            std::shared_ptr<EffectsManager> pEffectsManager,
            std::shared_ptr<SettingsManager> pSettingsManager,
            std::shared_ptr<Library> pLibrary,
            bool includeWaveformPreferences = true);
    virtual ~DlgPreferences();

    void addPageWidget(const PreferencesPage& page,
            const QString& pageTitle,
            const QString& iconFile,
            const QString& pageId = QString());
    Q_INVOKABLE bool hasPage(const QString& pageId) const;
    Q_INVOKABLE bool showPage(const QString& pageId);
    Q_INVOKABLE void clearPageScope();
    void removePageWidget(DlgPreferencePage* pWidget);
    void expandTreeItem(QTreeWidgetItem* pItem);
    void switchToPage(const QString& pageTitle, DlgPreferencePage* pPage);

  public slots:
    void changePage(QTreeWidgetItem* pCurrent, QTreeWidgetItem* pPrevious);
    void showSoundHardwarePage(
            std::optional<mixxx::preferences::SoundHardwareTab> tab =
                    std::nullopt);
    void showSoundHardwareInputPage();
    void slotButtonPressed(QAbstractButton* pButton);
    void reject() override;
  signals:
    void closeDlg();
    void showDlg();

    // Emitted just after the user clicks Apply or OK.
    void applyPreferences();
    // Emitted if the user clicks Cancel
    void cancelPreferences();
    // Emitted if the user clicks Reset to Defaults.
    void resetToDefaults();

    void reloadUserInterface();
    void tooltipModeChanged(mixxx::preferences::Tooltips tooltipMode);
    void menuBarAutoHideChanged();

  protected:
    bool eventFilter(QObject*, QEvent*) override;
    void changeEvent(QEvent* pEvent) override;
    void moveEvent(QMoveEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

  private:
    DlgPreferencePage* currentPage();
    void fixSliderStyle();
    QList<PreferencesPage> m_allPages;
    QHash<QString, DlgPreferencePage*> m_namedPages;
    QHash<QTreeWidgetItem*, bool> m_pageTreeHiddenBeforeScope;
    DlgPreferencePage* m_scopedPage = nullptr;
    bool m_pageScopedApply = false;
    void onShow();
    void onHide();
    QRect getDefaultGeometry();

    QAbstractButton* m_pApplyButton;
    QAbstractButton* m_pAcceptButton;

    QStringList m_geometry;
    UserSettingsPointer m_pConfig;
    std::shared_ptr<EffectsManager> m_pEffectsManager;
    std::unique_ptr<DlgPrefSound> m_pSoundDlg;
    PreferencesPage m_soundPage;
    DlgPrefControllers* m_pControllersDlg;

    QSize m_pageSizeHint;

    QDir m_iconsPath;
    bool pendingConfigValidOnAllPages();
};

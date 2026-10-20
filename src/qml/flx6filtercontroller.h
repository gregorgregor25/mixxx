#pragma once

#include <QString>
#include <QTimer>
#include <memory>

#include "control/controlobject.h"
#include "effects/defs.h"
#include "preferences/usersettings.h"

class EffectsManager;

namespace mixxx::qml {

class Flx6FilterController final : public QObject {
  public:
    Flx6FilterController(UserSettingsPointer pConfig,
            std::shared_ptr<EffectsManager> pEffectsManager,
            QObject* pParent = nullptr);
    ~Flx6FilterController() override;

  private:
    struct Deck {
        EffectSlotPointer slot;
        EffectParameterPointer parameter;
        double baselineQ{0.0};
        double lastAppliedQ{0.0};
        QString lastAppliedEffectId;
        int filterMode{0};
        bool hasBaseline{false};
        bool hasAppliedQ{false};
        bool everApplied{false};
        bool pendingRestore{false};
    };

    void reconcile(int deckIndex);
    void restore(int deckIndex);
    bool selectFilter(int deckIndex, int filterMode);
    void loadRestorationState(int deckIndex);
    void persist();
    void scheduleSave();
    double normalizedResonance(int deckIndex) const;
    double savedResonance(int deckIndex, int filterMode) const;

    UserSettingsPointer m_pConfig;
    std::shared_ptr<EffectsManager> m_pEffectsManager;
    std::unique_ptr<ControlObject> m_enabled;
    std::unique_ptr<ControlObject> m_resonance[2];
    std::unique_ptr<ControlObject> m_filterMode[2];
    std::unique_ptr<ControlObject> m_active[2];
    std::unique_ptr<ControlObject> m_available;
    std::unique_ptr<ControlObject> m_saveError;
    Deck m_decks[2];
    QTimer m_saveTimer;
    bool m_syncingControls{false};
    bool m_switchingEffect{false};
};

}

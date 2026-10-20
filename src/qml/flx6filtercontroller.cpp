#include "qml/flx6filtercontroller.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

#include "effects/backends/builtin/flx6resonancecurve.h"
#include "effects/backends/effectsbackendmanager.h"
#include "effects/chains/quickeffectchain.h"
#include "effects/effectparameter.h"
#include "effects/effectparameterslotbase.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"

namespace {
const QString kGroup = QStringLiteral("[Flx6Filter]");
const QString kStockFilterId = QStringLiteral("org.mixxx.effects.filter");
const QString kFilterId = QStringLiteral("org.mixxx.effects.flx6resonantfilter");
const QString kMoogId = QStringLiteral("org.mixxx.effects.moogladder4filter");
using mixxx::flx6filter::clampNormalized;
using mixxx::flx6filter::finiteBits;
using mixxx::flx6filter::kFlatQ;
using mixxx::flx6filter::kResonanceScaleVersion;
using mixxx::flx6filter::kWildQ;
using mixxx::flx6filter::legacyQFromNormalized;
using mixxx::flx6filter::normalizedFromQ;
using mixxx::flx6filter::qFromNormalized;

ConfigKey setting(const char* name) {
    return ConfigKey(kGroup, QString::fromLatin1(name));
}

ConfigKey resonanceSetting(int deckIndex, int filterMode) {
    return ConfigKey(kGroup, QStringLiteral("%1resonance%2")
            .arg(filterMode == 1 ? QStringLiteral("moog_") : QString(),
                    QString::number(deckIndex + 1)));
}

ConfigKey restorationSetting(int deckIndex, int filterMode, const QString& field) {
    const QString number = QString::number(deckIndex + 1);
    if (filterMode == 1) {
        return ConfigKey(kGroup, QStringLiteral("moog_%1%2").arg(field, number));
    }
    return ConfigKey(kGroup, QStringLiteral("%1%2").arg(field, number));
}

QString filterId(int filterMode) {
    return filterMode == 1 ? kMoogId : kFilterId;
}

QString resonanceParameterId(int filterMode) {
    return filterMode == 1 ? QStringLiteral("resonance") : QStringLiteral("q");
}

}

namespace mixxx::qml {

Flx6FilterController::Flx6FilterController(UserSettingsPointer pConfig,
        std::shared_ptr<EffectsManager> pEffectsManager,
        QObject* pParent)
        : QObject(pParent),
          m_pConfig(std::move(pConfig)),
          m_pEffectsManager(std::move(pEffectsManager)),
          m_enabled(std::make_unique<ControlObject>(setting("enabled"))),
          m_available(std::make_unique<ControlObject>(setting("available"))),
          m_saveError(std::make_unique<ControlObject>(setting("save_error"))) {
    m_available->setReadOnly();
    m_saveError->setReadOnly();
    m_available->forceSet(1.0);
    m_saveError->forceSet(0.0);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(300);
    connect(&m_saveTimer, &QTimer::timeout, this, &Flx6FilterController::persist);

    const ConfigKey scaleVersionKey = setting("resonance_scale_version");
    const int scaleVersion = m_pConfig->getValue<int>(scaleVersionKey, 0);
    if (scaleVersion < kResonanceScaleVersion) {
        for (int i = 0; i < 2; ++i) {
            const ConfigKey resonanceConfigKey =
                    setting(i == 0 ? "resonance1" : "resonance2");
            const double oldNormalized = clampNormalized(
                    m_pConfig->getValue<double>(resonanceConfigKey, 0.0));
            const double oldQ = legacyQFromNormalized(scaleVersion, oldNormalized);
            const double newNormalized = normalizedFromQ(oldQ);
            if (oldQ > kWildQ) {
                qWarning() << "FLX6 filter resonance deck" << i + 1 << "clamped from Q"
                           << oldQ << "to" << kWildQ;
            }
            m_pConfig->setValue(resonanceConfigKey, newNormalized);
            const ConfigKey appliedKey(kGroup,
                    QStringLiteral("last_applied_q%1").arg(i + 1));
            const ConfigKey appliedEffectKey(kGroup,
                    QStringLiteral("last_applied_effect_id%1").arg(i + 1));
            if (m_pConfig->exists(appliedKey) && !m_pConfig->exists(appliedEffectKey)) {
                m_pConfig->setValue(appliedEffectKey,
                        scaleVersion < 3 ? kStockFilterId : kFilterId);
            }
        }
        m_pConfig->setValue(scaleVersionKey, kResonanceScaleVersion);
        persist();
    }

    for (int i = 0; i < 2; ++i) {
        const QByteArray suffix = QByteArray::number(i + 1);
        const QByteArray resonanceKey = "resonance" + suffix;
        const QByteArray activeKey = "active" + suffix;
        m_resonance[i] = std::make_unique<ControlObject>(setting(resonanceKey.constData()));
        m_filterMode[i] = std::make_unique<ControlObject>(
                setting(i == 0 ? "filter_mode1" : "filter_mode2"));
        m_active[i] = std::make_unique<ControlObject>(setting(activeKey.constData()));
        m_active[i]->setReadOnly();
        m_active[i]->forceSet(0.0);

        const QString group = QStringLiteral("[Channel%1]").arg(i + 1);
        const auto chain = m_pEffectsManager->getQuickEffectChain(group);
        if (chain) {
            m_decks[i].slot = chain->getEffectSlot(0);
            if (m_decks[i].slot) {
                connect(m_decks[i].slot.data(),
                        &EffectSlot::effectChanged,
                        this,
                        [this, i]() { if (!m_switchingEffect) reconcile(i); });
                connect(m_decks[i].slot.data(),
                        &EffectSlot::parametersChanged,
                        this,
                        [this, i]() { if (!m_switchingEffect) reconcile(i); });
            }
        }

        auto& deck = m_decks[i];
        const int savedMode = m_pConfig->getValue<int>(
                setting(i == 0 ? "filter_mode1" : "filter_mode2"), 0) == 1 ? 1 : 0;
        deck.filterMode = deck.slot && deck.slot->id() == kMoogId ? 1 :
                deck.slot && deck.slot->id() == kFilterId ? 0 : savedMode;
        m_filterMode[i]->forceSet(deck.filterMode);
        const double value = savedResonance(i, deck.filterMode);
        m_resonance[i]->forceSet(value);
        connect(m_resonance[i].get(),
                &ControlObject::valueChanged,
                this,
                [this, i](double raw) {
                    if (m_syncingControls) {
                        return;
                    }
                    const double value = clampNormalized(raw);
                    if (value != raw) {
                        m_syncingControls = true;
                        m_resonance[i]->forceSet(value);
                        m_syncingControls = false;
                    }
                    m_pConfig->setValue(resonanceSetting(i, m_decks[i].filterMode), value);
                    scheduleSave();
                    reconcile(i);
                });
        connect(m_filterMode[i].get(),
                &ControlObject::valueChanged,
                this,
                [this, i](double raw) {
                    if (m_syncingControls) {
                        return;
                    }
                    const int requested = finiteBits(raw) && raw >= 0.5 ? 1 : 0;
                    if (requested == m_decks[i].filterMode &&
                            m_decks[i].slot &&
                            m_decks[i].slot->id() == filterId(requested)) {
                        return;
                    }
                    if (!selectFilter(i, requested)) {
                        m_syncingControls = true;
                        m_filterMode[i]->forceSet(m_decks[i].filterMode);
                        m_syncingControls = false;
                    }
                });
    }

    const bool enabled = m_pConfig->getValue<bool>(setting("enabled"), false);
    m_enabled->forceSet(enabled ? 1.0 : 0.0);
    for (int i = 0; i < 2; ++i) {
        loadRestorationState(i);
    }
    connect(m_enabled.get(),
            &ControlObject::valueChanged,
            this,
            [this](double raw) {
                if (m_syncingControls) {
                    return;
                }
                const bool enabled = finiteBits(raw) && raw > 0.5;
                if (raw != (enabled ? 1.0 : 0.0)) {
                    m_syncingControls = true;
                    m_enabled->forceSet(enabled ? 1.0 : 0.0);
                    m_syncingControls = false;
                }
                m_pConfig->setValue(setting("enabled"), enabled);
                if (enabled) {
                    for (int i = 0; i < 2; ++i) {
                        auto& deck = m_decks[i];
                        deck.parameter.clear();
                        deck.hasAppliedQ = false;
                        if (!deck.pendingRestore) {
                            deck.hasBaseline = false;
                            deck.everApplied = false;
                        }
                        deck.pendingRestore = false;
                        m_pConfig->setValue(restorationSetting(
                                i, deck.filterMode, "pending_restore_q"), false);
                    }
                }
                for (int i = 0; i < 2; ++i) {
                    reconcile(i);
                }
                if (!enabled) {
                    m_pEffectsManager->saveEffectsXml();
                }
                scheduleSave();
            });

    for (int i = 0; i < 2; ++i) {
        reconcile(i);
    }
}

Flx6FilterController::~Flx6FilterController() {
    if (m_saveTimer.isActive()) {
        m_saveTimer.stop();
        persist();
    }
}

double Flx6FilterController::savedResonance(int deckIndex, int filterMode) const {
    return clampNormalized(m_pConfig->getValue<double>(
            resonanceSetting(deckIndex, filterMode), filterMode == 1 ? 0.25 : 0.0));
}

void Flx6FilterController::loadRestorationState(int deckIndex) {
    auto& deck = m_decks[deckIndex];
    const int mode = deck.filterMode;
    const ConfigKey baselineKey = restorationSetting(deckIndex, mode, "baseline_q");
    const ConfigKey appliedKey = restorationSetting(deckIndex, mode, "last_applied_q");
    const ConfigKey appliedEffectKey =
            restorationSetting(deckIndex, mode, "last_applied_effect_id");
    const ConfigKey pendingKey = restorationSetting(deckIndex, mode, "pending_restore_q");
    const bool enabled = m_enabled->toBool();
    deck.parameter.clear();
    deck.hasBaseline = false;
    deck.hasAppliedQ = false;
    deck.everApplied = false;
    deck.lastAppliedEffectId.clear();
    deck.pendingRestore = !enabled && m_pConfig->getValue<bool>(pendingKey, false);
    const double minimum = mode == 1 ? 0.0 : 0.4;
    const double maximum = mode == 1 ? 4.0 : kWildQ;
    const double fallback = mode == 1 ? 1.0 : kFlatQ;
    if ((enabled || deck.pendingRestore) && m_pConfig->exists(baselineKey)) {
        deck.baselineQ = m_pConfig->getValue<double>(baselineKey, fallback);
        deck.hasBaseline = finiteBits(deck.baselineQ);
        if (deck.hasBaseline) {
            const double bounded = std::clamp(deck.baselineQ, minimum, maximum);
            if (bounded != deck.baselineQ) {
                deck.baselineQ = bounded;
                m_pConfig->setValue(baselineKey, bounded);
                scheduleSave();
            }
        }
    }
    if ((enabled || deck.pendingRestore) && m_pConfig->exists(appliedKey)) {
        deck.lastAppliedQ = m_pConfig->getValue<double>(appliedKey, fallback);
        deck.everApplied = finiteBits(deck.lastAppliedQ);
        if (deck.everApplied) {
            const double bounded = std::clamp(deck.lastAppliedQ, minimum, maximum);
            if (bounded != deck.lastAppliedQ) {
                deck.lastAppliedQ = bounded;
                m_pConfig->setValue(appliedKey, bounded);
                scheduleSave();
            }
            deck.lastAppliedEffectId =
                    m_pConfig->getValue<QString>(appliedEffectKey, QString());
        }
    }
    if (!deck.hasBaseline || !deck.everApplied) {
        if (deck.pendingRestore) {
            m_pConfig->setValue(pendingKey, false);
            scheduleSave();
        }
        deck.pendingRestore = false;
        if (!deck.everApplied) {
            deck.hasBaseline = false;
        }
    }
}

bool Flx6FilterController::selectFilter(int deckIndex, int filterMode) {
    auto& deck = m_decks[deckIndex];
    if (!deck.slot || !m_pEffectsManager) {
        return false;
    }
    const auto chain = m_pEffectsManager->getQuickEffectChain(
            QStringLiteral("[Channel%1]").arg(deckIndex + 1));
    if (!chain) {
        return false;
    }
    const double currentSweep = chain->getSuperParameter();
    const QString requestedId = filterId(filterMode);
    const auto manifest = m_pEffectsManager->getBackendManager()->getManifestFromUniqueId(
            requestedId);
    if (!manifest) {
        qWarning() << "FLX6 filter unavailable:" << requestedId;
        return false;
    }
    m_switchingEffect = true;
    deck.slot->loadEffectWithDefaults(manifest);
    deck.slot->setMetaParameter(finiteBits(currentSweep) ?
            std::clamp(currentSweep, 0.0, 1.0) : 0.5, true);
    m_switchingEffect = false;
    if (deck.slot->id() != requestedId) {
        return false;
    }
    deck.filterMode = filterMode;
    m_pConfig->setValue(setting(deckIndex == 0 ? "filter_mode1" : "filter_mode2"), filterMode);
    loadRestorationState(deckIndex);
    m_syncingControls = true;
    m_resonance[deckIndex]->forceSet(savedResonance(deckIndex, filterMode));
    m_filterMode[deckIndex]->forceSet(filterMode);
    m_syncingControls = false;
    reconcile(deckIndex);
    m_pEffectsManager->saveEffectsXml();
    scheduleSave();
    return true;
}

double Flx6FilterController::normalizedResonance(int deckIndex) const {
    return clampNormalized(m_resonance[deckIndex]->get());
}

void Flx6FilterController::restore(int deckIndex) {
    auto& deck = m_decks[deckIndex];
    const ConfigKey pendingKey =
            restorationSetting(deckIndex, deck.filterMode, "pending_restore_q");
    if (!deck.hasBaseline || !deck.everApplied) {
        const bool wasPending = deck.pendingRestore;
        deck.parameter.clear();
        deck.hasBaseline = false;
        deck.hasAppliedQ = false;
        deck.pendingRestore = false;
        if (wasPending) {
            m_pConfig->setValue(pendingKey, false);
            scheduleSave();
        }
        return;
    }
    if (!deck.slot || deck.lastAppliedEffectId.isEmpty() ||
            deck.slot->id() != deck.lastAppliedEffectId) {
        deck.parameter.clear();
        deck.hasAppliedQ = false;
        if (!deck.pendingRestore) {
            deck.pendingRestore = true;
            m_pConfig->setValue(pendingKey, true);
            scheduleSave();
        }
        return;
    }
    const auto loaded = deck.slot->getLoadedParameters().value(EffectParameterType::Knob);
    bool restored = false;
    for (int i = 0; i < loaded.size(); ++i) {
        const auto& parameter = loaded.at(i);
        if (!parameter || !parameter->manifest() ||
                parameter->manifest()->id() != resonanceParameterId(deck.filterMode)) {
            continue;
        }
        auto slot = deck.slot->getEffectParameterSlot(EffectParameterType::Knob, i);
        if (finiteBits(parameter->getValue()) &&
                std::abs(parameter->getValue() - deck.lastAppliedQ) < 0.000001 &&
                slot && slot->getManifest() &&
                slot->getManifest()->id() == resonanceParameterId(deck.filterMode)) {
            const ConfigKey key(deck.slot->getGroup(),
                    QStringLiteral("parameter%1").arg(i + 1));
            if (auto* control = ControlObject::getControl(key)) {
                ControlObject::set(key, deck.baselineQ);
                restored = true;
            }
        }
        break;
    }
    deck.parameter.clear();
    deck.hasBaseline = false;
    deck.hasAppliedQ = false;
    deck.everApplied = false;
    deck.pendingRestore = false;
    m_pConfig->setValue(pendingKey, false);
    scheduleSave();
    if (restored) {
        QTimer::singleShot(0, this, [this]() {
            m_pEffectsManager->saveEffectsXml();
        });
    }
}

void Flx6FilterController::reconcile(int deckIndex) {
    auto& deck = m_decks[deckIndex];
    if (deck.slot && (deck.slot->id() == kFilterId || deck.slot->id() == kMoogId)) {
        const int actualMode = deck.slot->id() == kMoogId ? 1 : 0;
        if (actualMode != deck.filterMode) {
            deck.filterMode = actualMode;
            m_pConfig->setValue(setting(deckIndex == 0 ? "filter_mode1" : "filter_mode2"),
                    actualMode);
            loadRestorationState(deckIndex);
            m_syncingControls = true;
            m_filterMode[deckIndex]->forceSet(actualMode);
            m_resonance[deckIndex]->forceSet(savedResonance(deckIndex, actualMode));
            m_syncingControls = false;
            scheduleSave();
        } else if (m_filterMode[deckIndex]->get() != actualMode) {
            m_syncingControls = true;
            m_filterMode[deckIndex]->forceSet(actualMode);
            m_syncingControls = false;
        }
    } else if (deck.slot && m_filterMode[deckIndex]->get() != 2.0) {
        m_syncingControls = true;
        m_filterMode[deckIndex]->forceSet(2.0);
        m_syncingControls = false;
    }
    if (!deck.slot || !m_enabled->toBool() ||
            deck.slot->id() != filterId(deck.filterMode)) {
        if (!m_enabled->toBool()) {
            restore(deckIndex);
        } else {
            deck.parameter.clear();
            deck.hasAppliedQ = false;
        }
        m_active[deckIndex]->forceSet(0.0);
        return;
    }

    const auto loaded = deck.slot->getLoadedParameters().value(EffectParameterType::Knob);
    for (int i = 0; i < loaded.size(); ++i) {
        const auto& parameter = loaded.at(i);
        if (!parameter || !parameter->manifest() ||
                parameter->manifest()->id() != resonanceParameterId(deck.filterMode) ||
                parameter->linkType() != EffectManifestParameter::LinkType::None) {
            continue;
        }
        auto slot = deck.slot->getEffectParameterSlot(EffectParameterType::Knob, i);
        if (!slot || !slot->getManifest() ||
                slot->getManifest()->id() != resonanceParameterId(deck.filterMode)) {
            continue;
        }
        const ConfigKey key(deck.slot->getGroup(),
                QStringLiteral("parameter%1").arg(i + 1));
        auto* control = ControlObject::getControl(key);
        if (!control) {
            continue;
        }

        if (deck.parameter != parameter) {
            deck.parameter = parameter;
            deck.hasAppliedQ = false;
            const ConfigKey baselineKey =
                    restorationSetting(deckIndex, deck.filterMode, "baseline_q");
            if (!deck.hasBaseline) {
                deck.baselineQ = parameter->getValue();
                if (!finiteBits(deck.baselineQ)) {
                    deck.baselineQ = deck.filterMode == 1 ? 1.0 : kFlatQ;
                }
                deck.baselineQ = std::clamp(deck.baselineQ,
                        parameter->manifest()->getMinimum(),
                        parameter->manifest()->getMaximum());
                deck.hasBaseline = true;
                m_pConfig->setValue(baselineKey, deck.baselineQ);
                scheduleSave();
            }
        }

        const double q = deck.filterMode == 1 ?
                4.0 * normalizedResonance(deckIndex) :
                qFromNormalized(normalizedResonance(deckIndex));
        if (!finiteBits(parameter->getValue()) ||
                std::abs(parameter->getValue() - q) > 0.000001) {
            ControlObject::set(key, q);
        }
        deck.lastAppliedQ = q;
        deck.hasAppliedQ = true;
        deck.everApplied = true;
        if (deck.lastAppliedEffectId != filterId(deck.filterMode)) {
            deck.lastAppliedEffectId = filterId(deck.filterMode);
            m_pConfig->setValue(restorationSetting(
                    deckIndex, deck.filterMode, "last_applied_effect_id"),
                    deck.lastAppliedEffectId);
            scheduleSave();
        }
        m_pConfig->setValue(
                restorationSetting(deckIndex, deck.filterMode, "last_applied_q"), q);
        m_active[deckIndex]->forceSet(1.0);
        return;
    }
    deck.parameter.clear();
    deck.hasAppliedQ = false;
    m_active[deckIndex]->forceSet(0.0);
}

void Flx6FilterController::scheduleSave() {
    m_saveTimer.start();
}

void Flx6FilterController::persist() {
    const bool saved = m_pConfig->save();
    m_saveError->forceSet(saved ? 0.0 : 1.0);
    if (!saved) {
        qWarning() << "Unable to save FLX6 filter settings";
    }
}

}

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <memory>
#include <vector>

#include "control/controlproxy.h"
#include "effects/backends/builtin/autopaneffect.h"
#include "effects/backends/builtin/echoeffect.h"
#include "effects/backends/builtin/flangereffect.h"
#include "effects/backends/builtin/glitcheffect.h"
#include "effects/backends/builtin/parametriceqeffect.h"
#include "effects/backends/builtin/phasereffect.h"
#include "effects/backends/builtin/reverbeffect.h"
#include "effects/backends/builtin/tremoloeffect.h"
#include "effects/effectchain.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "effects/presets/effectchainpreset.h"
#include "effects/presets/effectchainpresetmanager.h"
#include "engine/channelhandle.h"
#include "qml/qmlchainpresetmodel.h"
#include "qml/qmleffectslotparametersmodel.h"
#include "qml/qmleffectslotproxy.h"
#include "qml/qmleffectsmanagerproxy.h"
#include "qml/qmleffectunitproxy.h"
#include "test/mixxxtest.h"

namespace {

class QmlEffectsProxyTest : public MixxxTest {
  protected:
    void SetUp() override {
        auto pChannelHandleFactory = std::make_shared<ChannelHandleFactory>();
        m_pEffectsManager =
                std::make_shared<EffectsManager>(config(), pChannelHandleFactory);
        const QString mainOutputGroup = QStringLiteral("[MasterOutput]");
        m_pEffectsManager->registerInputChannel(ChannelHandleAndGroup(
                pChannelHandleFactory->getOrCreateHandle(mainOutputGroup), mainOutputGroup));
        m_pEffectsManager->setup();
        m_pProxy = std::make_unique<mixxx::qml::QmlEffectsManagerProxy>(
                m_pEffectsManager);
    }

    void TearDown() override {
        m_pProxy.reset();
        for (int unitIndex = 0; unitIndex < kNumStandardEffectUnits; ++unitIndex) {
            const auto pChain = m_pEffectsManager->getStandardEffectChain(unitIndex);
            auto pEmptyPreset = EffectChainPresetPointer::create();
            pEmptyPreset->setName(QString());
            pChain->loadChainPreset(pEmptyPreset);
            EXPECT_TRUE(pChain->isEmpty());
            EXPECT_TRUE(pChain->presetName().isEmpty());
        }
        m_pEffectsManager.reset();
    }

    std::shared_ptr<EffectsManager> m_pEffectsManager;
    std::unique_ptr<mixxx::qml::QmlEffectsManagerProxy> m_pProxy;
};

TEST_F(QmlEffectsProxyTest, UnitLookup) {
    auto* pUnit1 = m_pProxy->getEffectUnit(1);
    ASSERT_NE(nullptr, pUnit1);
    EXPECT_EQ(pUnit1, m_pProxy->getEffectUnit(1));
    EXPECT_EQ(1, pUnit1->getUnitNumber());
    EXPECT_EQ(QStringLiteral("[EffectRack1_EffectUnit1]"), pUnit1->getGroup());
    EXPECT_EQ(nullptr, m_pProxy->getEffectUnit(0));
    EXPECT_EQ(nullptr, m_pProxy->getEffectUnit(5));
}

TEST_F(QmlEffectsProxyTest, PresetModelsResetSeparately) {
    auto* pStandardModel = m_pProxy->property("standardChainPresetModel")
                                   .value<mixxx::qml::QmlChainPresetModel*>();
    auto* pQuickModel = m_pProxy->property("quickChainPresetModel")
                                .value<mixxx::qml::QmlChainPresetModel*>();
    ASSERT_NE(nullptr, pStandardModel);
    ASSERT_NE(nullptr, pQuickModel);
    EXPECT_GT(pStandardModel->rowCount({}), 0);
    EXPECT_GT(pQuickModel->rowCount({}), 0);
    EXPECT_TRUE(pStandardModel->roleNames().values().contains("name"));
    EXPECT_TRUE(pStandardModel->roleNames().values().contains("readOnly"));

    QSignalSpy standardResetSpy(pStandardModel, &QAbstractItemModel::modelReset);
    QSignalSpy quickResetSpy(pQuickModel, &QAbstractItemModel::modelReset);
    QMetaObject::invokeMethod(m_pEffectsManager->getChainPresetManager().get(),
            "effectChainPresetListUpdated");
    EXPECT_EQ(1, standardResetSpy.count());
    EXPECT_EQ(0, quickResetSpy.count());
    QMetaObject::invokeMethod(m_pEffectsManager->getChainPresetManager().get(),
            "quickEffectChainPresetListUpdated");
    EXPECT_EQ(1, standardResetSpy.count());
    EXPECT_EQ(1, quickResetSpy.count());
}

TEST_F(QmlEffectsProxyTest, UnitPresetStateAndLoading) {
    auto* pUnit1 = m_pProxy->getEffectUnit(1);
    ASSERT_NE(nullptr, pUnit1);
    QSignalSpy presetSpy(pUnit1, &mixxx::qml::QmlEffectUnitProxy::presetChanged);
    pUnit1->loadPreset(-1);
    EXPECT_EQ(0, presetSpy.count());
    pUnit1->loadPreset(0);
    EXPECT_EQ(0, pUnit1->getPresetIndex());
    EXPECT_FALSE(pUnit1->getPresetName().isEmpty());
    EXPECT_EQ(!pUnit1->isPresetReadOnly(), pUnit1->canUpdatePreset());
    EXPECT_EQ(pUnit1->canUpdatePreset() && !pUnit1->getPresetName().isEmpty(),
            pUnit1->canRenamePreset());
    EXPECT_GE(presetSpy.count(), 1);
}

TEST_F(QmlEffectsProxyTest, SlotEffectAndParameterVisibility) {
    constexpr int kExpectedParameterCount = 6;
    constexpr int kParameter1Index = 0;
    constexpr int kParameter2Index = 1;
    constexpr int kParameter3Index = 2;
    constexpr int kButtonParameter1Index = 4;

    std::unique_ptr<mixxx::qml::QmlEffectSlotProxy> pSlot(
            m_pProxy->getEffectSlot(1, 1));
    ASSERT_NE(nullptr, pSlot);
    QSignalSpy effectSpy(
            pSlot.get(), &mixxx::qml::QmlEffectSlotProxy::effectIdChanged);
    auto* pModel = pSlot->getParametersModel();
    ASSERT_NE(nullptr, pModel);
    QSignalSpy modelResetSpy(pModel, &QAbstractItemModel::modelReset);
    QSignalSpy dataChangedSpy(pModel, &QAbstractItemModel::dataChanged);

    pSlot->setEffectId(EchoEffect::getId());
    ASSERT_TRUE(pSlot->isLoaded());
    EXPECT_GE(effectSpy.count(), 1);
    EXPECT_GE(modelResetSpy.count(), 1);

    ASSERT_EQ(kExpectedParameterCount, pModel->rowCount({}));
    EXPECT_TRUE(pModel->roleNames().values().contains("unitString"));
    EXPECT_EQ(QStringLiteral("parameter1"),
            pModel->get(kParameter1Index).toMap().value("controlKey").toString());
    EXPECT_EQ(QStringLiteral("parameter2"),
            pModel->get(kParameter2Index).toMap().value("controlKey").toString());
    EXPECT_EQ(QStringLiteral("button_parameter1"),
            pModel->get(kButtonParameter1Index)
                    .toMap()
                    .value("controlKey")
                    .toString());

    pSlot->setParameterVisible(QStringLiteral("feedback_amount"), false);
    EXPECT_GE(dataChangedSpy.count(), 1);
    EXPECT_FALSE(pModel->get(kParameter2Index).toMap().value("loaded").toBool());
    EXPECT_TRUE(pModel->get(kParameter2Index)
                    .toMap()
                    .value("controlKey")
                    .toString()
                    .isEmpty());
    EXPECT_EQ(QStringLiteral("parameter2"),
            pModel->get(kParameter3Index).toMap().value("controlKey").toString());

    pSlot->setParameterVisible(QStringLiteral("feedback_amount"), true);
    EXPECT_TRUE(pModel->get(kParameter2Index).toMap().value("loaded").toBool());
    pSlot->saveDefaultSnapshot();

    pSlot->setEffectId(ParametricEQEffect::getId());
    EXPECT_EQ(QStringLiteral("dB"),
            pModel->get(kParameter1Index).toMap().value("unitString").toString());
}

TEST_F(QmlEffectsProxyTest, BeatTimeStepsAndFollowsParameters) {
    std::unique_ptr<mixxx::qml::QmlEffectSlotProxy> pSlot(m_pProxy->getEffectSlot(1, 1));
    ASSERT_NE(nullptr, pSlot);
    const QString group = pSlot->getGroup();
    ControlProxy step(group, QStringLiteral("beat_time_step"));
    ControlProxy beats(group, QStringLiteral("beat_time"));
    ControlProxy supported(group, QStringLiteral("beat_time_supported"));
    ControlProxy timeParameter(group, QStringLiteral("parameter1"));
    ControlProxy tripletParameter(group, QStringLiteral("button_parameter2"));

    EXPECT_EQ(0.0, supported.get());
    pSlot->setEffectId(EchoEffect::getId());
    ASSERT_EQ(1.0, supported.get());
    EXPECT_DOUBLE_EQ(0.5, beats.get());

    step.set(1);
    EXPECT_DOUBLE_EQ(1.0, timeParameter.get());
    EXPECT_DOUBLE_EQ(1.0, beats.get());
    step.set(1);
    EXPECT_DOUBLE_EQ(2.0, beats.get());
    step.set(1);
    EXPECT_DOUBLE_EQ(2.0, beats.get());
    step.set(0);
    EXPECT_DOUBLE_EQ(2.0, beats.get());

    step.set(-1);
    step.set(-1);
    step.set(-1);
    EXPECT_DOUBLE_EQ(0.25, beats.get());
    step.set(-1);
    EXPECT_DOUBLE_EQ(0.125, beats.get());

    tripletParameter.set(1);
    EXPECT_NEAR(1.0 / 24, beats.get(), 1e-9);
    step.set(1);
    EXPECT_NEAR(1.0 / 12, beats.get(), 1e-9);
    timeParameter.set(1);
    EXPECT_NEAR(1.0 / 3, beats.get(), 1e-9);
    pSlot->setParameterVisible(QStringLiteral("delay_time"), false);
    step.set(1);
    EXPECT_NEAR(2.0 / 3, beats.get(), 1e-9);
    pSlot->setParameterVisible(QStringLiteral("delay_time"), true);
    EXPECT_NEAR(2.0 / 3, beats.get(), 1e-9);

    pSlot->setEffectId(ParametricEQEffect::getId());
    EXPECT_EQ(0.0, supported.get());
    EXPECT_EQ(0.0, beats.get());
    step.set(1);
    EXPECT_EQ(ParametricEQEffect::getId(), pSlot->getEffectId());
    EXPECT_EQ(0.0, beats.get());
}

TEST_F(QmlEffectsProxyTest, BeatTimeRespectsFlangerRoundingAndTremoloRate) {
    std::unique_ptr<mixxx::qml::QmlEffectSlotProxy> pSlot(m_pProxy->getEffectSlot(1, 2));
    ASSERT_NE(nullptr, pSlot);
    const QString group = pSlot->getGroup();
    ControlProxy step(group, QStringLiteral("beat_time_step"));
    ControlProxy beats(group, QStringLiteral("beat_time"));
    ControlProxy supported(group, QStringLiteral("beat_time_supported"));
    ControlProxy firstParameter(group, QStringLiteral("parameter1"));
    ControlProxy secondParameter(group, QStringLiteral("parameter2"));

    pSlot->setEffectId(FlangerEffect::getId());
    ASSERT_EQ(1.0, supported.get());
    firstParameter.set(0.25);
    EXPECT_DOUBLE_EQ(0.5, beats.get());
    step.set(-1);
    EXPECT_DOUBLE_EQ(0.5, beats.get());
    step.set(1);
    EXPECT_DOUBLE_EQ(1.0, beats.get());
    EXPECT_DOUBLE_EQ(1.0, firstParameter.get());

    pSlot->setEffectId(TremoloEffect::getId());
    EXPECT_DOUBLE_EQ(1.0, beats.get());
    step.set(1);
    EXPECT_DOUBLE_EQ(2.0, beats.get());
    EXPECT_DOUBLE_EQ(0.5, secondParameter.get());
    step.set(-1);
    EXPECT_DOUBLE_EQ(1.0, beats.get());
    EXPECT_DOUBLE_EQ(1.0, secondParameter.get());
}

TEST_F(QmlEffectsProxyTest, FxParameterStepUsesBeatsOrReverbDecay) {
    std::unique_ptr<mixxx::qml::QmlEffectSlotProxy> pSlot(m_pProxy->getEffectSlot(1, 1));
    ASSERT_NE(nullptr, pSlot);
    const QString group = pSlot->getGroup();
    ControlProxy genericStep(group, QStringLiteral("fx_parameter_step"));
    ControlProxy parameterValue(group, QStringLiteral("fx_parameter_value"));
    ControlProxy parameterType(group, QStringLiteral("fx_parameter_type"));
    ControlProxy beatTime(group, QStringLiteral("beat_time"));
    ControlProxy beatSupported(group, QStringLiteral("beat_time_supported"));
    ControlProxy firstParameter(group, QStringLiteral("parameter1"));

    EXPECT_EQ(0.0, parameterType.get());
    genericStep.set(1);
    EXPECT_EQ(0.0, parameterValue.get());

    pSlot->setEffectId(EchoEffect::getId());
    EXPECT_EQ(1.0, parameterType.get());
    EXPECT_DOUBLE_EQ(beatTime.get(), parameterValue.get());
    genericStep.set(1);
    EXPECT_DOUBLE_EQ(1.0, parameterValue.get());
    EXPECT_DOUBLE_EQ(1.0, beatTime.get());

    pSlot->setEffectId(ReverbEffect::getId());
    EXPECT_EQ(2.0, parameterType.get());
    EXPECT_EQ(0.0, beatSupported.get());
    EXPECT_EQ(0.0, beatTime.get());
    EXPECT_NEAR(50.0, parameterValue.get(), 1e-9);
    genericStep.set(1);
    EXPECT_NEAR(0.55, firstParameter.get(), 1e-9);
    EXPECT_NEAR(55.0, parameterValue.get(), 1e-9);
    genericStep.set(-1);
    EXPECT_NEAR(50.0, parameterValue.get(), 1e-9);

    firstParameter.set(0.53);
    EXPECT_NEAR(53.0, parameterValue.get(), 1e-9);
    genericStep.set(1);
    EXPECT_NEAR(55.0, parameterValue.get(), 1e-9);
    genericStep.set(-1);
    EXPECT_NEAR(50.0, parameterValue.get(), 1e-9);

    pSlot->setParameterVisible(QStringLiteral("decay"), false);
    genericStep.set(1);
    EXPECT_NEAR(55.0, parameterValue.get(), 1e-9);
    pSlot->setParameterVisible(QStringLiteral("decay"), true);
    EXPECT_NEAR(0.55, firstParameter.get(), 1e-9);

    firstParameter.set(1.0);
    genericStep.set(1);
    EXPECT_NEAR(100.0, parameterValue.get(), 1e-9);
    firstParameter.set(0.0);
    genericStep.set(-1);
    EXPECT_NEAR(0.0, parameterValue.get(), 1e-9);
    genericStep.set(0);
    EXPECT_NEAR(0.0, parameterValue.get(), 1e-9);

    pSlot->setEffectId(ParametricEQEffect::getId());
    EXPECT_EQ(0.0, parameterType.get());
    EXPECT_EQ(0.0, parameterValue.get());
    genericStep.set(1);
    EXPECT_EQ(0.0, parameterValue.get());
}

TEST_F(QmlEffectsProxyTest, BeatStepsStopAtBothEndsForEachTimedEffect) {
    std::unique_ptr<mixxx::qml::QmlEffectSlotProxy> pSlot(m_pProxy->getEffectSlot(1, 1));
    ASSERT_NE(nullptr, pSlot);
    const QString group = pSlot->getGroup();
    ControlProxy step(group, QStringLiteral("fx_parameter_step"));
    ControlProxy beats(group, QStringLiteral("fx_parameter_value"));
    ControlProxy type(group, QStringLiteral("fx_parameter_type"));
    ControlProxy firstParameter(group, QStringLiteral("parameter1"));
    ControlProxy secondParameter(group, QStringLiteral("parameter2"));

    struct TimedEffect {
        QString id;
        std::vector<double> values;
        double initialRaw;
        bool timingIsSecondParameter;
    };
    const std::vector<TimedEffect> effects = {
            {EchoEffect::getId(), {0.125, 0.25, 0.5, 1, 2}, 0, false},
            {GlitchEffect::getId(), {0.125, 0.25, 0.5, 1, 2}, 0.125, false},
            {PhaserEffect::getId(), {0.5, 1, 2, 4}, 0.5, false},
            {FlangerEffect::getId(), {0.5, 1, 2, 4, 8, 16, 32}, 0.5, false},
            {AutoPanEffect::getId(), {0.5, 1, 2, 4}, 0.5, false},
            {TremoloEffect::getId(), {0.125, 0.25, 0.5, 1, 2, 4}, 8, true},
    };
    for (const auto& effect : effects) {
        pSlot->setEffectId(effect.id);
        ASSERT_EQ(1.0, type.get()) << effect.id.toStdString();
        auto& timingParameter =
                effect.timingIsSecondParameter ? secondParameter : firstParameter;
        timingParameter.set(effect.initialRaw);
        ASSERT_DOUBLE_EQ(effect.values.front(), beats.get())
                << effect.id.toStdString();

        step.set(-1);
        EXPECT_DOUBLE_EQ(effect.values.front(), beats.get())
                << effect.id.toStdString();
        for (std::size_t i = 1; i < effect.values.size(); ++i) {
            step.set(1);
            EXPECT_DOUBLE_EQ(effect.values[i], beats.get())
                    << effect.id.toStdString() << " at step " << i;
        }
        for (int repeat = 0; repeat < 3; ++repeat) {
            step.set(1);
            EXPECT_DOUBLE_EQ(effect.values.back(), beats.get())
                    << effect.id.toStdString() << " past upper limit";
        }
        for (std::size_t i = effect.values.size() - 1; i > 0; --i) {
            step.set(-1);
            EXPECT_DOUBLE_EQ(effect.values[i - 1], beats.get())
                    << effect.id.toStdString() << " at step " << i;
        }
        for (int repeat = 0; repeat < 3; ++repeat) {
            step.set(-1);
            EXPECT_DOUBLE_EQ(effect.values.front(), beats.get())
                    << effect.id.toStdString() << " past lower limit";
        }
    }
}

TEST_F(QmlEffectsProxyTest, StartupTimingResetKeepsEffectsAndOtherParameters) {
    std::unique_ptr<mixxx::qml::QmlEffectSlotProxy> pEcho(
            m_pProxy->getEffectSlot(1, 1));
    std::unique_ptr<mixxx::qml::QmlEffectSlotProxy> pReverb(
            m_pProxy->getEffectSlot(1, 2));
    ASSERT_NE(nullptr, pEcho);
    ASSERT_NE(nullptr, pReverb);
    pEcho->setEffectId(EchoEffect::getId());
    pReverb->setEffectId(ReverbEffect::getId());

    ControlProxy echoTime(pEcho->getGroup(), QStringLiteral("parameter1"));
    ControlProxy echoFeedback(pEcho->getGroup(), QStringLiteral("parameter2"));
    ControlProxy reverbDecay(pReverb->getGroup(), QStringLiteral("parameter1"));
    ControlProxy reverbBandwidth(pReverb->getGroup(), QStringLiteral("parameter2"));
    echoTime.set(1.0);
    echoFeedback.set(0.3);
    reverbDecay.set(0.8);
    reverbBandwidth.set(0.4);

    const auto pChain = m_pEffectsManager->getStandardEffectChain(0);
    ASSERT_NE(nullptr, pChain);
    auto pSavedPreset = EffectChainPresetPointer::create(pChain.data());
    pChain->loadChainPreset(pSavedPreset);
    EXPECT_NEAR(1.0, echoTime.get(), 1e-9);
    EXPECT_NEAR(0.8, reverbDecay.get(), 1e-9);

    for (const auto& pSlot : pChain->getEffectSlots()) {
        pSlot->resetTimingToManifestDefault();
    }
    EXPECT_EQ(EchoEffect::getId(), pEcho->getEffectId());
    EXPECT_EQ(ReverbEffect::getId(), pReverb->getEffectId());
    EXPECT_NEAR(0.5, echoTime.get(), 1e-9);
    EXPECT_NEAR(0.3, echoFeedback.get(), 1e-9);
    EXPECT_NEAR(0.5, reverbDecay.get(), 1e-9);
    EXPECT_NEAR(0.4, reverbBandwidth.get(), 1e-9);
}

TEST_F(QmlEffectsProxyTest, TimingResetUsesEachBuiltInDefault) {
    std::unique_ptr<mixxx::qml::QmlEffectSlotProxy> pSlot(
            m_pProxy->getEffectSlot(1, 1));
    ASSERT_NE(nullptr, pSlot);
    ControlProxy firstParameter(pSlot->getGroup(), QStringLiteral("parameter1"));
    ControlProxy secondParameter(pSlot->getGroup(), QStringLiteral("parameter2"));

    struct TimedEffect {
        QString id;
        double changed;
        double defaultValue;
        bool timingIsSecondParameter;
    };
    const std::vector<TimedEffect> effects = {
            {EchoEffect::getId(), 1.0, 0.5, false},
            {GlitchEffect::getId(), 1.0, 0.5, false},
            {PhaserEffect::getId(), 2.0, 1.0, false},
            {FlangerEffect::getId(), 16.0, 8.0, false},
            {AutoPanEffect::getId(), 4.0, 2.0, false},
            {TremoloEffect::getId(), 2.0, 1.0, true},
    };
    const auto pChain = m_pEffectsManager->getStandardEffectChain(0);
    ASSERT_NE(nullptr, pChain);
    const auto pCoreSlot = pChain->getEffectSlot(0);
    ASSERT_NE(nullptr, pCoreSlot);
    for (const auto& effect : effects) {
        pSlot->setEffectId(effect.id);
        auto& timingParameter =
                effect.timingIsSecondParameter ? secondParameter : firstParameter;
        timingParameter.set(effect.changed);
        ASSERT_NEAR(effect.changed, timingParameter.get(), 1e-9)
                << effect.id.toStdString();
        pCoreSlot->resetTimingToManifestDefault();
        EXPECT_NEAR(effect.defaultValue, timingParameter.get(), 1e-9)
                << effect.id.toStdString();
        EXPECT_EQ(effect.id, pSlot->getEffectId());
    }
}

} // namespace

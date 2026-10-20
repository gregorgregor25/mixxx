#include <gtest/gtest.h>

#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>
#include <vector>

#include "control/controlobject.h"
#include "coreservices.h"
#include "library/dao/playlistdao.h"
#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "qml/qmllibraryproxy.h"
#include "qml/qmltrackproxy.h"
#include "test/mixxxtest.h"
#include "track/beats.h"
#include "track/track.h"
#include "util/cmdlineargs.h"
#include "util/versionstore.h"

namespace {

class RejectFileDialogs final : public QObject {
  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Show) {
            if (auto* dialog = qobject_cast<QFileDialog*>(watched)) {
                QTimer::singleShot(0, dialog, &QDialog::reject);
            }
        }
        return false;
    }
};

void writeTestProfile(const QString& settingsPath) {
    auto settings = UserSettingsPointer(new UserSettings(settingsPath + "mixxx.cfg"));
    settings->setValue(ConfigKey("[Config]", "Version"), VersionStore::FUTURE_UNSTABLE);
    settings->save();

    QFile soundConfig(settingsPath + "soundconfig.xml");
    ASSERT_TRUE(soundConfig.open(QIODevice::WriteOnly | QIODevice::Truncate));
    ASSERT_GT(soundConfig.write("<SoundManagerConfig api=\"None\"/>\n"), 0);
}

class QmlLibraryProxyTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_previousArgs = CmdlineArgs::Instance();
        ASSERT_TRUE(m_profile.isValid());

        const QString settingsPath = m_profile.path() + QLatin1Char('/');
        writeTestProfile(settingsPath);

        CmdlineArgs::Instance().setSettingsPath(settingsPath);
        CmdlineArgs args;
        args.setSettingsPath(settingsPath);

        m_rejectFileDialogs = std::make_unique<RejectFileDialogs>();
        application()->installEventFilter(m_rejectFileDialogs.get());
        m_coreServices = std::make_shared<mixxx::CoreServices>(args, application());
        m_coreServices->initialize(application());
        application()->removeEventFilter(m_rejectFileDialogs.get());

        m_proxy = std::make_unique<mixxx::qml::QmlLibraryProxy>();
    }

    void TearDown() override {
        m_proxy.reset();
        m_coreServices.reset();
        if (m_rejectFileDialogs) {
            application()->removeEventFilter(m_rejectFileDialogs.get());
            m_rejectFileDialogs.reset();
        }
        CmdlineArgs::Instance() = m_previousArgs;
    }

    void setDeckControl(const QString& key, double value) {
        const ConfigKey configKey(QStringLiteral("[Channel1]"), key);
        if (!ControlObject::exists(configKey)) {
            m_testControls.emplace_back(std::make_unique<ControlObject>(configKey));
        }
        ControlObject::set(configKey, value);
    }

    std::unique_ptr<mixxx::qml::QmlLibraryProxy> m_proxy;

  private:
    QTemporaryDir m_profile;
    CmdlineArgs m_previousArgs;
    std::unique_ptr<RejectFileDialogs> m_rejectFileDialogs;
    std::shared_ptr<mixxx::CoreServices> m_coreServices;
    std::vector<std::unique_ptr<ControlObject>> m_testControls;
};

TEST_F(QmlLibraryProxyTest, DeckHotcueJumpDirectionMatchesLegacyRules) {
    constexpr auto kSampleRate = mixxx::audio::SampleRate(8000);
    constexpr auto kCuePosition = mixxx::audio::FramePos(10000);
    const QString group = QStringLiteral("[Channel1]");

    auto pTrack = Track::newTemporary();
    pTrack->setAudioProperties(
            mixxx::audio::ChannelCount(2),
            kSampleRate,
            mixxx::audio::Bitrate(),
            mixxx::Duration::fromSeconds(60));
    ASSERT_TRUE(pTrack->trySetBeats(mixxx::Beats::fromConstTempo(
            kSampleRate,
            mixxx::audio::kStartFramePos,
            mixxx::Bpm(48.0))));

    ASSERT_TRUE(pTrack->createAndAddCue(
            mixxx::CueType::HotCue,
            0,
            kCuePosition,
            mixxx::audio::kInvalidFramePos));
    ASSERT_TRUE(pTrack->createAndAddCue(
            mixxx::CueType::Loop,
            1,
            mixxx::audio::FramePos(20000),
            mixxx::audio::FramePos(22000)));
    ASSERT_TRUE(pTrack->createAndAddCue(
            mixxx::CueType::Jump,
            2,
            mixxx::audio::FramePos(22000),
            mixxx::audio::FramePos(20000)));
    ASSERT_TRUE(pTrack->createAndAddCue(
            mixxx::CueType::Jump,
            3,
            mixxx::audio::FramePos(20000),
            mixxx::audio::FramePos(22000)));

    mixxx::qml::QmlTrackProxy trackProxy(pTrack);
    setDeckControl(QStringLiteral("track_samples"), 40000.0);
    setDeckControl(QStringLiteral("quantize"), 0.0);

    setDeckControl(QStringLiteral("playposition"), 0.25);
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Forward,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 1));

    setDeckControl(QStringLiteral("playposition"), 0.75);
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Backward,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 1));

    setDeckControl(QStringLiteral("playposition"), 0.5025);
    setDeckControl(QStringLiteral("quantize"), 1.0);
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Impossible,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 1));

    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Forward,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 2));
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Forward,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 3));
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Backward,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 4));
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Impossible,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 0));
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Impossible,
            m_proxy->deckHotcueJumpDirection(nullptr, group, 1));
}

TEST_F(QmlLibraryProxyTest, FolderImportReusesPlaylistAndTrackMembership) {
    QTemporaryDir music;
    ASSERT_TRUE(music.isValid());
    QDir root(music.path());
    ASSERT_TRUE(root.mkpath(QStringLiteral("profile")));
    ASSERT_TRUE(root.mkpath(QStringLiteral("Music/Crate Swipe 1/Nested")));
    const QString folder = root.filePath(QStringLiteral("Music/Crate Swipe 1"));
    CmdlineArgs::Instance().setSettingsPath(root.filePath(QStringLiteral("profile")));
    const QVariantList folders = m_proxy->copiedMusicFolders();
    ASSERT_EQ(folders.size(), 1);
    EXPECT_EQ(folders.first().toMap().value(QStringLiteral("name")).toString(),
            QStringLiteral("Crate Swipe 1"));
    EXPECT_EQ(folders.first().toMap().value(QStringLiteral("directory")).toUrl(),
            QUrl::fromLocalFile(folder));
    ASSERT_TRUE(QFile::copy(getTestDir().filePath(QStringLiteral("id3-test-data/artist.mp3")),
            QDir(folder).filePath(QStringLiteral("first.mp3"))));
    ASSERT_TRUE(QFile::copy(getTestDir().filePath(QStringLiteral("id3-test-data/all.mp3")),
            QDir(folder).filePath(QStringLiteral("Nested/second.mp3"))));

    QSignalSpy importFinished(
            m_proxy.get(), &mixxx::qml::QmlLibraryProxy::folderPlaylistImportFinished);
    QSignalSpy analysisProgress(
            m_proxy.get(), &mixxx::qml::QmlLibraryProxy::analysisProgressChanged);
    ASSERT_TRUE(importFinished.isValid());
    ASSERT_TRUE(analysisProgress.isValid());
    m_proxy->importFolderPlaylist(
            QUrl::fromLocalFile(folder), QStringLiteral("Crate Swipe 1"));
    ASSERT_TRUE(importFinished.wait(20000));
    ASSERT_EQ(importFinished.size(), 1);
    EXPECT_EQ(importFinished.at(0).at(1).toInt(), 2);
    EXPECT_EQ(importFinished.at(0).at(2).toInt(), 2);
    EXPECT_EQ(importFinished.at(0).at(3).toInt(), 2);
    EXPECT_TRUE(importFinished.at(0).at(4).toString().isEmpty());
    EXPECT_GT(analysisProgress.size(), 0);
    EXPECT_GE(m_proxy->property("analysisTotalTracks").toInt(), 2);
    EXPECT_LE(m_proxy->property("analysisCompletedTracks").toInt(),
            m_proxy->property("analysisTotalTracks").toInt());

    auto& playlistDao = mixxx::qml::QmlLibraryProxy::get()
                                ->trackCollectionManager()
                                ->internalCollection()
                                ->getPlaylistDAO();
    const int playlistId = playlistDao.getPlaylistIdFromName(QStringLiteral("Crate Swipe 1"));
    ASSERT_NE(playlistId, kInvalidPlaylistId);
    EXPECT_EQ(playlistDao.getTrackIds(playlistId).size(), 2);

    importFinished.clear();
    m_proxy->importFolderPlaylist(
            QUrl::fromLocalFile(folder), QStringLiteral("Crate Swipe 1"));
    ASSERT_TRUE(importFinished.wait(20000));
    EXPECT_EQ(importFinished.at(0).at(1).toInt(), 2);
    EXPECT_EQ(importFinished.at(0).at(2).toInt(), 0);
    EXPECT_EQ(importFinished.at(0).at(3).toInt(), 2);
    EXPECT_TRUE(importFinished.at(0).at(4).toString().isEmpty());
    EXPECT_EQ(playlistDao.getTrackIds(playlistId).size(), 2);
    EXPECT_EQ(playlistDao.getPlaylists(PlaylistDAO::PLHT_NOT_HIDDEN).size(), 1);
}

} // namespace

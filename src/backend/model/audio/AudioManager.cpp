
#include "AudioManager.h"

#include <QByteArray>
#include <QDebug>
#include <QtGlobal>
#include <QFileInfo>

AudioManager::AudioManager(QObject* parent)
    : QObject(parent)
    , m_engineReady(false)
    , m_musicReady(false)
    , m_musicVolume(1.0f)
{
    // Start the miniaudio engine and audio device.
    const ma_result result = ma_engine_init(nullptr, &m_engine);

    if (result != MA_SUCCESS) {
        qWarning() << "Failed to initialise miniaudio:" << result;
        return;
    }

    m_engineReady = true;
}

AudioManager::~AudioManager()
{
    if (m_musicReady) {
        ma_sound_uninit(&m_music);
        m_musicReady = false;
    }

    if (m_engineReady) {
        ma_engine_uninit(&m_engine);
        m_engineReady = false;
    }
}

bool AudioManager::playMusic(const QString& filePath)
{
    if (!m_engineReady || filePath.isEmpty())
        return false;

    // Release the previous music track, if there is one.
    if (m_musicReady) {
        ma_sound_uninit(&m_music);
        m_musicReady = false;
    }

    const QByteArray path = filePath.toUtf8();

    // Initialise the new track using streaming playback.
    const ma_result result = ma_sound_init_from_file(
        &m_engine,
        path.constData(),
        MA_SOUND_FLAG_STREAM,
        nullptr,
        nullptr,
        &m_music
    );

    if (result != MA_SUCCESS) {
        qWarning() << "Failed to load music:" << filePath
                   << "Error:" << result;
        return false;
    }

    m_musicReady = true;

    // Loop the music continuously.
    ma_sound_set_looping(&m_music, MA_TRUE);

    // Apply the current volume.
    ma_sound_set_volume(&m_music, m_musicVolume);

    // Start playback.
    if (ma_sound_start(&m_music) != MA_SUCCESS) {
        qWarning() << "Failed to start music:" << filePath;
        ma_sound_uninit(&m_music);
        m_musicReady = false;
        return false;
    }

    return true;
}

void AudioManager::stopMusic()
{
    if (m_musicReady)
        ma_sound_stop(&m_music);
}

void AudioManager::setMusicVolume(float volume)
{
    m_musicVolume = qBound(0.0f, volume, 1.0f);

    if (m_musicReady)
        ma_sound_set_volume(&m_music, m_musicVolume);
}

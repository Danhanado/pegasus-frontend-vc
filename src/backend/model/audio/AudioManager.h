
#pragma once

#include <QObject>
#include <QString>

#include "../../../../thirdparty/miniaudio/miniaudio.h"

class AudioManager : public QObject
{
    Q_OBJECT

public:
    explicit AudioManager(QObject* parent = nullptr);
    ~AudioManager();

    Q_INVOKABLE bool playMusic(const QString& filePath);
    Q_INVOKABLE void stopMusic();
    Q_INVOKABLE void setMusicVolume(float volume);

private:
    ma_engine m_engine;
    ma_sound m_music;

    bool m_engineReady;
    bool m_musicReady;

    float m_musicVolume;
};

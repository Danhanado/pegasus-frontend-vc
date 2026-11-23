
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "../../../../thirdparty/miniaudio/miniaudio.h"

class BgmPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)

public:
    explicit BgmPlayer(QObject* parent = nullptr);
    ~BgmPlayer();

    bool isPlaying();
    Q_INVOKABLE void loadTrack(const QUrl& filePath, float volume);
    Q_INVOKABLE void playMusic();
    Q_INVOKABLE void stopMusic();
    Q_INVOKABLE void setMusicVolume(float volume);
    Q_INVOKABLE void fadeOutMusic(int duration);

    Q_INVOKABLE void suspendEngine();
    Q_INVOKABLE void resumeEngine();



signals:
    void isPlayingChanged();

private:
    ma_engine m_engine;
    ma_decoder m_decoder;
    ma_sound m_music;

    QString m_bgmPath;
    const char* m_bgmFile;
    QTimer m_fadeTimer;
    bool m_isFadingOut = false;

    bool m_engineReady;
    bool m_bgmReady;
    bool m_playing;

    float m_bgmVolume = 1.0f;
    int m_loopStart;
    int m_loopEnd;
    int m_loopLength;

    void setOggLoop(const char* fileName, ma_decoder* decoder);
};

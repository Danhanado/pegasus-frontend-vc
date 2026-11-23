#include "BgmPlayer.h"
#include "qguiapplication.h"
#include "qurl.h"

#include <QByteArray>
#include <QDebug>
#include <QtGlobal>
#include <QFileInfo>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

BgmPlayer::BgmPlayer(QObject* parent)
    : QObject(parent)
    , m_bgmPath("")
    , m_engineReady(false)
    , m_bgmReady(false)
{
    const ma_result result = ma_engine_init(nullptr, &m_engine);

    if (result != MA_SUCCESS){
        qWarning() << "Failed to initialise miniaudio:" << result;
        return;
    }
    if (qApp) {
        connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
            if (!m_engineReady){return;}
            if (state == Qt::ApplicationActive) {
               ma_engine_start(&m_engine);
            } else {
               ma_engine_stop(&m_engine);
            }
        });
    }

    m_fadeTimer.setSingleShot(true);
    connect(&m_fadeTimer, &QTimer::timeout, this, [this]() {
        emit isPlayingChanged();
    });

    m_engineReady = true;
}

BgmPlayer::~BgmPlayer(){
    if (m_bgmReady){
        ma_sound_uninit(&m_music);
        ma_decoder_uninit(&m_decoder);
        m_bgmReady = false;
    }
    if (m_engineReady){
        ma_engine_uninit(&m_engine);
        m_engineReady = false;
    }
}

void BgmPlayer::loadTrack(const QUrl& filePath, float volume){
    // Release the previous music track, if there is one.
    if (m_bgmReady){
        ma_sound_uninit(&m_music);
        ma_decoder_uninit(&m_decoder);
        m_bgmReady = false;
    }

    m_bgmPath = filePath.toLocalFile();

    QFileInfo fileInfo(m_bgmPath);

    if (!fileInfo.exists() || !fileInfo.isFile()){
        qWarning() << "miniaudio: file does not exist:" << m_bgmPath;
        m_bgmPath.clear();
        emit isPlayingChanged();
        return;
    }

    m_bgmFile = m_bgmPath.toUtf8().constData();

    //Initialize the decoder directly from the OGG file
    ma_result result = ma_decoder_init_file(m_bgmFile, nullptr, &m_decoder);
    if (result != MA_SUCCESS){
        qWarning() << "miniaudio: failed to initialize decoder:"
                   << m_bgmPath << "Error:" << result;
        m_bgmPath.clear();
        emit isPlayingChanged();
        return;
    }

    setOggLoop(m_bgmFile, &m_decoder);

    ma_uint32 musicFlags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_LOOPING | MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;

    result = ma_sound_init_from_data_source(
        &m_engine,
        &m_decoder,
        musicFlags,
        nullptr,
        &m_music
        );

    if (result != MA_SUCCESS){
        qWarning() << "miniaudio: failed to initialize sound:"
                   << m_bgmPath << "Error:" << result;

        ma_decoder_uninit(&m_decoder);
        m_bgmPath.clear();
        emit isPlayingChanged();
        return;
    }
    m_bgmReady = true;
    m_bgmVolume = qBound(0.0f, volume, 1.0f);
    ma_sound_set_volume(&m_music, m_bgmVolume);

}

void BgmPlayer::playMusic(){
    if (!m_engineReady || !m_bgmReady){return;}
    ma_sound_seek_to_pcm_frame(&m_music, 0);
    m_fadeTimer.stop();
    if (ma_sound_start(&m_music) != MA_SUCCESS) {
        qWarning() << "miniaudio: failed to start music:" << m_bgmPath;
        return;
    }
    emit isPlayingChanged();
}

void BgmPlayer::stopMusic(){
    if (!m_bgmReady){return;}
    m_fadeTimer.stop();
    ma_sound_stop(&m_music);
    emit isPlayingChanged();
}

void BgmPlayer::fadeOutMusic(int duration){
    if (!m_bgmReady){return;}
    ma_sound_stop_with_fade_in_milliseconds(&m_music, duration);
    m_fadeTimer.start(duration+100);
}

void BgmPlayer::setMusicVolume(float volume){
    m_bgmVolume = qBound(0.0f, volume, 1.0f);
    if (m_bgmReady){ma_sound_set_volume(&m_music, m_bgmVolume);}
}

bool BgmPlayer::isPlaying(){
    if (!m_engineReady || !m_bgmReady){return false;}
    m_playing = ma_sound_is_playing(&m_music);
    return m_playing;
}


void BgmPlayer::setOggLoop(const char* fileName, ma_decoder* decoder){
    stb_vorbis* vorbis = stb_vorbis_open_filename(fileName, 0, nullptr);
    if (!vorbis){return;}

    const ma_uint64 totalSamples = stb_vorbis_stream_length_in_samples(vorbis);
    const stb_vorbis_comment comment = stb_vorbis_get_comment(vorbis);

    ma_uint64 loopStart = 0;
    ma_uint64 loopEnd = totalSamples;
    ma_uint64 loopLength = 0;
    bool hasLoopStart = false;
    bool hasLoopEnd = false;
    bool hasLoopLength = false;

    // Fetch loop tags
    for (int i = 0; i < comment.comment_list_length; ++i){
        const QString entry = QString::fromUtf8(comment.comment_list[i]);
        const int tagValueIndex = entry.indexOf(QLatin1Char('='));
        if (tagValueIndex <= 0){continue;}

        const QString tagLabel = entry.left(tagValueIndex).trimmed().toUpper();
        const ma_uint64 tagValue = entry.mid(tagValueIndex+1).trimmed().toULongLong();

        if (tagLabel == QLatin1String("LOOPSTART") || tagLabel == QLatin1String("LOOP_START")){
            loopStart = tagValue;
            hasLoopStart = true;
        }else if (tagLabel == QLatin1String("LOOPEND") || tagLabel == QLatin1String("LOOP_END")){
            loopEnd = tagValue;
            hasLoopEnd = true;
        }else if (tagLabel == QLatin1String("LOOPLENGTH") || tagLabel == QLatin1String("LOOP_LENGTH")){
            loopLength = tagValue;
            hasLoopLength = true;
        }
        if (hasLoopStart && (hasLoopEnd || hasLoopLength)){break;} // Only needs START + END or LENGTH
    }

    stb_vorbis_close(vorbis);
    if (hasLoopStart){
        if (hasLoopEnd){
            if (loopEnd > loopStart){
                ma_data_source_set_loop_point_in_pcm_frames(decoder, loopStart, loopEnd);
            }
        }else if (hasLoopLength){
            ma_data_source_set_loop_point_in_pcm_frames(decoder, loopStart, loopStart+loopLength);
        }
    }else{
        ma_data_source_set_loop_point_in_pcm_frames(decoder, loopStart, loopEnd);
    }
}

void BgmPlayer::suspendEngine(){
    if (m_engineReady){ma_engine_stop(&m_engine);}
}

void BgmPlayer::resumeEngine(){
    if (m_engineReady){ma_engine_start(&m_engine);}
}

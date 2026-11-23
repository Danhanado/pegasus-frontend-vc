
#include "BgmPlayer.h"
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
    , m_bgmVolume(1.0f)
{
    // Start the miniaudio engine and audio device.
    const ma_result result = ma_engine_init(nullptr, &m_engine);

    if (result != MA_SUCCESS) {
        qWarning() << "Failed to initialise miniaudio:" << result;
        return;
    }

    m_engineReady = true;
}

BgmPlayer::~BgmPlayer()
{
    if (m_bgmReady) {
        ma_sound_uninit(&m_music);
        ma_decoder_uninit(&m_decoder);
        m_bgmReady = false;
    }

    if (m_engineReady) {
        ma_engine_uninit(&m_engine);
        m_engineReady = false;
    }
}

void BgmPlayer::loadTrack(const QUrl& filePath, float volume){
    m_bgmPath = filePath.toLocalFile();
    ma_engine_set_volume(&m_engine, volume);
    //applyOggLoopPoints(m_bgmPath, &m_music);

}

bool BgmPlayer::playMusic()
{
    // Release the previous music track, if there is one.
    if (m_bgmReady) {
        ma_sound_uninit(&m_music);
        ma_decoder_uninit(&m_decoder);
        m_bgmReady = false;
    }

    if (!m_engineReady || m_bgmPath.isEmpty())
        return false;

    if (m_bgmReady) {
        ma_sound_uninit(&m_music);
        ma_decoder_uninit(&m_decoder);
        m_bgmReady = false;
    }

    const QByteArray path = m_bgmPath.toUtf8();


    // 1. Initialize the decoder directly from the OGG file
    if (ma_decoder_init_file(path.constData(), nullptr, &m_decoder) != MA_SUCCESS) {
        qWarning() << "Failed to decode music:" << m_bgmPath;
        return false;
    }

    // 2. Apply LOOPSTART / LOOPLENGTH directly on &m_decoder (which IS a ma_data_source)
    applyOggLoopPoints(m_bgmPath, &m_decoder);

    // 3. Attach the decoder to m_music as its data source
    const ma_result result = ma_sound_init_from_data_source(
        &m_engine,
        &m_decoder,
        MA_SOUND_FLAG_STREAM,
        nullptr,
        &m_music
        );

    if (result != MA_SUCCESS) {
        qWarning() << "Failed to init sound from decoder:" << m_bgmPath << "Error:" << result;
        ma_decoder_uninit(&m_decoder);
        return false;
    }

    m_bgmReady = true;

    // Loop the music continuously.
    ma_sound_set_looping(&m_music, MA_TRUE);

    // Start playback.
    if (ma_sound_start(&m_music) != MA_SUCCESS) {
        qWarning() << "Failed to start music:" << m_bgmPath;
        ma_sound_uninit(&m_music);
        ma_decoder_uninit(&m_decoder);
        m_bgmReady = false;
        return false;
    }
    return true;
}

void BgmPlayer::stopMusic()
{
    if (m_bgmReady)
        ma_sound_stop(&m_music);
}

void BgmPlayer::setMusicVolume(float volume)
{
    m_bgmVolume = qBound(0.0f, volume, 1.0f);

    if (m_bgmReady)
        ma_sound_set_volume(&m_music, m_bgmVolume);
}

bool BgmPlayer::isPlaying(){
    if (!m_bgmReady){return false;}
    return ma_sound_is_playing(&m_music);
}


void BgmPlayer::applyOggLoopPoints(const QString& filePath, ma_decoder* decoder)
{
    int error = 0;
    const char* fileName = filePath.toUtf8().constData();
    stb_vorbis* vorbis = stb_vorbis_open_filename(fileName, &error, nullptr);
    if (!vorbis) {
        qWarning() << "applyOggLoopPoints: stb_vorbis failed to open" << filePath << "error:" << error;
        return;
    }

    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    const ma_uint64 totalFrames = stb_vorbis_stream_length_in_samples(vorbis);
    const stb_vorbis_comment comment = stb_vorbis_get_comment(vorbis);

    ma_uint64 loopStart = 0;
    ma_uint64 loopEnd = 0;
    ma_uint64 loopLength = 0;
    bool hasStart = false;
    bool hasEnd = false;
    bool hasLength = false;

    auto parseFrameValue = [&](const QString& rawVal, ma_uint64& outFrames) -> bool {
        bool ok = false;
        const ma_uint64 samples = rawVal.toULongLong(&ok);
        if (ok) {
            outFrames = samples;
            return true;
        }
        // Fallback: parse "MM:SS.mmm" or "HH:MM:SS.mmm" timestamp strings
        const QStringList parts = rawVal.split(QLatin1Char(':'));
        if (parts.size() >= 2 && parts.size() <= 3 && info.sample_rate > 0) {
            double seconds = 0.0;
            for (const QString& part : parts) {
                bool partOk = false;
                const double v = part.toDouble(&partOk);
                if (!partOk)
                    return false;
                seconds = seconds * 60.0 + v;
            }
            outFrames = static_cast<ma_uint64>(seconds * info.sample_rate);
            return true;
        }
        return false;
    };

    for (int i = 0; i < comment.comment_list_length; ++i) {
        const QString entry = QString::fromUtf8(comment.comment_list[i]);
        qDebug() << "OGG tag:" << entry;

        const int eqIndex = entry.indexOf(QLatin1Char('='));
        if (eqIndex <= 0)
            continue;

        const QString key = entry.left(eqIndex).trimmed().toUpper();
        const QString value = entry.mid(eqIndex + 1).trimmed();

        ma_uint64 frames = 0;
        if (!parseFrameValue(value, frames))
            continue;

        if (key == QLatin1String("LOOPSTART") || key == QLatin1String("LOOP_START")) {
            loopStart = frames;
            hasStart = true;
        } else if (key == QLatin1String("LOOPEND") || key == QLatin1String("LOOP_END")) {
            loopEnd = frames;
            hasEnd = true;
        } else if (key == QLatin1String("LOOPLENGTH") || key == QLatin1String("LOOP_LENGTH")) {
            loopLength = frames;
            hasLength = true;
        }
    }

    stb_vorbis_close(vorbis);

    if (!hasStart && !hasEnd && !hasLength) {
        qDebug() << "No loop tags found in" << filePath;
        return;
    }

    if (!hasEnd) {
        if (hasLength && loopLength > 0)
            loopEnd = loopStart + loopLength;
        else
            loopEnd = totalFrames;
    }

    if (totalFrames > 0 && loopEnd > totalFrames)
        loopEnd = totalFrames;


    if (loopEnd > loopStart) {
        const ma_result res = ma_data_source_set_loop_point_in_pcm_frames(decoder, loopStart, loopEnd);
        qDebug() << "Set OGG loop points:" << loopStart << "->" << loopEnd << "result:" << res;
    } else {
        qWarning() << "Invalid loop points:" << loopStart << "->" << loopEnd << "(total:" << totalFrames << ")";
    }
}

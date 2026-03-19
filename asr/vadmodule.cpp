#include "vadmodule.h"

#include <cstring>

#include "sherpa-onnx/c-api/c-api.h"

VadModule::~VadModule()
{
    if (m_vad) {
        SherpaOnnxDestroyVoiceActivityDetector(m_vad);
        m_vad = nullptr;
    }
}

bool VadModule::initialize(const QString &vadModelPath)
{
    if (m_vad) {
        SherpaOnnxDestroyVoiceActivityDetector(m_vad);
        m_vad = nullptr;
    }
    m_pendingSegments.clear();

    if (vadModelPath.trimmed().isEmpty())
        return false;

    QByteArray vadUtf8 = vadModelPath.toUtf8();
    SherpaOnnxVadModelConfig vadConfig;
    std::memset(&vadConfig, 0, sizeof(vadConfig));
    vadConfig.silero_vad.model = vadUtf8.constData();
    vadConfig.silero_vad.threshold = 0.5f;
    vadConfig.silero_vad.min_silence_duration = 0.35f;
    vadConfig.silero_vad.min_speech_duration = 0.10f;
    vadConfig.silero_vad.window_size = 512;
    vadConfig.silero_vad.max_speech_duration = 20.0f;
    vadConfig.sample_rate = 16000;
    vadConfig.num_threads = 1;
    vadConfig.provider = "cpu";
    vadConfig.debug = 0;

    m_vad = SherpaOnnxCreateVoiceActivityDetector(&vadConfig, 30.0f);
    return m_vad != nullptr;
}

bool VadModule::isAvailable() const
{
    return m_vad != nullptr;
}

void VadModule::clear()
{
    m_pendingSegments.clear();
    if (m_vad)
        SherpaOnnxVoiceActivityDetectorClear(m_vad);
}

void VadModule::flush()
{
    if (!m_vad)
        return;
    SherpaOnnxVoiceActivityDetectorFlush(m_vad);
    collectAvailableSegments();
}

void VadModule::acceptSamples(const std::vector<float> &samples)
{
    if (!m_vad || samples.empty())
        return;
    SherpaOnnxVoiceActivityDetectorAcceptWaveform(m_vad, samples.data(), static_cast<int32_t>(samples.size()));
    collectAvailableSegments();
}

bool VadModule::popSegment(std::vector<float> &outSegment)
{
    if (m_pendingSegments.empty())
        return false;
    outSegment = std::move(m_pendingSegments.front());
    m_pendingSegments.pop_front();
    return !outSegment.empty();
}

void VadModule::collectAvailableSegments()
{
    if (!m_vad)
        return;

    while (!SherpaOnnxVoiceActivityDetectorEmpty(m_vad)) {
        const SherpaOnnxSpeechSegment *seg = SherpaOnnxVoiceActivityDetectorFront(m_vad);
        if (seg && seg->samples && seg->n > 0) {
            m_pendingSegments.emplace_back(seg->samples, seg->samples + seg->n);
        }
        SherpaOnnxVoiceActivityDetectorPop(m_vad);
        if (seg)
            SherpaOnnxDestroySpeechSegment(seg);
    }
}

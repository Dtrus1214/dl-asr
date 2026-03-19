#include "asrengine.h"
#include "vadmodule.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <vector>
#include <cstring>

#include "sherpa-onnx/c-api/c-api.h"

AsrEngine::AsrEngine(QObject *parent)
    : QObject(parent)
{
    m_vadModule = new VadModule();
}

AsrEngine::~AsrEngine()
{
    delete m_vadModule;
    m_vadModule = nullptr;
    if (m_recognizer) {
        SherpaOnnxDestroyOfflineRecognizer(m_recognizer);
        m_recognizer = nullptr;
    }
}

bool AsrEngine::initialize()
{
    if (m_vadModule)
        m_vadModule->clear();
    if (m_recognizer) {
        SherpaOnnxDestroyOfflineRecognizer(m_recognizer);
        m_recognizer = nullptr;
    }

    const QString tokensPath = defaultTokensPath();
    const QString modelPath = defaultModelPath();
    const QString encoderPath = defaultEncoderPath();
    const QString decoderPath = defaultDecoderPath();
    const QString joinerPath = defaultJoinerPath();

    const bool hasSingleModel = QFileInfo::exists(modelPath);
    const bool hasTransducer = QFileInfo::exists(encoderPath)
                            && QFileInfo::exists(decoderPath)
                            && QFileInfo::exists(joinerPath);

    if ((!hasSingleModel && !hasTransducer) || !QFileInfo::exists(tokensPath)) {
        m_available = false;
        setState(BackendError);
        return false;
    }

    QByteArray modelUtf8 = modelPath.toUtf8();
    QByteArray encoderUtf8 = encoderPath.toUtf8();
    QByteArray decoderUtf8 = decoderPath.toUtf8();
    QByteArray joinerUtf8 = joinerPath.toUtf8();
    QByteArray tokensUtf8 = tokensPath.toUtf8();

    SherpaOnnxOfflineRecognizerConfig config;
    std::memset(&config, 0, sizeof(config));
    config.feat_config.sample_rate = 16000;
    config.feat_config.feature_dim = 80;
    if (hasTransducer) {
        config.model_config.transducer.encoder = encoderUtf8.constData();
        config.model_config.transducer.decoder = decoderUtf8.constData();
        config.model_config.transducer.joiner = joinerUtf8.constData();
    } else {
        config.model_config.paraformer.model = modelUtf8.constData();
    }
    config.model_config.tokens = tokensUtf8.constData();
    config.model_config.num_threads = 1;
    config.model_config.provider = "cpu";
    config.model_config.debug = 0;
    config.decoding_method = "greedy_search";
    config.max_active_paths = 4;

    m_recognizer = SherpaOnnxCreateOfflineRecognizer(&config);
    if (!m_recognizer) {
        m_available = false;
        setState(BackendError);
        return false;
    }

    const QString vadPath = defaultVadModelPath();
    if (m_vadModule && QFileInfo::exists(vadPath))
        m_vadModule->initialize(vadPath);

    m_available = true;
    setState(Ready);
    return true;
}

bool AsrEngine::isAvailable() const
{
    return m_available;
}

int AsrEngine::state() const
{
    return m_state;
}

void AsrEngine::setLanguage(const QString &languageCode)
{
    m_language = languageCode.trimmed();
}

QString AsrEngine::language() const
{
    return m_language;
}

void AsrEngine::transcribe(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (!m_available || trimmed.isEmpty())
        return;

    const quint64 requestId = ++m_requestId;
    setState(Processing);

    QFuture<QString> future = QtConcurrent::run([trimmed]() {
        // Placeholder ASR backend: keep text normalization path in place.
        return trimmed;
    });
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, requestId]() {
        const QString out = watcher->result();
        watcher->deleteLater();
        if (requestId != m_requestId)
            return;
        setState(Ready);
        emit transcriptionReady(out);
    });
    watcher->setFuture(future);
}

void AsrEngine::transcribePcm16(const QByteArray &pcm16Mono, int sampleRate)
{
    if (!m_available && !initialize())
        return;
    if (m_state == Processing)
        return;
    if (!m_recognizer || pcm16Mono.isEmpty() || sampleRate <= 0)
        return;

    const int sampleCount = pcm16Mono.size() / static_cast<int>(sizeof(qint16));
    if (sampleCount <= 0)
        return;
    std::vector<float> samples(static_cast<size_t>(sampleCount));
    const qint16 *src = reinterpret_cast<const qint16 *>(pcm16Mono.constData());
    for (int i = 0; i < sampleCount; ++i)
        samples[static_cast<size_t>(i)] = static_cast<float>(src[i]) / 32768.0f;
    startDecodeSamples(samples, sampleRate);
}

void AsrEngine::acceptAudioChunkPcm16(const QByteArray &pcm16Mono, int sampleRate)
{
    if (!m_available && !initialize())
        return;
    if (!m_recognizer || pcm16Mono.isEmpty() || sampleRate <= 0)
        return;

    if (!m_vadModule || !m_vadModule->isAvailable()) {
        transcribePcm16(pcm16Mono, sampleRate);
        return;
    }

    const int sampleCount = pcm16Mono.size() / static_cast<int>(sizeof(qint16));
    if (sampleCount <= 0)
        return;
    std::vector<float> samples(static_cast<size_t>(sampleCount));
    const qint16 *src = reinterpret_cast<const qint16 *>(pcm16Mono.constData());
    for (int i = 0; i < sampleCount; ++i)
        samples[static_cast<size_t>(i)] = static_cast<float>(src[i]) / 32768.0f;

    m_vadModule->acceptSamples(samples);
    if (m_state == Processing)
        return;

    std::vector<float> segSamples;
    if (m_vadModule->popSegment(segSamples))
        startDecodeSamples(segSamples, sampleRate);
}

void AsrEngine::flushAudio()
{
    if (!m_vadModule || !m_vadModule->isAvailable())
        return;
    m_vadModule->flush();
    if (m_state == Processing)
        return;
    std::vector<float> segSamples;
    if (m_vadModule->popSegment(segSamples))
        startDecodeSamples(segSamples, 16000);
}

void AsrEngine::stop()
{
    ++m_requestId;
    if (m_vadModule)
        m_vadModule->clear();
    setState(Ready);
}

void AsrEngine::setState(int nextState)
{
    if (m_state == nextState)
        return;
    m_state = nextState;
    emit stateChanged(m_state);
}

QString AsrEngine::defaultModelPath() const
{
    return QCoreApplication::applicationDirPath() + QLatin1String("/asr-model/model.int8.onnx");
}

QString AsrEngine::defaultEncoderPath() const
{
    return QCoreApplication::applicationDirPath() + QLatin1String("/asr-model/encoder.onnx");
}

QString AsrEngine::defaultDecoderPath() const
{
    return QCoreApplication::applicationDirPath() + QLatin1String("/asr-model/decoder.onnx");
}

QString AsrEngine::defaultJoinerPath() const
{
    return QCoreApplication::applicationDirPath() + QLatin1String("/asr-model/joiner.onnx");
}

QString AsrEngine::defaultTokensPath() const
{
    return QCoreApplication::applicationDirPath() + QLatin1String("/asr-model/tokens.txt");
}

QString AsrEngine::defaultVadModelPath() const
{
    return QCoreApplication::applicationDirPath() + QLatin1String("/asr-model/silero_vad.onnx");
}

void AsrEngine::startDecodeSamples(const std::vector<float> &samples, int sampleRate)
{
    if (!m_recognizer || samples.empty() || sampleRate <= 0)
        return;
    if (m_state == Processing)
        return;

    const quint64 requestId = ++m_requestId;
    setState(Processing);
    const auto *recognizer = m_recognizer;
    const std::vector<float> input = samples;

    QFuture<QString> future = QtConcurrent::run([recognizer, input, sampleRate]() -> QString {
        const SherpaOnnxOfflineStream *stream = SherpaOnnxCreateOfflineStream(recognizer);
        if (!stream)
            return QString();

        SherpaOnnxAcceptWaveformOffline(stream, sampleRate, input.data(), static_cast<int32_t>(input.size()));
        SherpaOnnxDecodeOfflineStream(recognizer, stream);
        const SherpaOnnxOfflineRecognizerResult *result = SherpaOnnxGetOfflineStreamResult(stream);
        QString text;
        if (result && result->text)
            text = QString::fromUtf8(result->text).trimmed();

        if (result)
            SherpaOnnxDestroyOfflineRecognizerResult(result);
        SherpaOnnxDestroyOfflineStream(stream);
        return text;
    });

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, requestId]() {
        const QString out = watcher->result();
        watcher->deleteLater();
        if (requestId != m_requestId)
            return;
        setState(Ready);
        if (!out.isEmpty())
            emit transcriptionReady(out);
    });
    watcher->setFuture(future);
}

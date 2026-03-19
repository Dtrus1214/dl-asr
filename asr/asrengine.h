#ifndef ASRENGINE_H
#define ASRENGINE_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <vector>

struct SherpaOnnxOfflineRecognizer;
class VadModule;

class AsrEngine : public QObject
{
    Q_OBJECT
public:
    enum State {
        Ready = 0,
        Processing = 1,
        BackendError = 2
    };

    explicit AsrEngine(QObject *parent = nullptr);
    ~AsrEngine() override;

    bool initialize();
    bool isAvailable() const;
    int state() const;

    void setLanguage(const QString &languageCode);
    QString language() const;

    void transcribe(const QString &text);
    void transcribePcm16(const QByteArray &pcm16Mono, int sampleRate);
    void acceptAudioChunkPcm16(const QByteArray &pcm16Mono, int sampleRate);
    void flushAudio();
    void stop();

signals:
    void stateChanged(int state);
    void transcriptionReady(const QString &text);

private:
    void setState(int nextState);
    QString defaultModelPath() const;
    QString defaultEncoderPath() const;
    QString defaultDecoderPath() const;
    QString defaultJoinerPath() const;
    QString defaultTokensPath() const;
    QString defaultVadModelPath() const;
    void startDecodeSamples(const std::vector<float> &samples, int sampleRate);

    bool m_available = false;
    int m_state = Ready;
    QString m_language;
    quint64 m_requestId = 0;
    const SherpaOnnxOfflineRecognizer *m_recognizer = nullptr;
    VadModule *m_vadModule = nullptr;
};

#endif // ASRENGINE_H

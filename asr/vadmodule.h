#ifndef VADMODULE_H
#define VADMODULE_H

#include <QString>
#include <vector>
#include <deque>

struct SherpaOnnxVoiceActivityDetector;

class VadModule
{
public:
    VadModule() = default;
    ~VadModule();

    bool initialize(const QString &vadModelPath);
    bool isAvailable() const;
    void clear();
    void flush();

    void acceptSamples(const std::vector<float> &samples);
    bool popSegment(std::vector<float> &outSegment);

private:
    void collectAvailableSegments();

    const SherpaOnnxVoiceActivityDetector *m_vad = nullptr;
    std::deque<std::vector<float>> m_pendingSegments;
};

#endif // VADMODULE_H

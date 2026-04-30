#include "PolyVoice.h"
#include <algorithm>
#include <cmath>
#include <limits>

void PolyVoice::loadFromBuffer (juce::AudioBuffer<float>&& newBuffer,
                                double sourceSampleRate,
                                juce::String sourcePath)
{
    buffer           = std::move (newBuffer);
    bufferSampleRate = sourceSampleRate;
    bufferNumSamples = buffer.getNumSamples();
    filePath         = std::move (sourcePath);
    readPos.store (0.0);
    loopStartSamples.store (0);
    loopEndSamples.store (bufferNumSamples);
    playing.store (false);
    analyseTransients();
}

double PolyVoice::getLengthSeconds() const
{
    if (bufferNumSamples == 0) return 0.0;
    return bufferNumSamples / bufferSampleRate;
}

void PolyVoice::setLoopStart (double n)
{
    n = juce::jlimit (0.0, 1.0, n);
    auto newStart = (int64_t) (n * (double) bufferNumSamples);
    loopStartSamples.store (newStart);
    if (readPos.load() < (double) newStart)
        readPos.store ((double) newStart);
}

void PolyVoice::setLoopEnd (double n)
{
    n = juce::jlimit (0.0, 1.0, n);
    loopEndSamples.store ((int64_t) (n * (double) bufferNumSamples));
}

double PolyVoice::getLoopStartNorm() const
{
    if (bufferNumSamples == 0) return 0.0;
    return (double) loopStartSamples.load() / (double) bufferNumSamples;
}

double PolyVoice::getLoopEndNorm() const
{
    if (bufferNumSamples == 0) return 1.0;
    return (double) loopEndSamples.load() / (double) bufferNumSamples;
}

void PolyVoice::setPlayheadNorm (double n)
{
    if (bufferNumSamples == 0) return;
    readPos.store (juce::jlimit (0.0, 1.0, n) * (double) bufferNumSamples);
}

double PolyVoice::getPlayheadNorm() const
{
    if (bufferNumSamples == 0) return 0.0;
    return juce::jlimit (0.0, 1.0, readPos.load() / (double) bufferNumSamples);
}

void PolyVoice::jumpToLoopStart()
{
    readPos.store ((double) loopStartSamples.load());
}

void PolyVoice::renderInto (juce::AudioBuffer<float>& dest, int startSample, int numSamples)
{
    if (! playing.load() || bufferNumSamples <= 1)
        return;

    const int outChannels = dest.getNumChannels();
    const int srcChannels = buffer.getNumChannels();
    const int numCh       = juce::jmin (outChannels, srcChannels);

    double loopStart = (double) loopStartSamples.load();
    double loopEnd   = (double) loopEndSamples.load();
    if (loopEnd <= loopStart + 1.0)
        loopEnd = (double) bufferNumSamples;

    const double rateRatio = (bufferSampleRate / deviceSampleRate) * speed.load();
    double pos = readPos.load();

    for (int i = 0; i < numSamples; ++i)
    {
        if (pos >= loopEnd || pos < loopStart)
            pos = loopStart;

        const int   idx     = (int) pos;
        const float frac    = (float) (pos - idx);
        int         idxNext = idx + 1;
        if (idxNext >= bufferNumSamples)
            idxNext = (int) loopStart;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* src = buffer.getReadPointer (ch);
            const float s = src[idx] + frac * (src[idxNext] - src[idx]);
            dest.addSample (ch, startSample + i, s);
        }

        if (numCh == 1 && outChannels >= 2)
        {
            auto* src = buffer.getReadPointer (0);
            const float s = src[idx] + frac * (src[idxNext] - src[idx]);
            dest.addSample (1, startSample + i, s);
        }

        pos += rateRatio;
    }

    readPos.store (pos);
}

bool PolyVoice::snapshotForExport (juce::AudioBuffer<float>& outBuffer, double& outSr) const
{
    if (bufferNumSamples == 0) return false;
    outBuffer.makeCopyOf (buffer);
    outSr = bufferSampleRate;
    return true;
}

std::vector<double> PolyVoice::getActiveTransients() const
{
    if (transientLimit == 0 || allTransients.empty()) return {};

    auto copy = allTransients;
    if (transientLimit > 0 && (int) copy.size() > transientLimit)
    {
        std::nth_element (copy.begin(),
                          copy.begin() + transientLimit,
                          copy.end(),
                          [] (const Transient& a, const Transient& b)
                          { return a.strength > b.strength; });
        copy.resize ((size_t) transientLimit);
    }
    std::vector<double> out;
    out.reserve (copy.size());
    for (const auto& t : copy) out.push_back (t.position);
    std::sort (out.begin(), out.end());
    return out;
}

double PolyVoice::snapToNearestTransient (double n, double tolerance) const
{
    const auto active = getActiveTransients();
    if (active.empty()) return n;
    double bestDist = std::numeric_limits<double>::max();
    double bestPos  = n;
    for (double t : active)
    {
        const double d = std::abs (t - n);
        if (d < bestDist) { bestDist = d; bestPos = t; }
    }
    return bestDist <= tolerance ? bestPos : n;
}

void PolyVoice::analyseTransients()
{
    allTransients.clear();
    if (bufferNumSamples < 1024) return;

    const int win = 256;
    const int hop = win;
    const int srcCh = buffer.getNumChannels();

    std::vector<float> env;
    env.reserve ((size_t) (bufferNumSamples / hop));
    for (int i = 0; i + win <= bufferNumSamples; i += hop)
    {
        double sum = 0.0;
        for (int j = 0; j < win; ++j)
        {
            float s = buffer.getSample (0, i + j);
            if (srcCh > 1) s = 0.5f * (s + buffer.getSample (1, i + j));
            sum += (double) s * (double) s;
        }
        env.push_back ((float) std::sqrt (sum / (double) win));
    }
    if (env.size() < 4) return;

    std::vector<float> diff (env.size(), 0.0f);
    for (size_t i = 1; i < env.size(); ++i)
        diff[i] = juce::jmax (0.0f, env[i] - env[i - 1]);

    double sum = 0.0;
    for (float v : diff) sum += v;
    const double mean = sum / (double) diff.size();
    double var = 0.0;
    for (float v : diff) { const double d = v - mean; var += d * d; }
    const double stdev = std::sqrt (var / (double) diff.size());
    const float thresh = (float) (mean + 2.0 * stdev);

    const int minSpacingHops = juce::jmax (1, (int) (0.05 * bufferSampleRate / hop));
    int lastPick = -minSpacingHops;
    for (int i = 1; i < (int) diff.size() - 1; ++i)
    {
        if (diff[i] > thresh
            && diff[i] >= diff[i - 1]
            && diff[i] >= diff[i + 1]
            && (i - lastPick) >= minSpacingHops)
        {
            allTransients.push_back ({ (double) (i * hop) / (double) bufferNumSamples,
                                       diff[i] });
            lastPick = i;
        }
    }
}

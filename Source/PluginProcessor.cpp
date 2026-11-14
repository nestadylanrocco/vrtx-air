#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    constexpr float midSplitHz  = 3000.0f;   // inizio banda MID AIR
    constexpr float highSplitHz = 10000.0f;  // inizio banda HIGH AIR

    // Soglie della parte dinamica: sopra questo livello la banda viene esaltata meno
    constexpr float midThreshold  = 0.08f;
    constexpr float highThreshold = 0.05f;

    inline float follow (float env, float input, float att, float rel) noexcept
    {
        const float coef = input > env ? att : rel;
        return input + coef * (env - input);
    }
}

VRTXAirProcessor::VRTXAirProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    midParam  = apvts.getRawParameterValue ("mid");
    highParam = apvts.getRawParameterValue ("high");
    trimParam = apvts.getRawParameterValue ("trim");
}

juce::AudioProcessorValueTreeState::ParameterLayout VRTXAirProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = [] (float v, int) { return String (v, 1) + " %"; };
    auto decibel = [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; };

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mid", 1 }, "Mid Air",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "high", 1 }, "High Air",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (percent)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "trim", 1 }, "Trim",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (decibel)));

    return layout;
}

bool VRTXAirProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void VRTXAirProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    numChannelsPrepared = juce::jmax (1, juce::jmin (2, getTotalNumOutputChannels()));

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numChannelsPrepared, (size_t) osFactorLog2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples (juce::roundToInt (oversampler->getLatencyInSamples()));

    const double osRate  = sampleRate * (double) (1 << osFactorLog2);
    const int    osBlock = samplesPerBlock * (1 << osFactorLog2);

    juce::dsp::ProcessSpec osSpec { osRate, (juce::uint32) osBlock, (juce::uint32) numChannelsPrepared };

    for (auto* f : { &splitMid, &splitHigh, &postMid, &postHigh })
    {
        f->setType (f == &splitMid || f == &splitHigh ? SVF::Type::lowpass : SVF::Type::highpass);
        f->setResonance (1.0f / std::sqrt (2.0f));
        f->prepare (osSpec);
        f->reset();
    }
    splitMid.setCutoffFrequency  (midSplitHz);
    postMid.setCutoffFrequency   (midSplitHz);
    splitHigh.setCutoffFrequency (highSplitHz);
    postHigh.setCutoffFrequency  (highSplitHz);

    attackCoef  = (float) std::exp (-1.0 / (0.002 * osRate)); // 2 ms
    releaseCoef = (float) std::exp (-1.0 / (0.080 * osRate)); // 80 ms
    envMid.fill (0.0f);
    envHigh.fill (0.0f);

    midSmooth.reset  (osRate, 0.03);
    highSmooth.reset (osRate, 0.03);
    midSmooth.setCurrentAndTargetValue  (midParam->load()  * 0.01f);
    highSmooth.setCurrentAndTargetValue (highParam->load() * 0.01f);

    midArr.assign  ((size_t) osBlock, 0.0f);
    highArr.assign ((size_t) osBlock, 0.0f);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numChannelsPrepared };
    trimGain.prepare (spec);
    trimGain.setRampDurationSeconds (0.03);
    trimGain.setGainDecibels (trimParam->load());
}

void VRTXAirProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (oversampler == nullptr || buffer.getNumSamples() == 0)
        return;

    const int numCh = juce::jmin (numChannelsPrepared, buffer.getNumChannels());
    juce::dsp::AudioBlock<float> block (buffer);
    auto mainBlock = block.getSubsetChannelBlock (0, (size_t) numCh);

    auto up = oversampler->processSamplesUp (mainBlock);
    const int n = (int) up.getNumSamples();

    if ((int) midArr.size() < n)
    {
        midArr.resize ((size_t) n);
        highArr.resize ((size_t) n);
    }

    midSmooth.setTargetValue  (midParam->load()  * 0.01f);
    highSmooth.setTargetValue (highParam->load() * 0.01f);
    for (int i = 0; i < n; ++i)
    {
        midArr[(size_t) i]  = midSmooth.getNextValue();
        highArr[(size_t) i] = highSmooth.getNextValue();
    }

    for (int ch = 0; ch < (int) up.getNumChannels(); ++ch)
    {
        float* d = up.getChannelPointer ((size_t) ch);
        float eM = envMid[(size_t) ch];
        float eH = envHigh[(size_t) ch];

        for (int i = 0; i < n; ++i)
        {
            const float x    = d[i];
            const float mAmt = midArr[(size_t) i];
            const float hAmt = highArr[(size_t) i];

            // 1) Separazione con passa-basso complementari (nessun buco di fase):
            //    banda media = LP10k - LP3k (3-10 kHz), banda alta = x - LP10k (>10 kHz)
            const float lp3   = splitMid.processSample  (ch, x);
            const float lp10  = splitHigh.processSample (ch, x);
            const float bandM = lp10 - lp3;
            const float bandH = x - lp10;

            // 2) Parte dinamica: più la banda è forte, meno la esaltiamo (niente sibilanti aggressive)
            eM = follow (eM, std::abs (bandM), attackCoef, releaseCoef);
            eH = follow (eH, std::abs (bandH), attackCoef, releaseCoef);
            const float dynM = 1.0f / (1.0f + eM / midThreshold);
            const float dynH = 1.0f / (1.0f + eH / highThreshold);

            // 3) Armoniche morbide generate dalla banda, poi ripulite sotto la frequenza di taglio
            const float kM = 3.0f + 6.0f * mAmt;
            const float kH = 3.0f + 8.0f * hAmt;
            const float harmM = postMid.processSample  (ch, bandM - std::tanh (bandM * kM) / kM);
            const float harmH = postHigh.processSample (ch, bandH - std::tanh (bandH * kH) / kH);

            // 4) Somma al segnale originale
            const float addM = mAmt * dynM * (1.0f * bandM + 1.5f * harmM);
            const float addH = hAmt * dynH * (1.6f * bandH + 2.0f * harmH);

            d[i] = x + addM + addH;
        }

        envMid[(size_t) ch]  = eM;
        envHigh[(size_t) ch] = eH;
    }

    oversampler->processSamplesDown (mainBlock);

    trimGain.setGainDecibels (trimParam->load());
    juce::dsp::ProcessContextReplacing<float> ctx (mainBlock);
    trimGain.process (ctx);

    // Meter L/R (tieni il picco più alto finché la GUI non lo legge)
    const int ns = buffer.getNumSamples();
    const float pL = buffer.getMagnitude (0, 0, ns);
    const float pR = numCh > 1 ? buffer.getMagnitude (1, 0, ns) : pL;
    if (pL > peakL.load()) peakL.store (pL);
    if (pR > peakR.load()) peakR.store (pR);

    // Campioni mono per lo spettro della GUI
    {
        const auto scope = analyserFifo.write (ns);
        const float* l = buffer.getReadPointer (0);
        const float* r = numCh > 1 ? buffer.getReadPointer (1) : l;
        auto copy = [&] (int start, int size, int offset)
        {
            for (int i = 0; i < size; ++i)
                analyserBuffer[(size_t) (start + i)] = 0.5f * (l[offset + i] + r[offset + i]);
        };
        if (scope.blockSize1 > 0) copy (scope.startIndex1, scope.blockSize1, 0);
        if (scope.blockSize2 > 0) copy (scope.startIndex2, scope.blockSize2, scope.blockSize1);
    }
}

void VRTXAirProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VRTXAirProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VRTXAirProcessor::createEditor()
{
    return new VRTXAirEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VRTXAirProcessor();
}

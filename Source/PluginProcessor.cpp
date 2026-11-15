#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    // ---- Motore "Dolby-A trick" -------------------------------------------
    // Una copia della banda viene compressa verso l'alto e sommata in parallelo:
    // i dettagli deboli (respiri, code delle consonanti) salgono fino a +10/+12 dB,
    // le parti forti (S, attacchi) restano quasi invariate.
    constexpr float midThreshold  = 0.08f;   // circa -22 dBFS nella banda MID
    constexpr float highThreshold = 0.04f;   // circa -28 dBFS nella banda HIGH
    constexpr float midMaxGain    = 2.0f;    // parte dinamica (dettagli deboli)
    constexpr float highMaxGain   = 2.5f;
    constexpr float midStatic     = 1.0f;    // parte fissa (la "base" di brillantezza)
    constexpr float highStatic    = 1.5f;

    // ---- Exciter ----------------------------------------------------------
    constexpr float exciterAmount = 2.5f;    // armoniche nuove sopra i 7 kHz

    inline float follow (float env, float input, float att, float rel) noexcept
    {
        const float coef = input > env ? att : rel;
        return input + coef * (env - input);
    }

    // Saturazione morbida sul segnale aggiunto: niente picchi aspri
    inline float soft (float v) noexcept { return std::tanh (v * 1.25f) * 0.8f; }
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

    auto setup = [&] (SVF& f, SVF::Type type, float hz)
    {
        f.setType (type);
        f.setResonance (1.0f / std::sqrt (2.0f));
        f.setCutoffFrequency (hz);
        f.prepare (osSpec);
        f.reset();
    };
    setup (lp3k,   SVF::Type::bandpass, 5200.0f);
    lp3k.setResonance (0.6f);
    setup (lp9k,   SVF::Type::lowpass,  9000.0f);
    setup (lp2k,   SVF::Type::lowpass,  2000.0f);
    setup (lp6k,   SVF::Type::lowpass,  6000.0f);
    setup (excHp1, SVF::Type::highpass, 7000.0f);
    setup (excHp2, SVF::Type::highpass, 7000.0f);

    attackCoef  = (float) std::exp (-1.0 / (0.001 * osRate)); // 1 ms
    releaseCoef = (float) std::exp (-1.0 / (0.060 * osRate)); // 60 ms
    meterCoef   = (float) std::exp (-1.0 / (0.030 * osRate));
    envMid.fill (0.0f);
    envHigh.fill (0.0f);
    envHf.fill (0.0f);

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

    double sumMidGain = 0.0, sumHighGain = 0.0;
    float hfLevel = 0.0f;

    for (int ch = 0; ch < (int) up.getNumChannels(); ++ch)
    {
        float* d = up.getChannelPointer ((size_t) ch);
        float eM = envMid[(size_t) ch], eH = envHigh[(size_t) ch], eHf = envHf[(size_t) ch];

        for (int i = 0; i < n; ++i)
        {
            const float x    = d[i];
            const float mAmt = midArr[(size_t) i];
            const float hAmt = highArr[(size_t) i];

            // 1) Bande: MID = 3-9 kHz, HIGH = sopra 9 kHz
            // MID: passa-banda largo centrato a 5.2 kHz (in fase: non taglia mai nulla)
            const float bandM = lp3k.processSample (ch, x);
            const float l9 = lp9k.processSample (ch, x);
            const float bandH = x - l9;

            // 2) Compressione verso l'alto in parallelo (Dolby-A trick)
            eM = follow (eM, std::abs (bandM), attackCoef, releaseCoef);
            eH = follow (eH, std::abs (bandH), attackCoef, releaseCoef);
            const float gM = juce::jmin (1.0f, midThreshold  / (eM + 1.0e-9f));
            const float gH = juce::jmin (1.0f, highThreshold / (eH + 1.0e-9f));

            // 3) Exciter: dalla zona 2-6 kHz nascono armoniche nuove, tenute solo sopra i 7 kHz
            const float src  = lp6k.processSample (ch, x) - lp2k.processSample (ch, x);
            const float harm = excHp2.processSample (ch, excHp1.processSample (ch, std::abs (src)));

            const float addM = soft (mAmt * (midStatic + midMaxGain * gM) * bandM);
            const float addH = soft (hAmt * ((highStatic + highMaxGain * gH) * bandH + exciterAmount * (0.4f + 0.6f * gH) * harm));

            d[i] = x + addM + addH;

            if (ch == 0)
            {
                sumMidGain  += mAmt * (midStatic  + midMaxGain  * gM);
                sumHighGain += hAmt * (highStatic + highMaxGain * gH);
                eHf = follow (eHf, std::abs (bandM + bandH), meterCoef, meterCoef);
                hfLevel = juce::jmax (hfLevel, eHf);
            }
        }

        envMid[(size_t) ch] = eM;
        envHigh[(size_t) ch] = eH;
        envHf[(size_t) ch] = eHf;
    }

    oversampler->processSamplesDown (mainBlock);

    trimGain.setGainDecibels (trimParam->load());
    juce::dsp::ProcessContextReplacing<float> ctx (mainBlock);
    trimGain.process (ctx);

    // Dati per la GUI: aria aggiunta sui dettagli (dB) e livello degli acuti
    const float avgM = (float) (sumMidGain  / (double) juce::jmax (1, n));
    const float avgH = (float) (sumHighGain / (double) juce::jmax (1, n));
    const bool active = hfLevel > 1.0e-4f; // sotto -80 dB: silenzio, niente aria da mostrare
    midBoostDb.store  (active ? juce::Decibels::gainToDecibels (1.0f + avgM) : 0.0f);
    highBoostDb.store (active ? juce::Decibels::gainToDecibels (1.0f + avgH) : 0.0f);
    hfLevelDb.store   (juce::Decibels::gainToDecibels (hfLevel, -100.0f));

    const int ns = buffer.getNumSamples();
    const float pL = buffer.getMagnitude (0, 0, ns);
    const float pR = numCh > 1 ? buffer.getMagnitude (1, 0, ns) : pL;
    if (pL > peakL.load()) peakL.store (pL);
    if (pR > peakR.load()) peakR.store (pR);
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

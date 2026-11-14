#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

class VRTXAirProcessor : public juce::AudioProcessor
{
public:
    VRTXAirProcessor();
    ~VRTXAirProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VRTX AIR"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Dati per la GUI: picchi in uscita e campioni per lo spettro
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    static constexpr int analyserSize = 16384;
    juce::AbstractFifo analyserFifo { analyserSize };
    std::vector<float> analyserBuffer = std::vector<float> ((size_t) analyserSize, 0.0f);

private:
    using SVF = juce::dsp::StateVariableTPTFilter<float>;

    std::atomic<float>* midParam  = nullptr;
    std::atomic<float>* highParam = nullptr;
    std::atomic<float>* trimParam = nullptr;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    static constexpr int osFactorLog2 = 2; // 4x

    SVF splitMid, splitHigh;   // passa-basso 3k e 10k per separare le bande
    SVF postMid, postHigh;     // pulizia armoniche generate

    std::array<float, 2> envMid  { 0.0f, 0.0f };
    std::array<float, 2> envHigh { 0.0f, 0.0f };
    float attackCoef = 0.0f, releaseCoef = 0.0f;

    juce::SmoothedValue<float> midSmooth, highSmooth;
    std::vector<float> midArr, highArr;

    juce::dsp::Gain<float> trimGain;
    int numChannelsPrepared = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VRTXAirProcessor)
};

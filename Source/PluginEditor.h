#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

//==============================================================================
// Font Inter incorporati nel plugin
struct VrtxFonts
{
    VrtxFonts();
    juce::Typeface::Ptr displayBold, displayLight, medium, semiBold;

    juce::Font make (const juce::Typeface::Ptr& tf, float height, float tracking = 0.0f) const;
};

//==============================================================================
class AtmosLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
private:
    void drawRing (juce::Graphics&, juce::Rectangle<float>, float pos, float a0, float a1, juce::Slider&);
    void drawTrim (juce::Graphics&, juce::Rectangle<float>, float pos, float a0, float a1, juce::Slider&);

    juce::SharedResourcePointer<VrtxFonts> fonts;
};

//==============================================================================
class SpectrumDisplay : public juce::Component
{
public:
    explicit SpectrumDisplay (VRTXAirProcessor&);
    void update();                       // chiamato dal timer dell'editor
    void paint (juce::Graphics&) override;

private:
    float xForFreq (float f) const;
    float yForCurveDb (float db) const;
    float airDb (float f, float mid, float high) const;

    VRTXAirProcessor& proc;
    juce::SharedResourcePointer<VrtxFonts> fonts;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::vector<float> history  = std::vector<float> ((size_t) fftSize, 0.0f);
    std::vector<float> fftData  = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> smoothDb = std::vector<float> ((size_t) fftSize / 2, -120.0f);
    int writePos = 0;

    float shownMid = 0.0f, shownHigh = 0.0f;
};

//==============================================================================
class OutputMeter : public juce::Component
{
public:
    explicit OutputMeter (VRTXAirProcessor& p) : proc (p) {}
    void update();
    void paint (juce::Graphics&) override;

private:
    static float levelToFraction (float db);
    VRTXAirProcessor& proc;
    juce::SharedResourcePointer<VrtxFonts> fonts;
    float levelL = 0.0f, levelR = 0.0f; // 0..1
};

//==============================================================================
class VRTXAirEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit VRTXAirEditor (VRTXAirProcessor&);
    ~VRTXAirEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider&, const juce::String& kind);

    VRTXAirProcessor& proc;
    juce::SharedResourcePointer<VrtxFonts> fonts;
    AtmosLookAndFeel lnf;
    juce::Image noiseTile;

    SpectrumDisplay display;
    OutputMeter meter;
    juce::Slider midKnob, highKnob, trimKnob;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> midAtt, highAtt, trimAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VRTXAirEditor)
};

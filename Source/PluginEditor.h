#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
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
class AirScope : public juce::Component
{
public:
    explicit AirScope (VRTXAirProcessor&);
    void update();                       // chiamato dal timer dell'editor (30 volte al secondo)
    void paint (juce::Graphics&) override;

private:
    static constexpr int historySize = 180;   // 6 secondi di storia
    VRTXAirProcessor& proc;
    juce::SharedResourcePointer<VrtxFonts> fonts;

    std::array<float, historySize> midHist {}, highHist {}, hfHist {};
    int head = 0;
    float midNow = 0.0f, highNow = 0.0f, hfNow = -100.0f;
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

    AirScope display;
    OutputMeter meter;
    juce::Slider midKnob, highKnob, trimKnob;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> midAtt, highAtt, trimAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VRTXAirEditor)
};

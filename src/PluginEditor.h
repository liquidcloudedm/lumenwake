#pragma once

#include "PluginProcessor.h"
#include "Renderer.h"

//==============================================================================
class LumenwakeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LumenwakeLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;

    static const juce::Colour background, panel, control, accent, text, dimText;
};

//==============================================================================
/** The area the visuals are drawn into. Reports mouse gestures to the editor. */
class VisualView : public juce::Component
{
public:
    std::function<void (juce::Point<float> deltaSinceDown, bool starting, bool ending)> onDrag;
    std::function<void (float wheelDelta)> onWheel;
    std::function<void()> onDoubleClick;

    void paint (juce::Graphics& g) override   { g.fillAll (juce::Colours::black); }

    void mouseDown (const juce::MouseEvent&) override          { if (onDrag) onDrag ({}, true, false); }
    void mouseDrag (const juce::MouseEvent& e) override        { if (onDrag) onDrag (e.getOffsetFromDragStart().toFloat(), false, false); }
    void mouseUp (const juce::MouseEvent& e) override          { if (onDrag) onDrag (e.getOffsetFromDragStart().toFloat(), false, true); }
    void mouseDoubleClick (const juce::MouseEvent&) override   { if (onDoubleClick) onDoubleClick(); }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (onWheel) onWheel (w.deltaY);
    }
};

//==============================================================================
class LumenwakeEditor : public juce::AudioProcessorEditor,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    explicit LumenwakeEditor (LumenwakeProcessor&);
    ~LumenwakeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attachment;
        bool modelOnly = false;
    };

    /** One effect: whether it is on, how much of it, and how long its cycle is. */
    struct FxRow
    {
        juce::TextButton on;
        juce::Label label;
        juce::Slider slider;
        juce::ComboBox rate;
        std::unique_ptr<ButtonAttachment> onAttachment;
        std::unique_ptr<SliderAttachment> amountAttachment;
        std::unique_ptr<ComboAttachment> rateAttachment;
    };

    void timerCallback() override;
    void setPage (int page);
    void updateFxSwitches();
    void addRow (const juce::String& parameterId, const juce::String& name, bool modelOnly = false);
    void addHeading (juce::Label&, const juce::String& text);
    void updateSceneControls();
    void updateRecordControls();
    void updateHint();
    void openFile (const juce::File&);
    void chooseFile (bool model);
    void setParameter (const juce::String& id, float value);
    void setPanelVisible (bool);
    int currentScene() const;

    LumenwakeProcessor& processor;
    LumenwakeLookAndFeel lookAndFeel;

    VisualView view;
    juce::Component panel;

    juce::Label title, sceneHeading, sourceHeading, reactHeading, lookHeading, recordHeading, statusLabel, hintLabel;
    juce::ComboBox sceneBox, imageModeBox, modelModeBox, fitBox, recordBox;
    juce::TextButton loadButton, resetButton, hideButton, recordButton, folderButton;
    juce::TooltipWindow tooltips { this, 600 };
    int shownRecordState = -1;
    double recordMessageUntil = 0.0;
    std::unique_ptr<ComboAttachment> sceneAttachment, imageModeAttachment, modelModeAttachment, fitAttachment;
    juce::OwnedArray<Row> rows;
    std::unique_ptr<juce::FileChooser> chooser;

    // The panel has two pages: the scene and its look, and the effects.
    juce::TextButton mainTab, fxTab;
    juce::Label syncHeading, effectsHeading, colorModeLabel;
    juce::ComboBox syncBox, colorModeBox;
    juce::Slider bpmSlider;
    std::unique_ptr<ComboAttachment> syncAttachment, colorModeAttachment;
    std::unique_ptr<SliderAttachment> bpmAttachment;
    juce::TextButton fxOnButton;
    std::unique_ptr<ButtonAttachment> fxOnAttachment;
    juce::OwnedArray<FxRow> fxRows;

    int shownScene = -1;
    float dragStartTurn = 0.0f, dragStartTilt = 0.0f;
    bool draggingModel = false, constructed = false, panelShotTaken = false;

    // Declared last so the GL context is torn down before the view it draws into.
    std::unique_ptr<VisualRenderer> renderer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenwakeEditor)
};

#include "PluginEditor.h"

//==============================================================================
const juce::Colour LumenwakeLookAndFeel::background { 0xff0e0d10 };
const juce::Colour LumenwakeLookAndFeel::panel      { 0xff17151b };
const juce::Colour LumenwakeLookAndFeel::control    { 0xff27242e };
const juce::Colour LumenwakeLookAndFeel::accent     { 0xffffb454 };
const juce::Colour LumenwakeLookAndFeel::text       { 0xffece7df };
const juce::Colour LumenwakeLookAndFeel::dimText    { 0xff8d8794 };

LumenwakeLookAndFeel::LumenwakeLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, dimText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
    setColour (juce::ComboBox::backgroundColourId, control);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, dimText);
    setColour (juce::PopupMenu::backgroundColourId, control);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::highlightedTextColourId, background);
    setColour (juce::TextButton::buttonColourId, control);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::TextEditor::backgroundColourId, control);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::CaretComponent::caretColourId, accent);
}

void LumenwakeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                             float, float, juce::Slider::SliderStyle, juce::Slider& slider)
{
    const float cy = (float) y + (float) height * 0.5f;
    const juce::Rectangle<float> track ((float) x, cy - 1.5f, (float) width, 3.0f);

    g.setColour (control);
    g.fillRoundedRectangle (track, 1.5f);

    // Bipolar parameters fill from the centre, the rest from the left.
    const bool bipolar = slider.getMinimum() < 0.0;
    const float origin = bipolar ? (float) x + (float) width * (float) slider.valueToProportionOfLength (0.0) : (float) x;

    g.setColour (accent.withAlpha (slider.isEnabled() ? 1.0f : 0.3f));
    g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (origin, sliderPos), cy - 1.5f,
                                                                        juce::jmax (origin, sliderPos), cy + 1.5f), 1.5f);

    const float r = slider.isMouseOverOrDragging() ? 6.0f : 5.0f;
    g.setColour (text);
    g.fillEllipse (sliderPos - r, cy - r, r * 2.0f, r * 2.0f);
}

void LumenwakeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down)
{
    // A button that is "on" is filled in: red for recording, amber for the page showing.
    const bool danger = (bool) button.getProperties().getWithDefault ("danger", false);
    const juce::Colour base = ! button.getToggleState() ? control
                            : danger ? juce::Colour (0xffb42a34) : control.interpolatedWith (accent, 0.45f);
    auto colour = base;

    if (down)             colour = button.getToggleState() ? base.darker (0.2f) : accent.withAlpha (0.55f);
    else if (highlighted) colour = base.brighter (0.15f);

    if (! button.isEnabled())
        colour = colour.withMultipliedAlpha (0.5f);

    g.setColour (colour);
    g.fillRoundedRectangle (button.getLocalBounds().toFloat(), 4.0f);
}

void LumenwakeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    g.setColour (box.isMouseOver (true) ? control.brighter (0.15f) : control);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f);

    juce::Path arrow;
    const float ax = (float) width - 15.0f, ay = (float) height * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (dimText);
    g.strokePath (arrow, juce::PathStrokeType (1.6f));
}

juce::Font LumenwakeLookAndFeel::getComboBoxFont (juce::ComboBox&)          { return juce::Font (juce::FontOptions (14.0f)); }
juce::Font LumenwakeLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return juce::Font (juce::FontOptions (13.5f)); }
juce::Font LumenwakeLookAndFeel::getLabelFont (juce::Label& label)          { return label.getFont(); }

//==============================================================================
namespace
{
    constexpr int panelWidth = 252;
    constexpr int minWidth = 520, minHeight = 680;
}

LumenwakeEditor::LumenwakeEditor (LumenwakeProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    addAndMakeVisible (view);
    addAndMakeVisible (panel);

    title.setText ("LUMENWAKE", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)).withExtraKerningFactor (0.14f));
    title.setBorderSize (juce::BorderSize<int> (0));
    title.setColour (juce::Label::textColourId, LumenwakeLookAndFeel::accent);
    panel.addAndMakeVisible (title);

    addHeading (sceneHeading, "SCENE");
    addHeading (sourceHeading, "SOURCE");
    addHeading (reactHeading, "REACT");
    addHeading (lookHeading, "LOOK");

    sceneBox.addItemList (LumenwakeProcessor::sceneNames, 1);
    imageModeBox.addItemList (LumenwakeProcessor::imageModeNames, 1);
    modelModeBox.addItemList (LumenwakeProcessor::modelModeNames, 1);
    fitBox.addItemList (LumenwakeProcessor::fitNames, 1);

    for (auto* box : { &sceneBox, &imageModeBox, &modelModeBox, &fitBox })
        panel.addAndMakeVisible (*box);

    sceneAttachment = std::make_unique<ComboAttachment> (processor.apvts, "scene", sceneBox);
    imageModeAttachment = std::make_unique<ComboAttachment> (processor.apvts, "imageMode", imageModeBox);
    modelModeAttachment = std::make_unique<ComboAttachment> (processor.apvts, "modelMode", modelModeBox);
    fitAttachment = std::make_unique<ComboAttachment> (processor.apvts, "imageFit", fitBox);

    panel.addAndMakeVisible (loadButton);
    panel.addAndMakeVisible (resetButton);
    panel.addAndMakeVisible (hideButton);
    hideButton.setButtonText ("Hide controls");
    hideButton.setTooltip ("Double-click the picture to bring the controls back");

    loadButton.onClick = [this] { chooseFile (currentScene() == sceneModel); };
    resetButton.onClick = [this]
    {
        if (currentScene() == sceneModel) processor.useBuiltinModel();
        else                              processor.clearImage();
    };
    hideButton.onClick = [this] { setPanelVisible (false); };

    //==========================================================================
    // Recording
    addHeading (recordHeading, "RECORD");

    recordBox.addItemList (LumenwakeProcessor::recordPresetNames, 1);
    recordBox.setSelectedItemIndex (processor.recordPreset, juce::dontSendNotification);
    recordBox.setTooltip ("Size and frame rate of the video");
    recordBox.onChange = [this] { processor.recordPreset = juce::jmax (0, recordBox.getSelectedItemIndex()); };
    panel.addAndMakeVisible (recordBox);

    recordButton.setButtonText ("Record");
    recordButton.getProperties().set ("danger", true);
    recordButton.setTooltip ("Records a video with a transparent background. It starts when playback starts and ends when playback stops.");
    recordButton.onClick = [this]
    {
        if (processor.recorder.getState() == VideoRecorder::idle)
            processor.armRecording();
        else
            processor.recorder.stop();

        updateRecordControls();
    };
    panel.addAndMakeVisible (recordButton);

    folderButton.setButtonText ("Folder");
    folderButton.setTooltip ("Show the recorded videos");
    folderButton.onClick = [this]
    {
        const auto last = processor.recorder.getLastFile();

        if (last.existsAsFile())
        {
            last.revealToUser();
            return;
        }

        const auto folder = LumenwakeProcessor::getRecordFolder();

        if (folder.createDirectory())
            folder.startAsProcess();
    };
    panel.addAndMakeVisible (folderButton);

    //==========================================================================
    // Pages
    mainTab.setButtonText ("Main");
    mainTab.setTooltip ("Scene, source and look");
    mainTab.onClick = [this] { setPage (0); };
    panel.addAndMakeVisible (mainTab);

    fxTab.setButtonText ("FX");
    fxTab.setTooltip ("Effects in time with the music");
    fxTab.onClick = [this] { setPage (1); };
    panel.addAndMakeVisible (fxTab);

    //==========================================================================
    // Effects page
    addHeading (syncHeading, "SYNC");
    addHeading (effectsHeading, "EFFECTS");

    syncBox.addItemList (LumenwakeProcessor::fxSyncNames, 1);
    syncBox.setTooltip ("Host tempo: effects follow the host's tempo and bar position. Free: they run at the tempo set here.");
    panel.addAndMakeVisible (syncBox);
    syncAttachment = std::make_unique<ComboAttachment> (processor.apvts, "fxSync", syncBox);

    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    bpmSlider.setScrollWheelEnabled (false);
    bpmSlider.setTooltip ("Tempo in BPM, used when Sync is Free or the host reports no tempo");
    panel.addAndMakeVisible (bpmSlider);
    bpmAttachment = std::make_unique<SliderAttachment> (processor.apvts, "fxBpm", bpmSlider);
    bpmSlider.setNumDecimalPlacesToDisplay (1);
    bpmSlider.updateText();

    static const char* const fxTips[numFx] =
    {
        "Cuts the picture in and out",
        "Full brightness at the start of each cycle, fading until the next",
        "Moves the picture through the colours. White and grey are tinted; colours have their hue turned",
        "Kicks the picture larger at the start of each cycle",
        "Jolts the picture at the start of each cycle",
        "Rocks the picture left and right",
        "Pulls red and blue apart",
        "Slices of the picture jump sideways",
        "Pumps the glow around bright parts",
        "Breaks the picture into blocks that resolve"
    };

    for (int i = 0; i < numFx; ++i)
    {
        auto* row = fxRows.add (new FxRow());

        row->label.setText (LumenwakeProcessor::fxNames[i], juce::dontSendNotification);
        row->label.setFont (juce::Font (juce::FontOptions (13.0f)));
        row->label.setMinimumHorizontalScale (1.0f);
        row->label.setTooltip (fxTips[i]);
        panel.addAndMakeVisible (row->label);

        row->slider.setSliderStyle (juce::Slider::LinearHorizontal);
        row->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        row->slider.setScrollWheelEnabled (false);
        row->slider.setTooltip (fxTips[i]);
        panel.addAndMakeVisible (row->slider);

        row->rate.addItemList (LumenwakeProcessor::fxRateNames, 1);
        row->rate.setTooltip ("Length of one cycle");
        panel.addAndMakeVisible (row->rate);

        row->amountAttachment = std::make_unique<SliderAttachment> (processor.apvts, LumenwakeProcessor::fxIds[i], row->slider);
        row->rateAttachment = std::make_unique<ComboAttachment> (processor.apvts, LumenwakeProcessor::fxIds[i] + "Rate", row->rate);
    }

    colorModeLabel.setText ("Color mode", juce::dontSendNotification);
    colorModeLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    colorModeLabel.setColour (juce::Label::textColourId, LumenwakeLookAndFeel::dimText);
    panel.addAndMakeVisible (colorModeLabel);

    colorModeBox.addItemList (LumenwakeProcessor::fxColorModeNames, 1);
    colorModeBox.setTooltip ("Cycle: glide through the colours over each cycle. Step: jump to a new colour at the start of each cycle.");
    panel.addAndMakeVisible (colorModeBox);
    colorModeAttachment = std::make_unique<ComboAttachment> (processor.apvts, "fxColorMode", colorModeBox);

    addRow ("sensitivity", "Sensitivity");
    addRow ("smoothing", "Smoothing");
    addRow ("amount", "Intensity");
    addRow ("speed", "Speed");
    addRow ("zoom", "Zoom");
    addRow ("trails", "Trails");
    addRow ("glow", "Glow");
    addRow ("hue", "Hue");
    addRow ("saturation", "Saturation");
    addRow ("brightness", "Brightness");
    addRow ("turn", "Turn", true);
    addRow ("tilt", "Tilt", true);
    addRow ("spin", "Spin", true);

    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setColour (juce::Label::textColourId, LumenwakeLookAndFeel::dimText);
    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setMinimumHorizontalScale (1.0f);
    panel.addAndMakeVisible (statusLabel);

    hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    hintLabel.setColour (juce::Label::textColourId, LumenwakeLookAndFeel::dimText);
    hintLabel.setJustificationType (juce::Justification::topLeft);
    hintLabel.setMinimumHorizontalScale (1.0f);
    panel.addAndMakeVisible (hintLabel);

    //==========================================================================
    view.onDoubleClick = [this] { setPanelVisible (! processor.panelVisible); };

    view.onWheel = [this] (float delta)
    {
        const float zoom = processor.apvts.getRawParameterValue ("zoom")->load();
        setParameter ("zoom", zoom * std::exp (delta * 0.6f));
    };

    view.onDrag = [this] (juce::Point<float> delta, bool starting, bool ending)
    {
        // Dragging turns the model; the angle you leave it at is the angle it keeps.
        auto* turn = processor.apvts.getParameter ("turn");
        auto* tilt = processor.apvts.getParameter ("tilt");

        if (starting)
        {
            if (currentScene() != sceneModel)
                return;

            draggingModel = true;
            dragStartTurn = processor.apvts.getRawParameterValue ("turn")->load();
            dragStartTilt = processor.apvts.getRawParameterValue ("tilt")->load();
            turn->beginChangeGesture();
            tilt->beginChangeGesture();
            return;
        }

        if (! draggingModel)
            return;

        float newTurn = dragStartTurn + delta.x * 0.4f;
        newTurn = std::fmod (newTurn + 180.0f, 360.0f);
        if (newTurn < 0.0f) newTurn += 360.0f;
        newTurn -= 180.0f;

        setParameter ("turn", newTurn);
        setParameter ("tilt", juce::jlimit (-180.0f, 180.0f, dragStartTilt + delta.y * 0.4f));

        if (ending)
        {
            draggingModel = false;
            turn->endChangeGesture();
            tilt->endChangeGesture();
        }
    };

    renderer = std::make_unique<VisualRenderer> (processor, view);

    // Setting the limits resizes the window once, so take the saved size first.
    int savedWidth = processor.editorWidth, savedHeight = processor.editorHeight;

    if (juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_DUMP", {}).isNotEmpty())
    {
        savedWidth = 1060;      // self-test renders at a fixed size
        savedHeight = 640;
        processor.panelVisible = true;
    }

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, 7680, 4320);
    setSize (juce::jmax (minWidth, savedWidth), juce::jmax (minHeight, savedHeight));
    constructed = true;
    resized();

    // Self-test: open on the effects page.
    if (juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_PAGE", {}) == "fx")
        processor.panelPage = 1;

    updateSceneControls();
    updateRecordControls();
    startTimerHz (8);

    // Self-test: arm a take as soon as the window opens.
    if (juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_AUTOREC", {}).isNotEmpty())
        processor.armRecording();
}

LumenwakeEditor::~LumenwakeEditor()
{
    stopTimer();

    // Nothing is drawn once the window has gone, so a take ends with it.
    processor.recorder.stop();
    renderer.reset();
    setLookAndFeel (nullptr);
}

//==============================================================================
void LumenwakeEditor::addHeading (juce::Label& label, const juce::String& textToShow)
{
    label.setText (textToShow, juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.14f));
    label.setColour (juce::Label::textColourId, LumenwakeLookAndFeel::dimText);
    panel.addAndMakeVisible (label);
}

void LumenwakeEditor::addRow (const juce::String& parameterId, const juce::String& name, bool modelOnly)
{
    auto* row = rows.add (new Row());
    row->modelOnly = modelOnly;

    row->label.setText (name, juce::dontSendNotification);
    row->label.setFont (juce::Font (juce::FontOptions (13.0f)));
    row->label.setMinimumHorizontalScale (1.0f);
    panel.addAndMakeVisible (row->label);

    row->slider.setSliderStyle (juce::Slider::LinearHorizontal);
    row->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    row->slider.setScrollWheelEnabled (false);
    panel.addAndMakeVisible (row->slider);

    row->attachment = std::make_unique<SliderAttachment> (processor.apvts, parameterId, row->slider);
    row->slider.setNumDecimalPlacesToDisplay (row->slider.getMaximum() > 100.0 ? 0 : 2);
    row->slider.updateText();
}

int LumenwakeEditor::currentScene() const
{
    return juce::roundToInt (processor.apvts.getRawParameterValue ("scene")->load());
}

void LumenwakeEditor::setParameter (const juce::String& id, float value)
{
    if (auto* param = processor.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void LumenwakeEditor::setPanelVisible (bool shouldBeVisible)
{
    processor.panelVisible = shouldBeVisible;
    resized();
}

void LumenwakeEditor::updateSceneControls()
{
    const int scene = currentScene();
    shownScene = scene;

    const bool isImage = scene == sceneImage, isModel = scene == sceneModel;
    const bool main = processor.panelPage == 0;

    mainTab.setToggleState (main, juce::dontSendNotification);
    fxTab.setToggleState (! main, juce::dontSendNotification);

    // Main page
    sceneHeading.setVisible (main);
    sceneBox.setVisible (main);
    reactHeading.setVisible (main);
    lookHeading.setVisible (main);
    hintLabel.setVisible (main);
    sourceHeading.setVisible (main && (isImage || isModel));
    imageModeBox.setVisible (main && isImage);
    fitBox.setVisible (main && isImage);
    modelModeBox.setVisible (main && isModel);
    loadButton.setVisible (main && (isImage || isModel));
    resetButton.setVisible (main && (isImage || isModel));
    loadButton.setButtonText (isModel ? "Load model..." : "Load image...");
    resetButton.setButtonText (isModel ? "Built-in" : "Placeholder");

    for (auto* row : rows)
    {
        row->label.setVisible (main && (! row->modelOnly || isModel));
        row->slider.setVisible (main && (! row->modelOnly || isModel));
    }

    // Effects page
    syncHeading.setVisible (! main);
    effectsHeading.setVisible (! main);
    syncBox.setVisible (! main);
    bpmSlider.setVisible (! main);
    colorModeLabel.setVisible (! main);
    colorModeBox.setVisible (! main);

    for (auto* row : fxRows)
    {
        row->label.setVisible (! main);
        row->slider.setVisible (! main);
        row->rate.setVisible (! main);
    }

    updateHint();
    resized();
}

void LumenwakeEditor::setPage (int page)
{
    processor.panelPage = page;
    updateSceneControls();
}

void LumenwakeEditor::updateHint()
{
    const int scene = currentScene();
    juce::String hint;

    if (processor.recorder.getState() != VideoRecorder::idle)
        hint = scene == sceneModel ? "The checks are see-through in the video. The model stays solid."
                                   : "The checks are see-through in the video: black is transparent.";
    else if (scene == sceneImage)
        hint = "Drop a PNG, JPG, GIF or WebP on the window.";
    else if (scene == sceneModel)
        hint = "Drop a GLB, glTF, FBX, OBJ, STL or PLY. Drag the model to turn it.";

    if (hint != hintLabel.getText())
        hintLabel.setText (hint, juce::dontSendNotification);
}

void LumenwakeEditor::updateRecordControls()
{
    const int state = (int) processor.recorder.getState();

    if (state == shownRecordState)
        return;

    // A take that has just ended leaves a message worth reading for a while.
    if (state == VideoRecorder::idle && shownRecordState > VideoRecorder::idle)
        recordMessageUntil = juce::Time::getMillisecondCounterHiRes() + 20000.0;

    shownRecordState = state;

    recordButton.setButtonText (state == VideoRecorder::armed     ? "Cancel"
                              : state == VideoRecorder::recording ? "Stop"
                              : state == VideoRecorder::finishing ? "Saving" : "Record");
    recordButton.setToggleState (state == VideoRecorder::armed || state == VideoRecorder::recording, juce::dontSendNotification);
    recordButton.setEnabled (state != VideoRecorder::finishing);
    recordBox.setEnabled (state == VideoRecorder::idle);
    updateHint();
}

//==============================================================================
void LumenwakeEditor::paint (juce::Graphics& g)
{
    g.fillAll (LumenwakeLookAndFeel::background);

    if (panel.isVisible())
    {
        g.setColour (LumenwakeLookAndFeel::panel);
        g.fillRect (panel.getBounds());
    }
}

void LumenwakeEditor::resized()
{
    if (constructed)
    {
        processor.editorWidth = getWidth();
        processor.editorHeight = getHeight();
    }

    auto area = getLocalBounds();
    const bool showPanel = processor.panelVisible;
    panel.setVisible (showPanel);

    if (showPanel)
        panel.setBounds (area.removeFromRight (panelWidth));

    view.setBounds (area);

    if (renderer != nullptr)
        renderer->setViewSize (view.getWidth(), view.getHeight());

    if (! showPanel)
        return;

    auto r = panel.getLocalBounds().reduced (14, 12);
    const int rowHeight = 23, boxHeight = 26, gap = 5;

    auto top = r.removeFromTop (26);
    fxTab.setBounds (top.removeFromRight (42).reduced (0, 2));
    top.removeFromRight (4);
    mainTab.setBounds (top.removeFromRight (42).reduced (0, 2));
    title.setBounds (top);
    r.removeFromTop (8);

    if (processor.panelPage == 1)
    {
        syncHeading.setBounds (r.removeFromTop (15));
        r.removeFromTop (3);

        auto syncRow = r.removeFromTop (boxHeight);
        syncBox.setBounds (syncRow.removeFromLeft (108));
        syncRow.removeFromLeft (gap);
        bpmSlider.setBounds (syncRow);
        r.removeFromTop (12);

        effectsHeading.setBounds (r.removeFromTop (15));
        r.removeFromTop (3);

        for (int i = 0; i < fxRows.size(); ++i)
        {
            auto line = r.removeFromTop (26);
            fxRows[i]->rate.setBounds (line.removeFromRight (66).reduced (0, 2));
            line.removeFromRight (gap);
            fxRows[i]->label.setBounds (line.removeFromLeft (86));
            fxRows[i]->slider.setBounds (line);

            if (i == fxColor)
            {
                auto modeLine = r.removeFromTop (26);
                colorModeBox.setBounds (modeLine.removeFromRight (66).reduced (0, 2));
                colorModeLabel.setBounds (modeLine);
            }
        }
    }

    sceneHeading.setBounds (r.removeFromTop (processor.panelPage == 0 ? 15 : 0));
    r.removeFromTop (processor.panelPage == 0 ? 3 : 0);
    sceneBox.setBounds (r.removeFromTop (processor.panelPage == 0 ? boxHeight : 0));
    r.removeFromTop (processor.panelPage == 0 ? 12 : 0);

    if (sourceHeading.isVisible())
    {
        sourceHeading.setBounds (r.removeFromTop (15));
        r.removeFromTop (3);

        auto modeRow = r.removeFromTop (boxHeight);

        if (fitBox.isVisible())
        {
            fitBox.setBounds (modeRow.removeFromRight (68));
            modeRow.removeFromRight (gap);
            imageModeBox.setBounds (modeRow);
        }
        else
        {
            modelModeBox.setBounds (modeRow);
        }

        r.removeFromTop (gap);
        auto buttons = r.removeFromTop (boxHeight);
        resetButton.setBounds (buttons.removeFromRight (86));
        buttons.removeFromRight (gap);
        loadButton.setBounds (buttons);
        r.removeFromTop (12);
    }

    auto layoutRow = [&] (Row& row)
    {
        if (! row.slider.isVisible())
            return;

        auto line = r.removeFromTop (rowHeight);
        row.label.setBounds (line.removeFromLeft (76));
        row.slider.setBounds (line);
    };

    if (processor.panelPage == 0)
    {
        reactHeading.setBounds (r.removeFromTop (15));
        r.removeFromTop (2);
        for (int i = 0; i < 4; ++i) layoutRow (*rows[i]);
        r.removeFromTop (10);

        lookHeading.setBounds (r.removeFromTop (15));
        r.removeFromTop (2);
        for (int i = 4; i < rows.size(); ++i) layoutRow (*rows[i]);
        r.removeFromTop (10);
    }

    hideButton.setBounds (r.removeFromBottom (24));
    r.removeFromBottom (6);
    statusLabel.setBounds (r.removeFromBottom (34));
    r.removeFromBottom (6);

    auto recordRow = r.removeFromBottom (boxHeight);
    recordButton.setBounds (recordRow.removeFromRight (62));
    recordRow.removeFromRight (gap);
    folderButton.setBounds (recordRow.removeFromRight (54));
    recordRow.removeFromRight (gap);
    recordBox.setBounds (recordRow);
    r.removeFromBottom (3);
    recordHeading.setBounds (r.removeFromBottom (15));
    r.removeFromBottom (6);

    hintLabel.setBounds (r.removeFromTop (juce::jmin (r.getHeight(), 48)));
}

//==============================================================================
void LumenwakeEditor::timerCallback()
{
    if (currentScene() != shownScene)
        updateSceneControls();

    updateRecordControls();

    juce::String status;
    const int scene = currentScene();
    const auto& recorder = processor.recorder;
    const auto recordState = recorder.getState();
    bool showFps = true;

    if (renderer != nullptr && renderer->getShaderError().isNotEmpty())
    {
        status = renderer->getShaderError();
    }
    else if (recordState == VideoRecorder::armed)
    {
        status = "Armed. Starts on play.";
    }
    else if (recordState == VideoRecorder::recording)
    {
        const int seconds = (int) recorder.getSecondsRecorded();
        status = "REC " + juce::String (seconds / 60) + ":" + juce::String (seconds % 60).paddedLeft ('0', 2)
               + "   " + juce::File::descriptionOfSizeInBytes (recorder.getBytesWritten());

        if (recorder.getFramesMissed() > 0)
            status << "   " << recorder.getFramesMissed() << " held";
    }
    else if (recordState == VideoRecorder::finishing)
    {
        status = "Saving...";
    }
    else if (recorder.getMessage().isNotEmpty() && juce::Time::getMillisecondCounterHiRes() < recordMessageUntil)
    {
        status = recorder.getMessage();
        showFps = false;
    }
    else if (processor.getStatus().isNotEmpty())
        status = processor.getStatus();
    else if (scene == sceneImage)
        status = processor.getImageName();
    else if (scene == sceneModel)
        status = processor.getModelName();

    if (renderer != nullptr && showFps)
        status << "\n" << juce::roundToInt (renderer->getFps()) << " fps";

    // Self-test: a picture of the control panel two seconds into a take.
    if (recordState == VideoRecorder::recording && recorder.getSecondsRecorded() > 2.0 && ! panelShotTaken)
    {
        panelShotTaken = true;
        const auto path = juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_PANEL_SHOT", {});

        if (path.isNotEmpty())
        {
            if (auto stream = juce::File (path).createOutputStream())
            {
                stream->setPosition (0);
                stream->truncate();
                juce::PNGImageFormat png;
                png.writeImageToStream (panel.createComponentSnapshot (panel.getLocalBounds()), *stream);
            }
        }
    }

    if (status != statusLabel.getText())
        statusLabel.setText (status, juce::dontSendNotification);
}

void LumenwakeEditor::chooseFile (bool model)
{
    chooser = std::make_unique<juce::FileChooser> (model ? "Choose a 3D model" : "Choose an image",
                                                   juce::File(),
                                                   model ? ModelLoader::getWildcard() : LumenwakeProcessor::getImageWildcard());

    juce::Component::SafePointer<LumenwakeEditor> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();

                              if (safe != nullptr && file.existsAsFile())
                                  safe->openFile (file);
                          });
}

void LumenwakeEditor::openFile (const juce::File& file)
{
    if (ModelLoader::isSupported (file))
    {
        processor.loadModel (file);
        setParameter ("scene", (float) sceneModel);
    }
    else if (LumenwakeProcessor::isSupportedImage (file))
    {
        processor.loadImage (file);
        setParameter ("scene", (float) sceneImage);
    }
}

bool LumenwakeEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& path : files)
    {
        const juce::File file (path);

        if (ModelLoader::isSupported (file) || LumenwakeProcessor::isSupportedImage (file))
            return true;
    }

    return false;
}

void LumenwakeEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& path : files)
    {
        const juce::File file (path);

        if (ModelLoader::isSupported (file) || LumenwakeProcessor::isSupportedImage (file))
        {
            openFile (file);
            return;
        }
    }
}

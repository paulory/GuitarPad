#pragma once

#include <JuceHeader.h>

#include "../Core/MappingEngine.h"
#include "../Core/NoteDetector.h"
#include "../Platform/KeySender.h"

class MainComponent;

// Uma linha da tabela: nota -> tecla -> modo
class MappingRow : public juce::Component
{
public:
    MappingRow (MainComponent& owner);
    void setMapping (const gp::Mapping& m);
    gp::Mapping getMapping() const;
    void setLearning (bool on);
    void setActive (bool keyDown);
    void resized() override;
    void paint (juce::Graphics& g) override;

    juce::TextButton learnButton { "Aprender" };

private:
    MainComponent& owner;
    juce::ComboBox noteBox, keyBox, modeBox;
    juce::TextButton removeButton { "X" };
    bool learning = false, active = false;
};

// Mostra nota detectada, frequencia e nivel com a linha do gate
class TunerDisplay : public juce::Component
{
public:
    void update (int note, float freq, float levelDb, float gateDb, bool holdOn);
    void paint (juce::Graphics& g) override;

private:
    int note = -1;
    float freq = 0, level = -120, gate = -48;
    bool hold = true;
};

class MainComponent : public juce::AudioAppComponent, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // chamados pelas linhas
    void rowChanged();
    void removeRow (MappingRow* row);
    void startLearning (MappingRow* row);

    static void fillNoteBox (juce::ComboBox& box, bool withNone);

private:
    void timerCallback() override;
    void addRow (const gp::Mapping& m);
    void pushSettingsToEngine();
    void layoutRows();
    void stopLearning();
    void refreshHoldButton();

    juce::var settingsToVar (bool includeAudio) const;
    void settingsFromVar (const juce::var& v, bool includeAudio);
    juce::File settingsFile() const;
    void saveSettings();
    void loadSettings();
    void loadDefaults();
    void showAudioSettings();
    void savePreset();
    void loadPreset();

    gp::KeySender sender;
    gp::MappingEngine engine { sender };
    gp::NoteDetector detector;
    std::vector<float> mono;

    juce::Label title, status, rowsHeader, gateLabel, tapLabel, commandLabel;
    juce::TextButton audioButton { "Audio..." }, panicButton { "Soltar tudo" };
    juce::TextButton holdButton, addRowButton { "+ Adicionar nota" };
    juce::TextButton savePresetButton { "Salvar preset" }, loadPresetButton { "Abrir preset" };
    juce::TextButton commandLearnButton { "Aprender" };
    juce::ToggleButton outputToggle { "Ativo" }, fastToggle { "Modo rapido" };
    juce::Slider gateSlider, tapSlider;
    juce::ComboBox commandBox;
    TunerDisplay tuner;
    juce::SharedResourcePointer<juce::TooltipWindow> tooltipWindow;

    juce::Viewport viewport;
    juce::Component rowsHolder;
    juce::OwnedArray<MappingRow> rows;

    MappingRow* learningRow = nullptr;
    bool learningCommand = false, outputWasEnabled = true;
    uint32_t lastNoteOnCounter = 0;
    bool hotkeyWasDown = false, loading = false;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String audioStateXml;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

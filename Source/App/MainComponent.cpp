#include "MainComponent.h"

namespace
{
juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

namespace C
{
const juce::Colour bg      { 0xff15161a };
const juce::Colour panel   { 0xff1f2127 };
const juce::Colour line    { 0xff2e313a };
const juce::Colour text    { 0xffe8e8ec };
const juce::Colour dim     { 0xff8a8d98 };
const juce::Colour accent  { 0xffff8a3d };
const juce::Colour on      { 0xff3ccf7a };
const juce::Colour off     { 0xffe0505a };
}

juce::String noteText (int midi)
{
    char buf[8];
    gp::noteName (midi, buf, (int) sizeof buf);
    return buf;
}

// posicao mais baixa no braco (afinacao padrao)
juce::String fretHint (int midi)
{
    const int open[] = { 64, 59, 55, 50, 45, 40 }; // cordas 1..6
    int bestString = -1, bestFret = 99;
    for (int s = 0; s < 6; ++s)
    {
        const int fret = midi - open[s];
        if (fret >= 0 && fret < bestFret) { bestFret = fret; bestString = s; }
    }
    if (bestString < 0 || bestFret > 24)
        return {};
    return T ("corda ") + juce::String (bestString + 1) + T (", casa ") + juce::String (bestFret);
}

juce::String keyDisplayName (int k)
{
    const juce::String n (gp::kKeys[k].name);
    if (n == "Espaco") return T ("Espaço");
    if (n == "Esquerda") return T ("Seta esquerda");
    if (n == "Direita") return T ("Seta direita");
    if (n == "Cima") return T ("Seta cima");
    if (n == "Baixo") return T ("Seta baixo");
    return n;
}

constexpr int kRowHeight = 34;
constexpr int kMinNote = 38, kMaxNote = 90;

// painel de audio que avisa quando fecha (para salvar o dispositivo escolhido)
struct AudioSettingsPanel : public juce::Component
{
    AudioSettingsPanel (juce::AudioDeviceManager& dm, std::function<void()> closed)
        : selector (dm, 1, 2, 0, 0, false, false, false, false), onClose (std::move (closed))
    {
        addAndMakeVisible (selector);
        setSize (520, 360);
    }
    ~AudioSettingsPanel() override { if (onClose) onClose(); }
    void resized() override { selector.setBounds (getLocalBounds().reduced (8)); }

    juce::AudioDeviceSelectorComponent selector;
    std::function<void()> onClose;
};
} // namespace

// =====================================================================
// MappingRow

MappingRow::MappingRow (MainComponent& o) : owner (o)
{
    MainComponent::fillNoteBox (noteBox, false);
    for (int k = 0; k < gp::kNumKeys; ++k)
        keyBox.addItem (keyDisplayName (k), k + 1);
    modeBox.addItem (T ("Toque"), 1);
    modeBox.addItem (T ("Sustentação"), 2);
    modeBox.addItem (T ("Trava"), 3);

    for (auto* b : { &noteBox, &keyBox, &modeBox })
    {
        addAndMakeVisible (b);
        b->onChange = [this] { owner.rowChanged(); };
    }
    addAndMakeVisible (learnButton);
    addAndMakeVisible (removeButton);
    learnButton.onClick = [this] { owner.startLearning (this); };
    removeButton.onClick = [this] { owner.removeRow (this); };
    learnButton.setTooltip (T ("Clique e toque a nota na guitarra"));
    modeBox.setTooltip (T ("Toque: aperta e solta. Sustentação: segura enquanto a nota soa. Trava: 1º toque aperta, 2º solta."));
}

void MappingRow::setMapping (const gp::Mapping& m)
{
    noteBox.setSelectedId (juce::jlimit (kMinNote, kMaxNote, m.note) + 1, juce::dontSendNotification);
    keyBox.setSelectedId (juce::jlimit (0, gp::kNumKeys - 1, m.key) + 1, juce::dontSendNotification);
    modeBox.setSelectedId ((int) m.mode + 1, juce::dontSendNotification);
}

gp::Mapping MappingRow::getMapping() const
{
    gp::Mapping m;
    m.note = noteBox.getSelectedId() - 1;
    m.key = keyBox.getSelectedId() - 1;
    m.mode = (gp::KeyMode) juce::jlimit (0, 2, modeBox.getSelectedId() - 1);
    return m;
}

void MappingRow::setLearning (bool on)
{
    learning = on;
    learnButton.setButtonText (on ? T ("Toque...") : T ("Aprender"));
    repaint();
}

void MappingRow::setActive (bool keyDown)
{
    if (active != keyDown) { active = keyDown; repaint(); }
}

void MappingRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (learning ? C::accent.withAlpha (0.18f) : C::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (active ? C::on : C::line);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ 14.0f, r.getCentreY() }));
}

void MappingRow::resized()
{
    auto r = getLocalBounds().reduced (4);
    r.removeFromLeft (24);
    removeButton.setBounds (r.removeFromRight (30));
    r.removeFromRight (6);
    const int w = r.getWidth();
    noteBox.setBounds (r.removeFromLeft ((int) (w * 0.36f)));
    r.removeFromLeft (4);
    learnButton.setBounds (r.removeFromLeft (84));
    r.removeFromLeft (8);
    keyBox.setBounds (r.removeFromLeft ((r.getWidth() - 8) / 2));
    r.removeFromLeft (8);
    modeBox.setBounds (r);
}

// =====================================================================
// TunerDisplay

void TunerDisplay::update (int n, float f, float lvl, float gateDb, bool holdOn)
{
    if (n != note || std::abs (f - freq) > 0.5f || std::abs (lvl - level) > 0.5f || std::abs (gateDb - gate) > 0.01f || holdOn != hold)
    {
        note = n; freq = f; level = lvl; gate = gateDb; hold = holdOn;
        repaint();
    }
}

void TunerDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (C::panel);
    g.fillRoundedRectangle (r, 10.0f);
    r.reduce (16, 12);

    auto meter = r.removeFromBottom (14);
    auto left = r.removeFromLeft (r.getWidth() * 0.45f);

    g.setColour (note >= 0 ? C::text : C::dim);
    g.setFont (juce::Font (juce::FontOptions (54.0f, juce::Font::bold)));
    g.drawText (note >= 0 ? noteText (note) : juce::String ("--"), left, juce::Justification::centredLeft);

    g.setFont (juce::Font (juce::FontOptions (15.0f)));
    g.setColour (C::dim);
    auto info = r.reduced (0, 6);
    g.drawText (freq > 0 ? juce::String (freq, 1) + " Hz" : juce::String (T ("sem nota")), info.removeFromTop (info.getHeight() / 2), juce::Justification::centredLeft);
    g.drawText (note >= 0 ? fretHint (note) : juce::String(), info, juce::Justification::centredLeft);

    // medidor de nivel: -90 dB a 0 dB
    auto toX = [&] (float db) { return meter.getX() + meter.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f); };
    g.setColour (C::bg);
    g.fillRoundedRectangle (meter, 4.0f);
    g.setColour (level > gate ? C::on : C::dim);
    g.fillRoundedRectangle (meter.withRight (toX (level)), 4.0f);
    g.setColour (C::accent);
    g.fillRect (juce::Rectangle<float> (toX (gate) - 1.0f, meter.getY() - 3.0f, 2.0f, meter.getHeight() + 6.0f));
}

// =====================================================================
// MainComponent

MainComponent::MainComponent()
{
    setOpaque (true);

    title.setText ("GuitarPad", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, C::text);
    addAndMakeVisible (title);

    for (auto* l : { &status, &rowsHeader, &gateLabel, &tapLabel, &commandLabel })
    {
        l->setColour (juce::Label::textColourId, C::dim);
        addAndMakeVisible (l);
    }
    rowsHeader.setText (T ("Nota  >  Tecla  >  Modo"), juce::dontSendNotification);
    gateLabel.setText (T ("Sensibilidade (gate)"), juce::dontSendNotification);
    tapLabel.setText (T ("Duração do toque"), juce::dontSendNotification);
    commandLabel.setText (T ("Nota que liga/desliga a sustentação"), juce::dontSendNotification);

    audioButton.setButtonText (T ("Áudio..."));
    for (auto* b : { &audioButton, &panicButton, &holdButton, &addRowButton, &savePresetButton, &loadPresetButton, &commandLearnButton })
        addAndMakeVisible (b);

    holdButton.setClickingTogglesState (false);
    holdButton.onClick = [this] { engine.toggleHold(); };
    panicButton.onClick = [this] { engine.panic(); };
    audioButton.onClick = [this] { showAudioSettings(); };
    addRowButton.onClick = [this]
    {
        gp::Mapping m;
        if (! rows.isEmpty()) m.note = juce::jmin (kMaxNote, rows.getLast()->getMapping().note + 2);
        m.key = gp::keyIndexFromName ("Z");
        addRow (m);
        rowChanged();
        viewport.setViewPosition (0, rowsHolder.getHeight());
    };
    savePresetButton.onClick = [this] { savePreset(); };
    loadPresetButton.onClick = [this] { loadPreset(); };
    commandLearnButton.onClick = [this]
    {
        stopLearning();
        learningCommand = true;
        commandLearnButton.setButtonText (T ("Toque..."));
        outputWasEnabled = engine.isOutputEnabled();
        engine.setOutputEnabled (false);
        lastNoteOnCounter = detector.getNoteOnCounter();
    };

    addAndMakeVisible (outputToggle);
    outputToggle.setToggleState (true, juce::dontSendNotification);
    outputToggle.setTooltip (T ("Desligado: o app escuta a guitarra mas não manda teclas"));
    outputToggle.onClick = [this] { engine.setOutputEnabled (outputToggle.getToggleState()); saveSettings(); };

    fastToggle.setButtonText (T ("Modo rápido"));
    addAndMakeVisible (fastToggle);
    fastToggle.setTooltip (T ("Confirma a nota mais rápido (uns 5 ms), com um pouco mais de risco de errar a nota"));
    fastToggle.onClick = [this] { detector.setFastMode (fastToggle.getToggleState()); saveSettings(); };

    gateSlider.setRange (-80.0, -10.0, 1.0);
    gateSlider.setTextValueSuffix (" dB");
    gateSlider.setTooltip (T ("Abaixo desse nível o som é ignorado. Suba se o ruído dispara teclas, desça se notas fracas não disparam."));
    gateSlider.onValueChange = [this] { detector.setGateDb ((float) gateSlider.getValue()); saveSettings(); };
    tapSlider.setRange (10.0, 200.0, 1.0);
    tapSlider.setTextValueSuffix (" ms");
    tapSlider.setTooltip (T ("Quanto tempo o modo Toque segura a tecla. 30 a 60 ms costuma funcionar em jogos a 60 fps."));
    tapSlider.onValueChange = [this] { rowChanged(); };
    for (auto* s : { &gateSlider, &tapSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
        addAndMakeVisible (s);
    }

    fillNoteBox (commandBox, true);
    commandBox.onChange = [this] { rowChanged(); };
    addAndMakeVisible (commandBox);

    addAndMakeVisible (tuner);
    viewport.setViewedComponent (&rowsHolder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    const bool perm = gp::KeySender::ensurePermission();
    status.setText (T ("F12 liga/desliga a sustentação mesmo com o jogo em foco.  Saída: ")
                        + gp::KeySender::platformName()
                        + (perm ? juce::String() : T ("  (libere o app em Ajustes > Privacidade > Acessibilidade)")),
                    juce::dontSendNotification);

    loadSettings();
    engine.start();

    std::unique_ptr<juce::XmlElement> xml;
    if (audioStateXml.isNotEmpty())
        xml = juce::parseXML (audioStateXml);
    setAudioChannels (2, 0, xml.get());

    setSize (860, 720);
    startTimerHz (60);
}

MainComponent::~MainComponent()
{
    stopTimer();
    saveSettings();
    shutdownAudio();
    engine.stop();
}

void MainComponent::fillNoteBox (juce::ComboBox& box, bool withNone)
{
    if (withNone)
        box.addItem (T ("Nenhuma"), 1000);
    for (int n = kMinNote; n <= kMaxNote; ++n)
        box.addItem (noteText (n) + "   (" + fretHint (n) + ")", n + 1);
}

// ---------- audio ----------

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    mono.assign ((size_t) juce::jmax (512, samplesPerBlockExpected), 0.0f);
    detector.prepare (sampleRate);
    detector.setListener (&engine);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    auto* buffer = info.buffer;
    const int chans = buffer->getNumChannels();
    int done = 0;
    while (done < info.numSamples)
    {
        const int n = juce::jmin ((int) mono.size(), info.numSamples - done);
        std::fill (mono.begin(), mono.begin() + n, 0.0f);
        for (int c = 0; c < chans; ++c)
        {
            const float* src = buffer->getReadPointer (c, info.startSample + done);
            for (int i = 0; i < n; ++i)
                mono[(size_t) i] += src[i];
        }
        detector.process (mono.data(), n);
        done += n;
    }
    info.clearActiveBufferRegion();
}

void MainComponent::releaseResources() {}

// ---------- linhas ----------

void MainComponent::addRow (const gp::Mapping& m)
{
    auto* row = rows.add (new MappingRow (*this));
    row->setMapping (m);
    rowsHolder.addAndMakeVisible (row);
    layoutRows();
}

void MainComponent::removeRow (MappingRow* row)
{
    if (learningRow == row) stopLearning();
    rows.removeObject (row);
    layoutRows();
    rowChanged();
}

void MainComponent::layoutRows()
{
    const int w = viewport.getMaximumVisibleWidth();
    rowsHolder.setSize (w, juce::jmax (1, rows.size() * (kRowHeight + 4)));
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (0, i * (kRowHeight + 4), w, kRowHeight);
}

void MainComponent::rowChanged()
{
    pushSettingsToEngine();
    saveSettings();
}

void MainComponent::pushSettingsToEngine()
{
    gp::MappingSettings s;
    for (auto* r : rows)
        s.mappings.push_back (r->getMapping());
    const int cmd = commandBox.getSelectedId();
    s.commandNote = (cmd == 1000 || cmd == 0) ? -1 : cmd - 1;
    s.tapMs = (int) tapSlider.getValue();
    engine.setSettings (s);
}

void MainComponent::startLearning (MappingRow* row)
{
    if (learningRow == row) { stopLearning(); return; }
    stopLearning();
    learningRow = row;
    row->setLearning (true);
    outputWasEnabled = engine.isOutputEnabled();
    engine.setOutputEnabled (false); // nao dispara teclas enquanto aprende
    lastNoteOnCounter = detector.getNoteOnCounter();
}

void MainComponent::stopLearning()
{
    if (learningRow == nullptr && ! learningCommand)
        return;
    if (learningRow != nullptr) learningRow->setLearning (false);
    learningRow = nullptr;
    learningCommand = false;
    commandLearnButton.setButtonText (T ("Aprender"));
    engine.setOutputEnabled (outputWasEnabled && outputToggle.getToggleState());
}

void MainComponent::refreshHoldButton()
{
    const bool on = engine.isHoldEnabled();
    holdButton.setButtonText (on ? T ("SUSTENTAÇÃO LIGADA") : T ("SUSTENTAÇÃO DESLIGADA (tudo vira toque)"));
    holdButton.setColour (juce::TextButton::buttonColourId, on ? C::on.darker (0.35f) : C::off.darker (0.35f));
}

// ---------- timer da interface ----------

void MainComponent::timerCallback()
{
    tuner.update (detector.getCurrentNote(), detector.getFrequency(), detector.getLevelDb(),
                  (float) gateSlider.getValue(), engine.isHoldEnabled());

    // atalho global F12
    const bool hk = gp::KeySender::isToggleHotkeyDown();
    if (hk && ! hotkeyWasDown)
        engine.toggleHold();
    hotkeyWasDown = hk;

    static uint32_t lastToggles = 0xffffffff;
    if (engine.getHoldToggleCounter() != lastToggles)
    {
        lastToggles = engine.getHoldToggleCounter();
        refreshHoldButton();
        saveSettings();
    }

    // aprender nota
    const uint32_t c = detector.getNoteOnCounter();
    if (c != lastNoteOnCounter)
    {
        lastNoteOnCounter = c;
        const int n = juce::jlimit (kMinNote, kMaxNote, detector.getLastNoteOn());
        if (learningRow != nullptr)
        {
            auto m = learningRow->getMapping();
            m.note = n;
            learningRow->setMapping (m);
            stopLearning();
            rowChanged();
        }
        else if (learningCommand)
        {
            commandBox.setSelectedId (n + 1, juce::dontSendNotification);
            stopLearning();
            rowChanged();
        }
    }

    for (auto* r : rows)
        r->setActive (engine.isKeyDown (r->getMapping().key));
}

// ---------- desenho e layout ----------

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (C::bg);
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (16);

    auto top = r.removeFromTop (36);
    title.setBounds (top.removeFromLeft (180));
    audioButton.setBounds (top.removeFromRight (100));
    top.removeFromRight (8);
    outputToggle.setBounds (top.removeFromRight (90));
    r.removeFromTop (10);

    tuner.setBounds (r.removeFromTop (110));
    r.removeFromTop (10);

    auto holdRow = r.removeFromTop (48);
    panicButton.setBounds (holdRow.removeFromRight (130));
    holdRow.removeFromRight (8);
    holdButton.setBounds (holdRow);
    r.removeFromTop (10);

    auto gateRow = r.removeFromTop (28);
    gateLabel.setBounds (gateRow.removeFromLeft (170));
    fastToggle.setBounds (gateRow.removeFromRight (130));
    gateSlider.setBounds (gateRow);
    auto tapRow = r.removeFromTop (28);
    tapLabel.setBounds (tapRow.removeFromLeft (170));
    tapRow.removeFromRight (130);
    tapSlider.setBounds (tapRow);
    r.removeFromTop (6);
    auto cmdRow = r.removeFromTop (30);
    commandLabel.setBounds (cmdRow.removeFromLeft (260));
    commandLearnButton.setBounds (cmdRow.removeFromRight (84));
    cmdRow.removeFromRight (6);
    commandBox.setBounds (cmdRow);
    r.removeFromTop (12);

    status.setBounds (r.removeFromBottom (24));
    auto bottom = r.removeFromBottom (34);
    addRowButton.setBounds (bottom.removeFromLeft (160));
    loadPresetButton.setBounds (bottom.removeFromRight (120));
    bottom.removeFromRight (8);
    savePresetButton.setBounds (bottom.removeFromRight (120));
    r.removeFromBottom (8);

    rowsHeader.setBounds (r.removeFromTop (22));
    viewport.setBounds (r);
    layoutRows();
}

// ---------- configuracoes ----------

juce::var MainComponent::settingsToVar (bool includeAudio) const
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> maps;
    for (auto* row : rows)
    {
        const auto m = row->getMapping();
        auto* mo = new juce::DynamicObject();
        mo->setProperty ("note", m.note);
        mo->setProperty ("key", juce::String (gp::kKeys[juce::jlimit (0, gp::kNumKeys - 1, m.key)].name));
        mo->setProperty ("mode", m.mode == gp::KeyMode::Tap ? "tap" : m.mode == gp::KeyMode::Hold ? "hold" : "latch");
        maps.add (juce::var (mo));
    }
    o->setProperty ("version", 1);
    o->setProperty ("mappings", maps);
    const int cmd = commandBox.getSelectedId();
    o->setProperty ("commandNote", (cmd == 1000 || cmd == 0) ? -1 : cmd - 1);
    o->setProperty ("tapMs", (int) tapSlider.getValue());
    o->setProperty ("gateDb", gateSlider.getValue());
    o->setProperty ("fastMode", fastToggle.getToggleState());
    o->setProperty ("holdEnabled", engine.isHoldEnabled());
    if (includeAudio)
    {
        o->setProperty ("outputEnabled", outputToggle.getToggleState());
        o->setProperty ("audio", audioStateXml);
    }
    return juce::var (o);
}

void MainComponent::settingsFromVar (const juce::var& v, bool includeAudio)
{
    const juce::ScopedValueSetter<bool> guard (loading, true);
    if (auto* arr = v["mappings"].getArray())
    {
        stopLearning();
        rows.clear();
        for (auto& mv : *arr)
        {
            gp::Mapping m;
            m.note = (int) mv["note"];
            m.key = gp::keyIndexFromName (mv["key"].toString().toRawUTF8());
            if (m.key < 0) continue;
            const auto mode = mv["mode"].toString();
            m.mode = mode == "hold" ? gp::KeyMode::Hold : mode == "latch" ? gp::KeyMode::Latch : gp::KeyMode::Tap;
            addRow (m);
        }
    }
    const int cmd = v.hasProperty ("commandNote") ? (int) v["commandNote"] : -1;
    commandBox.setSelectedId (cmd < 0 ? 1000 : cmd + 1, juce::dontSendNotification);
    tapSlider.setValue (v.hasProperty ("tapMs") ? (double) v["tapMs"] : 40.0, juce::dontSendNotification);
    gateSlider.setValue (v.hasProperty ("gateDb") ? (double) v["gateDb"] : -48.0, juce::dontSendNotification);
    fastToggle.setToggleState ((bool) v.getProperty ("fastMode", false), juce::dontSendNotification);
    detector.setGateDb ((float) gateSlider.getValue());
    detector.setFastMode (fastToggle.getToggleState());
    engine.setHoldEnabled ((bool) v.getProperty ("holdEnabled", true));
    if (includeAudio)
    {
        outputToggle.setToggleState ((bool) v.getProperty ("outputEnabled", true), juce::dontSendNotification);
        engine.setOutputEnabled (outputToggle.getToggleState());
        audioStateXml = v["audio"].toString();
    }
    pushSettingsToEngine();
}

juce::File MainComponent::settingsFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("GuitarPad").getChildFile ("settings.json");
}

void MainComponent::saveSettings()
{
    if (loading) return;
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (settingsToVar (true)));
}

void MainComponent::loadDefaults()
{
    // exemplo: ajuste as teclas para os controles do seu jogo
    const juce::String json = R"({
      "mappings": [
        { "note": 40, "key": "Esquerda", "mode": "hold" },
        { "note": 45, "key": "Direita",  "mode": "hold" },
        { "note": 50, "key": "Baixo",    "mode": "hold" },
        { "note": 55, "key": "Espaco",   "mode": "tap" },
        { "note": 59, "key": "X",        "mode": "tap" },
        { "note": 64, "key": "C",        "mode": "tap" }
      ],
      "commandNote": 84, "tapMs": 40, "gateDb": -48, "fastMode": false, "holdEnabled": true
    })";
    settingsFromVar (juce::JSON::parse (json), false);
}

void MainComponent::loadSettings()
{
    const auto f = settingsFile();
    const auto v = f.existsAsFile() ? juce::JSON::parse (f) : juce::var();
    if (v.isObject()) settingsFromVar (v, true);
    else loadDefaults();
    refreshHoldButton();
}

void MainComponent::showAudioSettings()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new AudioSettingsPanel (deviceManager, [this]
    {
        if (auto xml = deviceManager.createStateXml())
            audioStateXml = xml->toString();
        saveSettings();
    }));
    o.dialogTitle = T ("Entrada de áudio (escolha só a entrada da guitarra)");
    o.dialogBackgroundColour = C::bg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

void MainComponent::savePreset()
{
    chooser = std::make_unique<juce::FileChooser> (T ("Salvar preset"),
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("GuitarPad preset.json"), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File())
                f.withFileExtension ("json").replaceWithText (juce::JSON::toString (settingsToVar (false)));
        });
}

void MainComponent::loadPreset()
{
    chooser = std::make_unique<juce::FileChooser> (T ("Abrir preset"),
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            const auto v = juce::JSON::parse (f);
            if (v.isObject())
            {
                settingsFromVar (v, false);
                saveSettings();
            }
        });
}

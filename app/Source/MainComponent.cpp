#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize (480, 800);

    // --- top inputs ---
    urlField.setMultiLine (false);
    urlField.setReturnKeyStartsNewLine (false);
    urlField.setTextToShowWhenEmpty ("YouTube URL", juce::Colours::grey);
    addAndMakeVisible (urlField);

    pasteBtn.onClick = [this]
    {
        const auto clip = juce::SystemClipboard::getTextFromClipboard();
        if (clip.isNotEmpty())
        {
            urlField.setText (clip, juce::sendNotification);
            setStatus ("Pasted from clipboard");
        }
        else
        {
            setStatus ("Clipboard is empty");
        }
    };
    addAndMakeVisible (pasteBtn);

    efxBtn.onClick = [this]
    {
        if (efxView != nullptr)
        {
            efxView->syncFromChain();
            efxView->setVisible (true);
            efxView->toFront (true);
        }
    };
    addAndMakeVisible (efxBtn);

    loadLocalBtn.onClick = [this] { onLoadLocalClicked(); };
    addAndMakeVisible (loadLocalBtn);

    newBtn.onClick = [this] { onNewProjectClicked(); };
    addAndMakeVisible (newBtn);

    serverField.setMultiLine (false);
    serverField.setText ("http://100.99.27.104:8000", juce::dontSendNotification);
    addAndMakeVisible (serverField);

    // --- waveform + pitch ---
    addAndMakeVisible (waveform);
    waveform.onLoopStartChanged = [this] (double n) { sampler.setLoopStart (n); };
    waveform.onLoopEndChanged   = [this] (double n) { sampler.setLoopEnd   (n); };
    waveform.onPlayheadChanged  = [this] (double n) { sampler.setPlayheadNorm (n); };
    waveform.snapper            = [this] (double n) { return sampler.snapToNearestTransient (n, 0.01); };

    zoomOutBtn  .onClick = [this] { waveform.zoomOut();    }; addAndMakeVisible (zoomOutBtn);
    zoomInBtn   .onClick = [this] { waveform.zoomIn();     }; addAndMakeVisible (zoomInBtn);
    zoomResetBtn.onClick = [this] { waveform.resetView();  }; addAndMakeVisible (zoomResetBtn);

    transientBtn.onClick = [this] { cycleTransientLimit(); };
    addAndMakeVisible (transientBtn);

    addAndMakeVisible (pitch);
    pitch.onChange = [this] (double mult) { sampler.setSpeed (mult); };

    addAndMakeVisible (masterPitch);
    masterPitch.setStaticLabel ("MASTER");
    masterPitch.onChange = [this] (double mult) { sampler.setMasterSpeed (mult); };

    // --- transport row ---
    fetchBtn .onClick = [this] { onFetchClicked();  }; addAndMakeVisible (fetchBtn);
    recBtn   .onClick = [this] { onRecClicked();    }; addAndMakeVisible (recBtn);
    playBtn  .onClick = [this] { onPlayClicked();   }; addAndMakeVisible (playBtn);
    stopBtn  .onClick = [this] { onStopClicked();   }; addAndMakeVisible (stopBtn);
    exportBtn.onClick = [this] { onExportClicked(); }; addAndMakeVisible (exportBtn);

    // --- loop row ---
    setInBtn .onClick = [this] { onSetInClicked();  }; addAndMakeVisible (setInBtn);
    setOutBtn.onClick = [this] { onSetOutClicked(); }; addAndMakeVisible (setOutBtn);
    saveBtn  .onClick = [this] { onSaveClicked();   }; addAndMakeVisible (saveBtn);
    resetBtn .onClick = [this] { onResetClicked();  }; addAndMakeVisible (resetBtn);

    // --- pads ---
    for (int i = 0; i < kNumPads; ++i)
    {
        padBtns[i].setButtonText ("PAD " + juce::String (i + 1));
        padBtns[i].onClick = [this, i] { onPadClicked (i); };
        addAndMakeVisible (padBtns[i]);
    }

    rangeBtn.onClick = [this] { onRangeClicked(); };
    rangeBtn.setButtonText ("RANGE ±8%");
    addAndMakeVisible (rangeBtn);

    spreadBtn.onClick = [this] { onSpreadClicked(); };
    addAndMakeVisible (spreadBtn);

    statusLabel.setText ("Ready", juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    refreshPadButtons();

    // --- audio ---
    deviceManager.initialiseWithDefaultDevices (0, 2);
    audioSourcePlayer.setSource (&effects);
    deviceManager.addAudioCallback (&audioSourcePlayer);

    // --- effects "room" ---
    efxView = std::make_unique<EfxView> (effects);
    efxView->setVisible (false);
    efxView->onClose = [this] { if (efxView != nullptr) efxView->setVisible (false); };
    addChildComponent (efxView.get());

    startTimerHz (30);

    restoreState();
}

MainComponent::~MainComponent()
{
    persistState();
    stopTimer();
    if (recorder.isRecording()) recorder.stopRecording();
    if (inputDeviceOpened) deviceManager.removeAudioCallback (&recorder);
    deviceManager.removeAudioCallback (&audioSourcePlayer);
    audioSourcePlayer.setSource (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (10);

    auto urlRow = r.removeFromTop (40);
    newBtn      .setBounds (urlRow.removeFromLeft (60));
    urlRow.removeFromLeft (4);
    efxBtn      .setBounds (urlRow.removeFromRight (50));
    urlRow.removeFromRight (4);
    loadLocalBtn.setBounds (urlRow.removeFromRight (60));
    urlRow.removeFromRight (4);
    pasteBtn    .setBounds (urlRow.removeFromRight (60));
    urlRow.removeFromRight (4);
    urlField    .setBounds (urlRow);

    if (efxView != nullptr)
        efxView->setBounds (getLocalBounds());
    r.removeFromTop (6);
    serverField.setBounds (r.removeFromTop (32)); r.removeFromTop (8);

    statusLabel.setBounds (r.removeFromBottom (24));
    r.removeFromBottom (6);

    auto padArea   = r.removeFromBottom (140); // 2 rows of pads + range
    auto loopRow   = r.removeFromBottom (52);  r.removeFromBottom (4);
    auto transRow  = r.removeFromBottom (52);  r.removeFromBottom (4);
    auto pitchRow  = r.removeFromBottom (40);  r.removeFromBottom (2);
    auto masterRow = r.removeFromBottom (36);  r.removeFromBottom (4);
    auto zoomRow   = r.removeFromBottom (32);  r.removeFromBottom (4);

    waveform.setBounds (r);

    pitch.setBounds       (pitchRow);
    masterPitch.setBounds (masterRow);

    {
        const int gap = 4;
        const int w = (zoomRow.getWidth() - gap * 3) / 4;
        const int y = zoomRow.getY(), h = zoomRow.getHeight();
        zoomOutBtn  .setBounds (zoomRow.getX(),                 y, w, h);
        zoomInBtn   .setBounds (zoomRow.getX() + w + gap,       y, w, h);
        zoomResetBtn.setBounds (zoomRow.getX() + 2 * (w + gap), y, w, h);
        transientBtn.setBounds (zoomRow.getX() + 3 * (w + gap), y, w, h);
    }

    auto layoutRow = [] (juce::Rectangle<int> row, std::initializer_list<juce::Button*> btns)
    {
        const int n = (int) btns.size();
        const int gap = 4;
        const int w = (row.getWidth() - gap * (n - 1)) / n;
        int x = row.getX();
        for (auto* b : btns)
        {
            b->setBounds (x, row.getY(), w, row.getHeight());
            x += w + gap;
        }
    };

    layoutRow (transRow, { &fetchBtn,  &recBtn, &playBtn, &stopBtn, &exportBtn });
    layoutRow (loopRow,  { &setInBtn,  &setOutBtn, &saveBtn,  &resetBtn  });

    const int padGap = 4;
    const int padRowH = (padArea.getHeight() - padGap) / 2;
    auto row1 = padArea.removeFromTop (padRowH);
    padArea.removeFromTop (padGap);
    auto row2 = padArea;

    layoutRow (row1, { &padBtns[0], &padBtns[1], &padBtns[2], &padBtns[3] });

    const int cellW = (row2.getWidth() - padGap * 3) / 4;
    const int y = row2.getY(), h = row2.getHeight();
    padBtns[4].setBounds (row2.getX(),                       y, cellW, h);
    padBtns[5].setBounds (row2.getX() + 1 * (cellW + padGap), y, cellW, h);
    rangeBtn  .setBounds (row2.getX() + 2 * (cellW + padGap), y, cellW, h);
    spreadBtn .setBounds (row2.getX() + 3 * (cellW + padGap), y, cellW, h);
}

void MainComponent::timerCallback()
{
    if (sampler.hasFile())
    {
        waveform.setLoopStart (sampler.getLoopStartNorm());
        waveform.setLoopEnd   (sampler.getLoopEndNorm());
        waveform.setPlayhead  (sampler.getPlayheadNorm());
    }

    if (recorder.isRecording())
    {
        const int sec = (int) recorder.getElapsedSeconds();
        recBtn.setButtonText ("REC " + juce::String (sec) + "s");
    }
}

void MainComponent::setStatus (const juce::String& s)
{
    statusLabel.setText (s, juce::dontSendNotification);
}

void MainComponent::onFetchClicked()
{
    auto u = urlField.getText().trim();
    auto s = serverField.getText().trim();
    if (u.isEmpty() || s.isEmpty()) { setStatus ("Need URL and server"); return; }

    setStatus ("Fetching...");
    fetchBtn.setEnabled (false);

    fetcher.fetch (u, s, [this] (bool ok, juce::File f, juce::String err)
    {
        fetchBtn.setEnabled (true);
        if (! ok) { setStatus ("Error: " + err); return; }
        loadFromAnyFile (f);
    });
}

void MainComponent::onPlayClicked()
{
    if (! sampler.hasFile()) { setStatus ("Load a clip first"); return; }
    sampler.setPlaying (true);
}

void MainComponent::onStopClicked()
{
    sampler.stopAllVoices();
}

void MainComponent::onSetInClicked()
{
    if (! sampler.hasFile()) return;
    auto p = sampler.getPlayheadNorm();
    if (sampler.getTransientLimit() != 0) p = sampler.snapToNearestTransient (p, 0.01);
    auto e = sampler.getLoopEndNorm();
    p = juce::jmin (p, e - 0.001);
    sampler.setLoopStart (p);
    waveform.setLoopStart (p);
}

void MainComponent::onSetOutClicked()
{
    if (! sampler.hasFile()) return;
    auto p = sampler.getPlayheadNorm();
    if (sampler.getTransientLimit() != 0) p = sampler.snapToNearestTransient (p, 0.01);
    auto s = sampler.getLoopStartNorm();
    p = juce::jmax (p, s + 0.001);
    sampler.setLoopEnd (p);
    waveform.setLoopEnd (p);
}

void MainComponent::onSaveClicked()
{
    if (! sampler.hasFile()) { setStatus ("Load a clip first"); return; }
    capturePad (sampler.getActiveVoice());
    setStatus ("Saved PAD " + juce::String (sampler.getActiveVoice() + 1));
}

void MainComponent::onResetClicked()
{
    pitch.resetToZero();
}

void MainComponent::onLoadLocalClicked()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select audio file",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto url = fc.getURLResult();
            if (url.isEmpty())
            {
                setStatus ("Load cancelled");
                return;
            }

            // Preserve the picked file's extension so the format manager
            // can pick the right decoder (Android returns content:// URIs).
            auto fname = url.getFileName();
            auto ext   = fname.fromLastOccurrenceOf (".", true, false);
            if (ext.isEmpty() || ext.length() > 6) ext = ".wav";

            auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("boceto_local_"
                                          + juce::String (juce::Time::currentTimeMillis())
                                          + ext);
            tmp.deleteFile();

            std::unique_ptr<juce::InputStream> in (
                url.createInputStream (juce::URL::InputStreamOptions (
                    juce::URL::ParameterHandling::inAddress)));
            if (in == nullptr)
            {
                setStatus ("Cannot open file");
                return;
            }

            juce::FileOutputStream fos (tmp);
            if (! fos.openedOk())
            {
                setStatus ("Cannot write temp: " + tmp.getFullPathName());
                return;
            }
            const auto bytes = fos.writeFromInputStream (*in, -1);
            fos.flush();

            if (bytes <= 0)
            {
                setStatus ("Empty file");
                return;
            }

            loadFromAnyFile (tmp);
        });
}

juce::File MainComponent::getStateFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
              .getChildFile ("Boceto").getChildFile ("state.json");
}

void MainComponent::persistState()
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("active_voice", sampler.getActiveVoice());

    juce::Array<juce::var> padArr;
    for (int i = 0; i < kNumPads; ++i)
    {
        auto* pp = new juce::DynamicObject();
        pp->setProperty ("occupied",  pads[i].occupied);
        pp->setProperty ("clip_path", pads[i].clipPath);
        pp->setProperty ("loopStart", pads[i].loopStart);
        pp->setProperty ("loopEnd",   pads[i].loopEnd);
        pp->setProperty ("speed",     pads[i].speed);
        padArr.add (juce::var (pp));
    }
    root->setProperty ("pads", padArr);

    auto stateFile = getStateFile();
    stateFile.getParentDirectory().createDirectory();
    stateFile.replaceWithText (juce::JSON::toString (juce::var (root)));
}

void MainComponent::restoreState()
{
    auto stateFile = getStateFile();
    if (! stateFile.existsAsFile()) { refreshPadButtons(); return; }

    auto json = juce::JSON::parse (stateFile);
    auto* obj = json.getDynamicObject();
    if (obj == nullptr) { refreshPadButtons(); return; }

    auto padsVar = obj->getProperty ("pads");
    if (padsVar.isArray())
    {
        const auto& arr = *padsVar.getArray();
        for (int i = 0; i < kNumPads && i < arr.size(); ++i)
        {
            auto* pp = arr[i].getDynamicObject();
            if (pp == nullptr) continue;
            pads[i].occupied  = pp->getProperty ("occupied");
            pads[i].clipPath  = pp->getProperty ("clip_path").toString();
            pads[i].loopStart = (double) pp->getProperty ("loopStart");
            pads[i].loopEnd   = (double) pp->getProperty ("loopEnd");
            pads[i].speed     = (double) pp->getProperty ("speed");

            // Reload each occupied pad's clip into its voice
            if (pads[i].occupied)
            {
                bool ok = false;
                if (pads[i].clipPath.isNotEmpty())
                {
                    auto clipFile = juce::File (pads[i].clipPath);
                    if (clipFile.existsAsFile())
                    {
                        sampler.setActiveVoice (i);
                        if (sampler.loadFile (clipFile))
                        {
                            sampler.voice (i).setLoopStart (pads[i].loopStart);
                            sampler.voice (i).setLoopEnd   (pads[i].loopEnd);
                            sampler.voice (i).setSpeed     (pads[i].speed);
                            ok = true;
                        }
                    }
                }
                if (! ok) pads[i].occupied = false;
            }
        }
    }

    int activeIdx = (int) obj->getProperty ("active_voice");
    activeIdx = juce::jlimit (0, kNumPads - 1, activeIdx);
    selectActivePad (activeIdx);
    refreshPadButtons();

    for (int i = 0; i < kNumPads; ++i)
        if (pads[i].occupied) { projectDirty = true; break; }
}

void MainComponent::selectActivePad (int padIndex)
{
    sampler.setActiveVoice (padIndex);
    auto& p = pads[padIndex];

    if (p.occupied && p.clipPath.isNotEmpty())
    {
        auto f = juce::File (p.clipPath);
        if (f.existsAsFile())
            waveform.setSource (f);
    }
    else
    {
        waveform.clear();
    }
    waveform.setTransients (sampler.getActiveTransients());
    pitch.setRateMultiplier (sampler.getSpeed());
    rangeBtn.setButtonText ("RANGE \xc2\xb1" + juce::String (pitch.getRangePercent()) + "%");
    refreshPadButtons();
}

void MainComponent::loadFromAnyFile (const juce::File& f)
{
    const int slot = sampler.getActiveVoice();
    if (sampler.loadFile (f))
    {
        const auto cached = sampler.getCachedClipFile (slot);
        pads[slot].occupied  = true;
        pads[slot].clipPath  = cached.getFullPathName();
        pads[slot].loopStart = 0.0;
        pads[slot].loopEnd   = 1.0;
        pads[slot].speed     = 1.0;

        waveform.setSource (cached);
        waveform.setLoopStart (0.0);
        waveform.setLoopEnd   (1.0);
        sampler.setLoopStart (0.0);
        sampler.setLoopEnd   (1.0);
        sampler.setSpeed (1.0);
        pitch.setRateMultiplier (1.0);
        waveform.setTransients (sampler.getActiveTransients());

        setStatus ("Loaded into PAD " + juce::String (slot + 1) + " — "
                   + juce::String (sampler.getLengthSeconds(), 2) + " s ("
                   + juce::String ((int) sampler.getActiveTransients().size()) + " transients)");
        refreshPadButtons();
        persistState();
        markDirty();
    }
    else
    {
        setStatus ("Decode failed");
    }
}

void MainComponent::cycleTransientLimit()
{
    // Cycle: OFF(0) → 4 → 8 → 16 → 32 → ALL(-1) → OFF...
    static constexpr int values[] = { 0, 4, 8, 16, 32, -1 };
    static constexpr const char* labels[] = { "T:OFF", "T:4", "T:8", "T:16", "T:32", "T:ALL" };
    transientCycleIdx = (transientCycleIdx + 1) % 6;
    const int v = values[transientCycleIdx];
    sampler.setTransientLimit (v);
    transientBtn.setButtonText (labels[transientCycleIdx]);
    transientBtn.setColour (juce::TextButton::buttonColourId,
                            v == 0 ? juce::Colour (0xff262626)
                                   : juce::Colour (0xff2e6c8a));
    if (sampler.hasFile())
    {
        const auto active = sampler.getActiveTransients();
        waveform.setTransients (active);
        setStatus (juce::String (labels[transientCycleIdx]) + " — "
                   + juce::String ((int) active.size()) + " active");
    }
}

void MainComponent::onRangeClicked()
{
    pitch.cycleRange();
    rangeBtn.setButtonText ("RANGE ±" + juce::String (pitch.getRangePercent()) + "%");
}

void MainComponent::onSpreadClicked()
{
    if (! sampler.hasFile()) { setStatus ("Load a clip first"); return; }

    // PopupMenu was unreliable on Android touch screens — same tap that opens
    // it can also dismiss it. AlertWindow's button row is rock-solid.
    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withTitle ("Spread across pads")
            .withMessage ("How should the source clip be sliced into the 6 pads?")
            .withButton ("Equal slices")
            .withButton ("By transients")
            .withButton ("Cancel"),
        [this] (int choice)
        {
            // AlertWindow button-index convention is 1-based, in order added.
            if (choice == 1) spreadToAllPads (false);
            else if (choice == 2) spreadToAllPads (true);
        });
}

void MainComponent::spreadToAllPads (bool useTransients)
{
    const int sourceVoice = sampler.getActiveVoice();
    if (! sampler.voice (sourceVoice).hasFile()) { setStatus ("Load a clip first"); return; }

    auto sourceFile = juce::File (sampler.voice (sourceVoice).getFilePath());
    if (! sourceFile.existsAsFile()) { setStatus ("Source file missing"); return; }

    sampler.stopAllVoices();

    double bounds[kNumPads + 1] {};
    bool didTransients = false;

    if (useTransients)
    {
        const auto trs = sampler.voice (sourceVoice).getActiveTransients();
        if ((int) trs.size() >= kNumPads)
        {
            bounds[0] = 0.0;
            bounds[kNumPads] = 1.0;
            for (int i = 1; i < kNumPads; ++i)
            {
                int idx = juce::roundToInt ((double) i * (double) (trs.size() - 1) / (double) kNumPads);
                idx = juce::jlimit (0, (int) trs.size() - 1, idx);
                bounds[i] = juce::jlimit (0.0, 1.0, trs[(std::size_t) idx]);
            }
            for (int i = 1; i <= kNumPads; ++i)
                if (bounds[i] <= bounds[i - 1])
                    bounds[i] = juce::jmin (1.0, bounds[i - 1] + 0.001);
            didTransients = true;
        }
    }

    if (! didTransients)
        for (int i = 0; i <= kNumPads; ++i)
            bounds[i] = (double) i / (double) kNumPads;

    for (int i = 0; i < kNumPads; ++i)
    {
        sampler.setActiveVoice (i);
        if (! sampler.loadFile (sourceFile))
        {
            sampler.setActiveVoice (sourceVoice);
            setStatus ("Spread failed at PAD " + juce::String (i + 1));
            return;
        }
        const auto cached = sampler.getCachedClipFile (i);
        const double s = bounds[i];
        const double e = bounds[(size_t) (i + 1)];
        sampler.voice (i).setLoopStart (s);
        sampler.voice (i).setLoopEnd   (e);
        sampler.voice (i).setSpeed     (1.0);

        pads[i].occupied  = true;
        pads[i].clipPath  = cached.getFullPathName();
        pads[i].loopStart = s;
        pads[i].loopEnd   = e;
        pads[i].speed     = 1.0;
    }

    sampler.setActiveVoice (sourceVoice);
    selectActivePad (sourceVoice);

    if (useTransients && ! didTransients)
        setStatus ("Not enough transients — spread equally across 6 pads");
    else
        setStatus (didTransients ? "Spread by transients across 6 pads"
                                 : "Spread equally across 6 pads");

    refreshPadButtons();
    persistState();
    markDirty();
}

// ─── recording ──────────────────────────────────────────────────────────────

#if JUCE_ANDROID
#include <jni.h>
namespace juce { extern JNIEnv* getEnv() noexcept; }

static bool androidHeadphonesConnected()
{
    auto* env = juce::getEnv();
    if (env == nullptr) return false;

    jclass activityThreadCls = env->FindClass ("android/app/ActivityThread");
    if (activityThreadCls == nullptr) { env->ExceptionClear(); return false; }
    jmethodID currentApp = env->GetStaticMethodID (activityThreadCls,
                                                    "currentApplication",
                                                    "()Landroid/app/Application;");
    jobject app = env->CallStaticObjectMethod (activityThreadCls, currentApp);
    if (app == nullptr) { env->ExceptionClear(); return false; }

    jclass contextCls = env->FindClass ("android/content/Context");
    jmethodID getSystemService = env->GetMethodID (contextCls, "getSystemService",
                                                    "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring audioName = env->NewStringUTF ("audio");
    jobject am = env->CallObjectMethod (app, getSystemService, audioName);
    env->DeleteLocalRef (audioName);
    if (am == nullptr) { env->ExceptionClear(); return false; }

    jclass amCls = env->FindClass ("android/media/AudioManager");
    jmethodID isWiredHeadsetOn  = env->GetMethodID (amCls, "isWiredHeadsetOn",  "()Z");
    jmethodID isBluetoothA2dpOn = env->GetMethodID (amCls, "isBluetoothA2dpOn", "()Z");

    bool wired = env->CallBooleanMethod (am, isWiredHeadsetOn);
    bool bt    = env->CallBooleanMethod (am, isBluetoothA2dpOn);

    env->DeleteLocalRef (am);
    env->DeleteLocalRef (app);
    env->DeleteLocalRef (amCls);
    env->DeleteLocalRef (contextCls);
    env->DeleteLocalRef (activityThreadCls);

    return wired || bt;
}
#endif

static bool isOutputRouteSafeForMonitoring()
{
   #if JUCE_ANDROID
    return androidHeadphonesConnected();
   #else
    // Desktop dev: assume an audio interface or headphones — don't auto-stop loops.
    return true;
   #endif
}

void MainComponent::onRecClicked()
{
    if (recorder.isRecording())
    {
        stopRecordingFlow();
        return;
    }

   #if JUCE_ANDROID || JUCE_IOS
    juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
        [this] (bool granted)
        {
            if (! granted) { setStatus ("Mic permission denied"); return; }
            startRecordingFlow();
        });
   #else
    startRecordingFlow();
   #endif
}

void MainComponent::startRecordingFlow()
{
    // Close the output side entirely while recording — even with voices
    // stopped, Android Oboe input+output combined can trigger HAL-level
    // sidetone / passthrough on some devices (Pixel 4a 5G observed).
    sampler.stopAllVoices();
    loopsAutoStoppedForRec = true;

    deviceManager.removeAudioCallback (&audioSourcePlayer);
    deviceManager.closeAudioDevice();

    const auto err = deviceManager.initialiseWithDefaultDevices (1, 0);
    if (err.isNotEmpty())
    {
        // restore the output-only device so the app keeps working
        deviceManager.initialiseWithDefaultDevices (0, 2);
        deviceManager.addAudioCallback (&audioSourcePlayer);
        inputDeviceOpened = false;
        setStatus ("Audio input failed: " + err);
        return;
    }
    deviceManager.addAudioCallback (&recorder);
    inputDeviceOpened = true;

    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile ("boceto_rec_"
                                 + juce::String (juce::Time::currentTimeMillis())
                                 + ".wav");

    if (! recorder.startRecording (tmp))
    {
        setStatus ("Recording failed (no input?)");
        return;
    }

    recBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffaa2030));
    setStatus ("Recording — output muted, tap REC to stop");
}

void MainComponent::stopRecordingFlow()
{
    auto file = recorder.stopRecording();
    recBtn.setButtonText ("REC");
    recBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff262626));

    // Tear down input device, restore output-only
    if (inputDeviceOpened)
    {
        deviceManager.removeAudioCallback (&recorder);
        deviceManager.closeAudioDevice();
        deviceManager.initialiseWithDefaultDevices (0, 2);
        deviceManager.addAudioCallback (&audioSourcePlayer);
        inputDeviceOpened = false;
    }

    if (! file.existsAsFile() || file.getSize() < 1024)
    {
        if (file.existsAsFile()) file.deleteFile();
        setStatus ("No audio captured");
        return;
    }

    setStatus ("Recorded " + juce::String (file.getSize() / 1024) + " KB — loading…");
    loadFromAnyFile (file);
    file.deleteFile();
    juce::ignoreUnused (loopsAutoStoppedForRec);
}

// ─── project (NEW / save) ───────────────────────────────────────────────────

void MainComponent::markDirty()
{
    projectDirty = true;
    if (! newBtn.getButtonText().endsWithChar ('*'))
        newBtn.setButtonText ("NEW*");
}

bool MainComponent::saveProjectToDisk()
{
    auto projectsRoot = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Boceto").getChildFile ("Projects");
    projectsRoot.createDirectory();

    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S");
    auto folder = projectsRoot.getChildFile (stamp);
    int suffix = 1;
    while (folder.exists()) folder = projectsRoot.getChildFile (stamp + "_" + juce::String (++suffix));
    if (! folder.createDirectory()) return false;

    auto* root = new juce::DynamicObject();
    root->setProperty ("name", stamp);
    root->setProperty ("active_voice", sampler.getActiveVoice());

    juce::Array<juce::var> padArr;
    for (int i = 0; i < kNumPads; ++i)
    {
        auto* pp = new juce::DynamicObject();
        pp->setProperty ("occupied",  pads[i].occupied);
        pp->setProperty ("loopStart", pads[i].loopStart);
        pp->setProperty ("loopEnd",   pads[i].loopEnd);
        pp->setProperty ("speed",     pads[i].speed);

        if (pads[i].occupied && pads[i].clipPath.isNotEmpty())
        {
            auto src = juce::File (pads[i].clipPath);
            if (src.existsAsFile())
            {
                auto dst = folder.getChildFile ("pad" + juce::String (i) + ".wav");
                src.copyFileTo (dst);
                pp->setProperty ("clip_path", dst.getFullPathName());
            }
            else
            {
                pp->setProperty ("clip_path", juce::var());
            }
        }
        padArr.add (juce::var (pp));
    }
    root->setProperty ("pads", padArr);

    auto projectFile = folder.getChildFile ("project.json");
    if (! projectFile.replaceWithText (juce::JSON::toString (juce::var (root))))
        return false;

    setStatus ("Saved project: " + folder.getFullPathName());
    return true;
}

void MainComponent::clearAllPadsAndState()
{
    sampler.stopAllVoices();
    if (recorder.isRecording()) recorder.stopRecording();

    sampler.clearAllVoices();

    for (int i = 0; i < kNumPads; ++i)
    {
        pads[i] = LoopState();
        sampler.getCachedClipFile (i).deleteFile();
    }

    waveform.clear();
    pitch.setRateMultiplier (1.0);
    pitch.resetToZero();

    projectDirty = false;
    newBtn.setButtonText ("NEW");
    refreshPadButtons();
    persistState();
    setStatus ("New project");
}

void MainComponent::onNewProjectClicked()
{
    if (! projectDirty)
    {
        clearAllPadsAndState();
        return;
    }

    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withTitle ("Start a new project?")
            .withMessage ("Current pads have unsaved changes.")
            .withButton ("Save")
            .withButton ("Discard")
            .withButton ("Cancel"),
        [this] (int result)
        {
            // JUCE returns 1 for the first button, 2 for the second, etc.
            if (result == 1)
            {
                if (saveProjectToDisk()) clearAllPadsAndState();
                else                     setStatus ("Project save failed — keeping current state");
            }
            else if (result == 2)
            {
                clearAllPadsAndState();
            }
            // result == 0 / Cancel: do nothing
        });
}

void MainComponent::onPadClicked (int padIndex)
{
    selectActivePad (padIndex);
    if (pads[padIndex].occupied)
    {
        auto& v = sampler.voice (padIndex);
        if (v.isPlaying())
        {
            v.setPlaying (false);
            setStatus ("Stopped PAD " + juce::String (padIndex + 1));
        }
        else
        {
            recallPad (padIndex);
            setStatus ("Triggered PAD " + juce::String (padIndex + 1));
        }
    }
    else
    {
        setStatus ("PAD " + juce::String (padIndex + 1) + " selected — fetch or load to fill");
    }
}

void MainComponent::capturePad (int padIndex)
{
    auto& p = pads[padIndex];
    if (! sampler.voice (padIndex).hasFile()) return;
    p.occupied  = true;
    p.loopStart = sampler.voice (padIndex).getLoopStartNorm();
    p.loopEnd   = sampler.voice (padIndex).getLoopEndNorm();
    p.speed     = sampler.voice (padIndex).getSpeed();
    refreshPadButtons();
    persistState();
    markDirty();
}

void MainComponent::recallPad (int padIndex)
{
    auto& p = pads[padIndex];
    auto& v = sampler.voice (padIndex);
    if (! v.hasFile()) return;
    v.setLoopStart (p.loopStart);
    v.setLoopEnd   (p.loopEnd);
    v.setSpeed     (p.speed);
    v.jumpToLoopStart();
    v.setPlaying   (true);

    pitch.setRateMultiplier (p.speed);
    rangeBtn.setButtonText ("RANGE \xc2\xb1" + juce::String (pitch.getRangePercent()) + "%");
}

void MainComponent::refreshPadButtons()
{
    for (int i = 0; i < kNumPads; ++i)
    {
        const bool occ    = pads[i].occupied;
        const bool active = (i == sampler.getActiveVoice());
        padBtns[i].setEnabled (true);

        juce::Colour col;
        if (occ && active)      col = juce::Colour (0xff44a060);
        else if (occ)           col = juce::Colour (0xff2e6c4a);
        else if (active)        col = juce::Colour (0xff444444);
        else                    col = juce::Colour (0xff262626);
        padBtns[i].setColour (juce::TextButton::buttonColourId, col);
    }
}

void MainComponent::onExportClicked()
{
    if (! sampler.hasFile()) { setStatus ("Load a clip first"); return; }

    LoopExporter::Request req;
    if (! sampler.snapshotForExport (req.source, req.sourceSampleRate))
    {
        setStatus ("Snapshot failed");
        return;
    }

    const double total = (double) req.source.getNumSamples();
    req.startSample = (int64_t) (sampler.getLoopStartNorm() * total);
    req.endSample   = (int64_t) (sampler.getLoopEndNorm()   * total);
    req.speed       = sampler.getSpeed();
    req.padIndex    = sampler.getActiveVoice() + 1;

    setStatus ("Exporting...");
    exportBtn.setEnabled (false);

    exporter.exportLoop (std::move (req), [this] (bool ok, juce::File f, juce::String err)
    {
        exportBtn.setEnabled (true);
        setStatus (ok ? "Saved: " + f.getFullPathName()
                      : "Export error: " + err);
    });
}

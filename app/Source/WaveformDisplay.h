#pragma once
#include "JuceHeader.h"

/*  Waveform display with draggable In/Out markers, a playhead overlay,
    tap-to-scrub, zoom (Z+/Z-) and pan-when-zoomed.

    Coordinates:
      * loopStart/loopEnd/playhead are 0..1 of the full clip.
      * viewStart/viewEnd are 0..1 of the full clip — currently visible window.
      * Zoom factor = 1 / (viewEnd - viewStart). */
class WaveformDisplay : public juce::Component, private juce::ChangeListener
{
public:
    WaveformDisplay()
    {
        formatManager.registerBasicFormats();
        thumb.addChangeListener (this);
    }

    ~WaveformDisplay() override
    {
        thumb.removeChangeListener (this);
    }

    void setSource (const juce::File& f)
    {
        thumb.setSource (new juce::FileInputSource (f));
        resetView();
    }

    void clear()
    {
        thumb.clear();
        resetView();
    }

    void setLoopStart (double n) { loopStart = juce::jlimit (0.0, 1.0, n); repaint(); }
    void setLoopEnd   (double n) { loopEnd   = juce::jlimit (0.0, 1.0, n); repaint(); }
    void setPlayhead  (double n) { playhead  = juce::jlimit (0.0, 1.0, n); repaint(); }
    void setTransients (std::vector<double> t) { transients = std::move (t); repaint(); }

    /** Optional snap callback — if set, marker drags pull to nearest transient. */
    std::function<double (double)> snapper;

    void zoomIn()      { applyZoom (zoomFactor() * 2.0); }
    void zoomOut()     { applyZoom (zoomFactor() * 0.5); }
    void resetView()   { viewStart = 0.0; viewEnd = 1.0; repaint(); }

    double zoomFactor() const
    {
        const double w = viewEnd - viewStart;
        return w > 0.0 ? 1.0 / w : 1.0;
    }

    std::function<void (double)> onLoopStartChanged;
    std::function<void (double)> onLoopEndChanged;
    std::function<void (double)> onPlayheadChanged;

    void paint (juce::Graphics& g) override
    {
        auto rFloat = getLocalBounds().toFloat();
        g.fillAll (juce::Colour (0xff0c0c0c));
        g.setColour (juce::Colour (0xff222222));
        g.drawRect (rFloat, 1.0f);

        if (thumb.getTotalLength() <= 0.0)
        {
            g.setColour (juce::Colours::grey);
            g.drawText ("no clip loaded", getLocalBounds(),
                        juce::Justification::centred);
            return;
        }

        const double total = thumb.getTotalLength();
        g.setColour (juce::Colour (0xff44d680));
        thumb.drawChannels (g, getLocalBounds().reduced (2),
                            viewStart * total, viewEnd * total, 1.0f);

        const auto viewW = viewEnd - viewStart;
        auto toX = [&] (double n) -> float
        {
            return (float) ((n - viewStart) / viewW * getWidth());
        };

        // loop overlay (clipped to view)
        const double s = juce::jlimit (viewStart, viewEnd, loopStart);
        const double e = juce::jlimit (viewStart, viewEnd, loopEnd);
        if (e > s)
        {
            g.setColour (juce::Colour (0x33ffaa00));
            g.fillRect (toX (s), 0.0f, toX (e) - toX (s), (float) getHeight());
        }

        auto drawMarker = [&] (double pos, juce::Colour c)
        {
            if (pos < viewStart || pos > viewEnd) return;
            const float x = toX (pos);
            g.setColour (c);
            g.drawLine (x, 0.0f, x, (float) getHeight(), 2.0f);
            g.fillRect (juce::Rectangle<float> (x - 5.0f, 0.0f, 10.0f, 10.0f));
        };
        // transient ticks (drawn before markers so markers stay on top)
        if (! transients.empty())
        {
            g.setColour (juce::Colours::skyblue.withAlpha (0.85f));
            for (double t : transients)
            {
                if (t < viewStart || t > viewEnd) continue;
                const float x = toX (t);
                g.drawLine (x, 2.0f, x, (float) getHeight() - 2.0f, 1.0f);
            }
        }

        drawMarker (loopStart, juce::Colours::orange);
        drawMarker (loopEnd,   juce::Colours::orangered);

        if (playhead >= viewStart && playhead <= viewEnd)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawLine (toX (playhead), 0.0f, toX (playhead), (float) getHeight(), 1.0f);
        }

        if (zoomFactor() > 1.0001)
        {
            g.setColour (juce::Colours::lightgrey);
            g.setFont (12.0f);
            g.drawText (juce::String (zoomFactor(), 1) + "x",
                        getLocalBounds().reduced (4),
                        juce::Justification::topRight);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (thumb.getTotalLength() <= 0.0) return;

        const double n      = pixelToNorm (e.x);
        const float  xs     = normToPixel (loopStart);
        const float  xe     = normToPixel (loopEnd);
        const float  hit    = (float) hitBox;
        const float  distS  = std::abs ((float) e.x - xs);
        const float  distE  = std::abs ((float) e.x - xe);

        if (distS < hit && distS <= distE)
            dragging = DragStart;
        else if (distE < hit)
            dragging = DragEnd;
        else if (zoomFactor() > 1.0001)
        {
            dragging      = DragPan;
            panStartView  = viewStart;
            panStartMouse = e.x;
        }
        else
        {
            dragging = None;
            // tap-to-scrub
            playhead = n;
            if (onPlayheadChanged) onPlayheadChanged (n);
            repaint();
        }

        if (dragging == DragStart || dragging == DragEnd)
            handleMarkerDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging == DragStart || dragging == DragEnd)
            handleMarkerDrag (e);
        else if (dragging == DragPan)
            handlePan (e);
        else
        {
            // free scrub when not zoomed
            if (zoomFactor() <= 1.0001)
            {
                const double n = pixelToNorm (e.x);
                playhead = n;
                if (onPlayheadChanged) onPlayheadChanged (n);
                repaint();
            }
        }
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = None; }

private:
    double pixelToNorm (int x) const
    {
        const double w = viewEnd - viewStart;
        return juce::jlimit (0.0, 1.0,
                             viewStart + (double) x / (double) getWidth() * w);
    }

    float normToPixel (double n) const
    {
        const double w = viewEnd - viewStart;
        return (float) ((n - viewStart) / w * getWidth());
    }

    void handleMarkerDrag (const juce::MouseEvent& e)
    {
        double n = pixelToNorm (e.x);
        if (snapper) n = snapper (n);
        if (dragging == DragStart)
        {
            const double clamped = juce::jmin (n, loopEnd - 0.001);
            setLoopStart (clamped);
            if (onLoopStartChanged) onLoopStartChanged (clamped);
        }
        else
        {
            const double clamped = juce::jmax (n, loopStart + 0.001);
            setLoopEnd (clamped);
            if (onLoopEndChanged) onLoopEndChanged (clamped);
        }
    }

    void handlePan (const juce::MouseEvent& e)
    {
        const double w = viewEnd - viewStart;
        const double dxNorm = (double) (e.x - panStartMouse) / (double) getWidth() * w;
        double newStart = panStartView - dxNorm;
        const double maxStart = 1.0 - w;
        newStart = juce::jlimit (0.0, maxStart, newStart);
        viewStart = newStart;
        viewEnd   = newStart + w;
        repaint();
    }

    void applyZoom (double newFactor)
    {
        newFactor = juce::jlimit (1.0, 64.0, newFactor);
        const double newW = 1.0 / newFactor;
        const double centre = (viewStart + viewEnd) * 0.5;
        double newStart = centre - newW * 0.5;
        double newEnd   = newStart + newW;
        if (newStart < 0.0) { newStart = 0.0; newEnd = newW; }
        if (newEnd   > 1.0) { newEnd   = 1.0; newStart = 1.0 - newW; }
        viewStart = newStart;
        viewEnd   = newEnd;
        repaint();
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    juce::AudioFormatManager  formatManager;
    juce::AudioThumbnailCache thumbCache { 1 };
    juce::AudioThumbnail      thumb { 512, formatManager, thumbCache };

    double loopStart = 0.0, loopEnd = 1.0, playhead = 0.0;
    double viewStart = 0.0, viewEnd = 1.0;
    std::vector<double> transients;

    enum DragMode { None, DragStart, DragEnd, DragPan } dragging = None;
    double panStartView = 0.0;
    int    panStartMouse = 0;

    static constexpr int hitBox = 28;
};

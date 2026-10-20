#pragma once
// Disc wait synchronization: what the game's disc check is told while the Insert-disc window is open.
#include <atomic>

namespace dbziw::setup {

class DiscWait
{
public:
    enum class Event
    {
        None,
        WindowOpened, // the requested disc is missing: open the Insert-disc window
        DiscReady,    // it is installed now: close the window, the game continues
    };

    bool notReady(bool installed, Event &event)
    {
        event = Event::None;
        if (m_cancelled)
            return false;
        if (installed)
        {
            if (m_waiting.exchange(false))
                event = Event::DiscReady;
            return false;
        }
        if (!m_waiting.exchange(true))
            event = Event::WindowOpened;
        return true;
    }

    void cancel()
    {
        if (m_waiting.exchange(false))
            m_cancelled = true;
    }

    void checked() { m_cancelled = false; }

    bool waiting() const { return m_waiting; }

private:
    std::atomic<bool> m_waiting{false};
    std::atomic<bool> m_cancelled{false};
};

} // namespace dbziw::setup

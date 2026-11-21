#pragma once

namespace atmos::editor
{
    struct EditorContext;

    // Floating preferences window (Edit > Preferences): appearance, camera, viewport and autosave.
    // Saves Saved/Editor/EditorPrefs.json when an edit is finished.
    class PreferencesWindow
    {
    public:
        void Open() { open_ = true; }
        void Draw(EditorContext& context);

    private:
        bool open_ = false;
        bool dirty_ = false;
    };
}

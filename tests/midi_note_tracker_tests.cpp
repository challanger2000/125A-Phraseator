#include "midi_note_tracker.h"
#include "test_common.h"

using namespace phraseator;

int main() {
    MidiNoteTracker tracker;

    CHECK(!tracker.held(60));
    CHECK(tracker.highestHeld() == -1);

    tracker.noteOn(60);
    tracker.noteOn(60);
    CHECK(tracker.held(60));
    CHECK(tracker.highestHeld() == 60);

    tracker.noteOff(60);
    CHECK(tracker.held(60));
    CHECK(tracker.highestHeld() == 60);

    tracker.noteOff(60);
    CHECK(!tracker.held(60));
    CHECK(tracker.highestHeld() == -1);

    tracker.noteOff(60); // extra NoteOff must not underflow
    CHECK(!tracker.held(60));

    tracker.noteOn(48);
    tracker.noteOn(72);
    tracker.noteOn(67);
    CHECK(tracker.highestHeld() == 72);
    tracker.noteOff(72);
    CHECK(tracker.highestHeld() == 67);

    tracker.clear();
    CHECK(tracker.highestHeld() == -1);

    tracker.noteOn(-1);
    tracker.noteOn(128);
    CHECK(tracker.highestHeld() == -1);

    return 0;
}

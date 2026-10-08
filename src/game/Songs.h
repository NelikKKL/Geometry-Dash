// Soundtrack table, read from the 1.0 APK (LevelTools::getAudioTitle / getAudioFileName / artistForAudio /
// urlForAudio / nameForArtist / ngURLForArtist / ytURLForArtist / fbURLForArtist). Switch tables were decoded
// by hand from libgame.so because Ghidra cannot follow the jump tables.
#pragma once
#include <cstddef>

namespace ogd {

struct Artist {
    const char* name;
    const char* ng;   // Newgrounds profile
    const char* yt;   // nullptr = none
    const char* fb;   // nullptr = none
};

struct Song {
    int id;               // -1 = practice-mode track
    const char* title;
    const char* file;     // nullptr for the practice track (not shipped as its own file in the 1.0 level list)
    int artist;           // index into kArtists
    const char* url;      // Newgrounds audio page, opened by "Download"
};

inline constexpr Artist kArtists[5] = {
    {"DJVI", "http://djvi.newgrounds.com/", "http://www.youtube.com/user/DJVITechno", nullptr},
    {"Waterflame", "http://waterflame.newgrounds.com/", "http://www.youtube.com/user/waterflame89",
     "http://www.facebook.com/pages/Waterflame/210371073165"},
    {"OcularNebula", "http://ocularnebula.newgrounds.com/", nullptr, nullptr},
    {"ForeverBound", "http://foreverbound.newgrounds.com/", "http://www.youtube.com/user/ForeverBoundOfficial",
     "https://www.facebook.com/foreverboundofficial"},
    {"Step", "http://step.newgrounds.com/", "http://www.youtube.com/user/NGStep",
     "https://www.facebook.com/StephanWellsMusic"},
};

inline constexpr Song kSongs[11] = {
    {-1, "Practice: Stay Inside Me", "StayInsideMe.mp3", 2, "http://www.newgrounds.com/audio/listen/535331"},
    {0, "Stereo Madness", "StereoMadness.mp3", 3, "http://www.newgrounds.com/audio/listen/500476"},
    {1, "Back On Track", "BackOnTrack.mp3", 0, "http://www.newgrounds.com/audio/listen/522654"},
    {2, "Polargeist", "Polargeist.mp3", 4, "http://www.newgrounds.com/audio/listen/523561"},
    {3, "Dry Out", "DryOut.mp3", 0, "http://www.newgrounds.com/audio/listen/498543"},
    {4, "Base After Base", "BaseAfterBase.mp3", 0, "http://www.newgrounds.com/audio/listen/404997"},
    {5, "Cant Let Go", "CantLetGo.mp3", 0, "http://www.newgrounds.com/audio/listen/485351"},
    {6, "Jumper", "Jumper.mp3", 1, "http://www.newgrounds.com/audio/listen/168734"},
    {7, "Time Machine", "TimeMachine.mp3", 1, "http://www.newgrounds.com/audio/listen/291458"},
    {8, "Cycles", "Cycles.mp3", 0, "http://www.newgrounds.com/audio/listen/529148"},
    {9, "xStep", "xStep.mp3", 0, "http://www.newgrounds.com/audio/listen/516735"},
};

// SongsLayer::customSetup lists tracks 0..6 and then SongObject(-1).
inline constexpr int kSongsListed = 8;
inline const Song& listedSong(int row) { return row < 7 ? kSongs[row + 1] : kSongs[0]; }

} // namespace ogd

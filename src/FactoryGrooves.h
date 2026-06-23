#pragma once

#include <JuceHeader.h>
#include <array>
#include <utility>
#include "Pattern.h"
#include "DrumKit.h"

//==============================================================================
// Factory groove library: 10 genre banks x 10 patterns (100), preloaded into the
// preset library (banks 1-10). Banks 11-12 are left empty for user presets.
//
// Each groove declares a time signature + step rate; the step count is derived
// (see TimeGrid). This is what lets shuffles live on a triplet grid (4/4 @ 1/8T),
// ballads breathe in 12/8, waltzes in 3/4, etc. Step strings, one per lane:
//   'X' = accent, 'x' = normal hit, 'o' = ghost, '.' = off.
//
// Triplet/compound grids are 12 steps grouped 3+3+3+3 (beats on 0,3,6,9); the
// shuffle ride "x.xx.xx.xx.x" hits the downbeat + swung upbeat of each beat.
//
// Lane indices match LMOne::kVoiceDefs (0 Bass .. 11 Clap, 12 Open Hat).
//==============================================================================
namespace FactoryGrooves
{
    struct Groove { juce::String name; juce::String genre; int tempo; Pattern pattern; };

    inline Pattern makeGroove (int tsNum, int tsDen, int rate,
                               std::initializer_list<std::pair<int, const char*>> lanes)
    {
        Pattern p;
        p.clear();
        p.setMeter (tsNum, tsDen, rate);     // sets time sig + rate, derives numSteps
        p.numLanes = DrumKit::kNumVoices;

        for (const auto& ls : lanes)
        {
            const int   lane = ls.first;
            const char* s    = ls.second;
            if (lane < 0 || lane >= Pattern::kMaxLanes)
                continue;

            for (int i = 0; i < p.numSteps && s[i] != '\0'; ++i)
            {
                const char c = s[i];
                const juce::uint8 v = (c == 'X') ? (juce::uint8) 122
                                    : (c == 'x') ? (juce::uint8) 100
                                    : (c == 'o') ? (juce::uint8) 64
                                                 : (juce::uint8) 0;
                if (v > 0)
                    p.vel[(size_t) lane][(size_t) i] = v;
            }
        }
        return p;
    }

    inline std::array<std::array<Groove, 10>, 10> build()
    {
        enum { BASS = 0, SNR = 1, HAT = 2, CAB = 3, TMB = 4, TML = 5, TMH = 6,
               CGL = 7, CGH = 8, COW = 9, CLV = 10, CLP = 11, OHH = 12 };
        constexpr int R4  = TimeGrid::kQuarter,  R8  = TimeGrid::kEighth, R16 = TimeGrid::kSixteenth,
                      R8T = TimeGrid::kEighthT,   R16T = TimeGrid::kSixteenthT;
        juce::ignoreUnused (R4, R16T);

        std::array<std::array<Groove, 10>, 10> b;

        // === Bank 1: 80s POP =====================================================
        b[0][0] = { "Billie Jean", "80s Pop", 117, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[0][1] = { "Thriller",    "80s Pop", 118, makeGroove (4,4,R16, { {BASS,"x.....x.x......."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[0][2] = { "Synth Pop",   "80s Pop", 120, makeGroove (4,4,R16, { {BASS,"x...x...x...x..."},{CLP,"....x.......x..."},{OHH,"..x...x...x...x."} }) };
        b[0][3] = { "Gated Snare", "80s Pop", 110, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....X.......X..."},{HAT,"x...x...x...x..."} }) };
        b[0][4] = { "New Pop",     "80s Pop", 124, makeGroove (4,4,R16, { {BASS,"x..x..x...x..x.."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{CLP,"....x.......x..."} }) };
        b[0][5] = { "Power Ballad","80s Pop",  76, makeGroove (12,8,R8, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"x..x..x..x.."} }) };
        b[0][6] = { "Dance Pop",   "80s Pop", 120, makeGroove (4,4,R16, { {BASS,"x...x...x...x..."},{CLP,"....x.......x..."},{OHH,"..x...x...x...x."},{CAB,"xxxxxxxxxxxxxxxx"} }) };
        b[0][7] = { "Heartbeat",   "80s Pop", 103, makeGroove (4,4,R16, { {BASS,"x.......x...x..."},{SNR,"....x.......x..."},{TMB,"..x...x...x...x."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[0][8] = { "Pop Shuffle", "80s Pop", 104, makeGroove (4,4,R8T, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[0][9] = { "Uptempo Pop", "80s Pop", 128, makeGroove (4,4,R16, { {BASS,"x...x...x...x..."},{SNR,"....x.......x..."},{HAT,"xxxxxxxxxxxxxxxx"},{OHH,"..............x."} }) };

        // === Bank 2: 80s DANCE / NEW WAVE ========================================
        b[1][0] = { "Boogie",      "80s Dance", 112, makeGroove (4,4,R16, { {BASS,"x..x..x...x..x.."},{SNR,"....x.......x..."},{HAT,"xxxxxxxxxxxxxxxx"} }) };
        b[1][1] = { "Electro",     "80s Dance", 115, makeGroove (4,4,R16, { {BASS,"x.....x...x....."},{CLP,"....x.......x..."},{OHH,"..x...x...x...x."},{COW,"x...x...x...x..."} }) };
        b[1][2] = { "Freestyle",   "80s Dance", 110, makeGroove (4,4,R16, { {BASS,"x...x.x...x.x..."},{CLP,"....x.......x..."},{CGH,"..x...x...x...x."},{CGL,"x.....x...x....."} }) };
        b[1][3] = { "Funk Dance",  "80s Dance", 116, makeGroove (4,4,R16, { {BASS,"x..x..x..x..x..x"},{SNR,"....X..o....X..o"},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[1][4] = { "Boogie Down", "80s Dance", 114, makeGroove (4,4,R16, { {BASS,"x..x..x...x..x.."},{OHH,"..x...x...x...x."},{CAB,"xxxxxxxxxxxxxxxx"},{SNR,"....x.......x..."} }) };
        b[1][5] = { "New Wave",    "New Wave",  138, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[1][6] = { "Robotic",     "New Wave",  144, makeGroove (4,4,R16, { {BASS,"x...x...x...x..."},{SNR,"....x.......x..."},{HAT,"xxxxxxxxxxxxxxxx"} }) };
        b[1][7] = { "Skinny Tie",  "New Wave",  150, makeGroove (4,4,R16, { {BASS,"x.....x.x......."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[1][8] = { "Motorik",     "New Wave",  132, makeGroove (4,4,R16, { {BASS,"x.x.x.x.x.x.x.x."},{SNR,"....x.......x..."},{HAT,"..x...x...x...x."} }) };
        b[1][9] = { "Cold Wave",   "New Wave",  120, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....X.......X..."},{TMB,"..............x."} }) };

        // === Bank 3: FUNK ========================================================
        b[2][0] = { "Funk One",    "Funk", 100, makeGroove (4,4,R16, { {BASS,"x..x..x...x..x.."},{SNR,"....x.......x..."},{HAT,"xxxxxxxxxxxxxxxx"} }) };
        b[2][1] = { "Ghost Funk",  "Funk",  96, makeGroove (4,4,R16, { {BASS,"x.....x..x......"},{SNR,"..o.X..o..o.X..o"},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[2][2] = { "The One",     "Funk", 104, makeGroove (4,4,R16, { {BASS,"x.......x.x....."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{CAB,"..x...x...x...x."} }) };
        b[2][3] = { "Clav Funk",   "Funk", 108, makeGroove (4,4,R16, { {BASS,"x..x..x...x.x..."},{SNR,"....x.......x..."},{HAT,"x.xxx.xxx.xxx.xx"} }) };
        b[2][4] = { "Minneapolis", "Funk", 110, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....x.......x..."},{CLP,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[2][5] = { "P-Funk",      "Funk",  98, makeGroove (4,4,R16, { {BASS,"x..x..x...x..x.."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{CGL,"..x..x..x..x..x."} }) };
        b[2][6] = { "Bootsy",      "Funk",  92, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....x.......x..."},{HAT,"x...x...x...x..."} }) };
        b[2][7] = { "Tight Funk",  "Funk", 112, makeGroove (4,4,R16, { {BASS,"x..x...x.x..x..."},{SNR,"....x.......x..."},{HAT,"xxxxxxxxxxxxxxxx"} }) };
        b[2][8] = { "Funk Shuffle","Funk",  94, makeGroove (4,4,R8T, { {BASS,"x.....x..x.."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[2][9] = { "Slow Funk",   "Funk",  88, makeGroove (4,4,R16, { {BASS,"x.....x...x....."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{OHH,"..............x."} }) };

        // === Bank 4: R&B / GOSPEL ================================================
        b[3][0] = { "New Jack",    "R&B",    108, makeGroove (4,4,R16, { {BASS,"x..x...x.x..x..."},{SNR,"....X.......X..."},{HAT,"x.xxx.xxx.xxx.xx"} }) };
        b[3][1] = { "Quiet Storm", "R&B",     84, makeGroove (4,4,R16, { {BASS,"x.......x..x...."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[3][2] = { "Slow Jam",    "R&B",     72, makeGroove (12,8,R8, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"xxxxxxxxxxxx"} }) };
        b[3][3] = { "R&B Swing",   "R&B",     96, makeGroove (4,4,R8T, { {BASS,"x.....x..x.."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[3][4] = { "90s R&B",     "R&B",     92, makeGroove (4,4,R16, { {BASS,"x..x..x...x..x.."},{SNR,"....x.......x..."},{CLP,"....x.......x..."},{HAT,"xxxxxxxxxxxxxxxx"} }) };
        b[3][5] = { "Gospel",      "Gospel", 100, makeGroove (12,8,R8, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"},{TMB,"...x.....x.."} }) };
        b[3][6] = { "Praise Break","Gospel", 140, makeGroove (4,4,R16, { {BASS,"x..x..x...x..x.."},{SNR,"....x..x....x..x"},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[3][7] = { "Shout",       "Gospel", 152, makeGroove (12,8,R8, { {BASS,"x..x..x..x.."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[3][8] = { "Gospel Ballad","Gospel", 68, makeGroove (12,8,R8, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"x..x..x..x.."} }) };
        b[3][9] = { "Two-Beat",    "Gospel", 100, makeGroove (2,4,R16, { {BASS,"x...x..."},{SNR,"....X..."},{TMB,"x.x.x.x."} }) };

        // === Bank 5: HIP-HOP =====================================================
        b[4][0] = { "Boom Bap",    "Hip-Hop",  90, makeGroove (4,4,R16, { {BASS,"x......x.x......"},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[4][1] = { "Lo-Fi",       "Hip-Hop",  84, makeGroove (4,4,R16, { {BASS,"x.....x...x....."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[4][2] = { "Dilla",       "Hip-Hop",  88, makeGroove (4,4,R8T, { {BASS,"x.....x..x.."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[4][3] = { "Trap",        "Hip-Hop", 140, makeGroove (4,4,R16, { {BASS,"x.....x.....x..."},{SNR,"........x......."},{HAT,"x.x.xxx.x.x.xxxx"} }) };
        b[4][4] = { "West Coast",  "Hip-Hop",  94, makeGroove (4,4,R16, { {BASS,"x.......x...x..."},{SNR,"....x.......x..."},{COW,"..x...x...x...x."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[4][5] = { "Old School",  "Hip-Hop", 102, makeGroove (4,4,R16, { {BASS,"x...x..x.x...x.."},{SNR,"....x.......x..."},{CLP,"....x.......x..."},{COW,"x...x...x...x..."} }) };
        b[4][6] = { "Drill",       "Hip-Hop", 142, makeGroove (4,4,R16, { {BASS,"x....x....x..x.."},{SNR,"........x......."},{HAT,"x.xx.x.xx.x.xx.x"} }) };
        b[4][7] = { "Memphis",     "Hip-Hop", 138, makeGroove (4,4,R16, { {BASS,"x.....x...x....."},{SNR,"........x......."},{HAT,"xxxxxxxxxxxxxxxx"},{COW,"..............x."} }) };
        b[4][8] = { "East Coast",  "Hip-Hop",  92, makeGroove (4,4,R16, { {BASS,"X......x.x....x."},{SNR,"....X.......X..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[4][9] = { "Lo-Fi Loop",  "Hip-Hop",  80, makeGroove (4,4,R8, { {BASS,"x.....x."},{SNR,"....x..."},{HAT,"x.xxx.x."} }) };

        // === Bank 6: NEO-SOUL ====================================================
        b[5][0] = { "Brokenese",   "Neo-Soul", 88, makeGroove (4,4,R16, { {BASS,"x.....x..x......"},{SNR,"..o.x..o..o.x..o"},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[5][1] = { "Voodoo",      "Neo-Soul", 84, makeGroove (4,4,R16, { {BASS,"x.......x..x...."},{SNR,"....x.......x..."},{HAT,"x..x.x..x..x.x.."} }) };
        b[5][2] = { "Badu",        "Neo-Soul", 90, makeGroove (4,4,R8T, { {BASS,"x.....x..x.."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[5][3] = { "Questo",      "Neo-Soul", 82, makeGroove (4,4,R16, { {BASS,"x..............x"},{SNR,"........x......."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[5][4] = { "Deep Pocket", "Neo-Soul", 86, makeGroove (4,4,R16, { {BASS,"x.....x..x..x..."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[5][5] = { "Rhodes",      "Neo-Soul", 92, makeGroove (4,4,R16, { {BASS,"x..x..x...x..x.."},{SNR,"....x.......x..."},{HAT,"xxxxxxxxxxxxxxxx"} }) };
        b[5][6] = { "Neo Shuffle", "Neo-Soul", 94, makeGroove (4,4,R8T, { {BASS,"x..x..x..x.."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[5][7] = { "Slow Burn",   "Neo-Soul", 72, makeGroove (12,8,R8, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"x..x..x..x.."} }) };
        b[5][8] = { "Future Soul", "Neo-Soul", 96, makeGroove (4,4,R16, { {BASS,"x..x...x.x..x..."},{SNR,"....x.......x..."},{CLP,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[5][9] = { "Head Nod",    "Neo-Soul", 80, makeGroove (4,4,R16, { {BASS,"x.......x.x....."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };

        // === Bank 7: REGGAE / LATIN ==============================================
        b[6][0] = { "One Drop",    "Reggae", 72, makeGroove (4,4,R16, { {BASS,"........x......."},{SNR,"........x......."},{HAT,"x.x.x.x.x.x.x.x."},{OHH,"..x...x...x...x."} }) };
        b[6][1] = { "Steppers",    "Reggae", 76, makeGroove (4,4,R16, { {BASS,"x...x...x...x..."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{OHH,"..x...x...x...x."} }) };
        b[6][2] = { "Rockers",     "Reggae", 78, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{OHH,"..x...x...x...x."} }) };
        b[6][3] = { "Dancehall",   "Reggae", 94, makeGroove (4,4,R16, { {BASS,"x.....x.x......."},{SNR,"........x......."},{HAT,"x.x.x.x.x.x.x.x."},{OHH,"..x...x...x...x."} }) };
        b[6][4] = { "Dub",         "Reggae", 70, makeGroove (4,4,R16, { {BASS,"........x......."},{SNR,"........x......."},{HAT,"x.x.x.x.x.x.x.x."},{OHH,"..x...x...x...x."},{CLP,"............x..."} }) };
        b[6][5] = { "Bossa Nova",  "Latin", 130, makeGroove (4,4,R16, { {BASS,"x.......x......."},{CLV,"x..x..x...x.x..."},{CGH,"..x...x...x...x."} }) };
        b[6][6] = { "Songo",       "Latin", 104, makeGroove (4,4,R16, { {BASS,"x..x..x...x..x.."},{CGL,"x..x..x...x..x.."},{CGH,"..x.x...x.x...x."},{COW,"x.x.x.x.x.x.x.x."} }) };
        b[6][7] = { "Mambo",       "Latin", 110, makeGroove (4,4,R16, { {BASS,"x..x..x...x..x.."},{COW,"x..x.xx..x.xx.x."},{CGH,"..x...x...x...x."},{SNR,"....x.......x..."} }) };
        b[6][8] = { "Cha-Cha",     "Latin", 124, makeGroove (4,4,R16, { {COW,"x...x...x...x..."},{CGL,"..x...x...x...x."},{CLV,"x..x..x...x.x..."},{SNR,"............x.x."} }) };
        b[6][9] = { "Samba",       "Latin", 132, makeGroove (2,4,R16, { {BASS,"x...X..."},{TMB,"xxxxxxxx"},{CGH,"x.xx.x.x"},{CLV,"x..x..x."} }) };

        // === Bank 8: ROCK ========================================================
        b[7][0] = { "Basic Rock",  "Rock", 120, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[7][1] = { "Arena Rock",  "Rock", 128, makeGroove (4,4,R16, { {BASS,"x...x...x...x..."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{OHH,"..............x."} }) };
        b[7][2] = { "Half-Time",   "Rock",  92, makeGroove (4,4,R16, { {BASS,"x......x.x......"},{SNR,"........x......."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[7][3] = { "Tom Anthem",  "Rock",  96, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....X.......X..."},{TML,"x...x...x...x..."},{TMH,"..x...x...x...x."} }) };
        b[7][4] = { "Driving Rock","Rock", 138, makeGroove (4,4,R16, { {BASS,"x.....x.x.....x."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[7][5] = { "Stadium Stomp","Rock", 84, makeGroove (4,4,R16, { {BASS,"x.x.....x.x....."},{CLP,"....x.......x..."},{TML,"x.x.....x.x....."} }) };
        b[7][6] = { "Hard Rock",   "Rock", 144, makeGroove (4,4,R16, { {BASS,"x..x..x.x..x..x."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[7][7] = { "Rock Shuffle","Rock", 120, makeGroove (4,4,R8T, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[7][8] = { "Power Ballad","Rock",  76, makeGroove (12,8,R8, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"xxxxxxxxxxxx"} }) };
        b[7][9] = { "Punk",        "Rock", 170, makeGroove (4,4,R16, { {BASS,"x.x.x.x.x.x.x.x."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };

        // === Bank 9: BLUES =======================================================
        b[8][0] = { "Blues Shuffle","Blues",100, makeGroove (4,4,R8T, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[8][1] = { "Slow Blues",  "Blues",  60, makeGroove (12,8,R8, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"xxxxxxxxxxxx"} }) };
        b[8][2] = { "Texas Shuffle","Blues",120, makeGroove (4,4,R8T, { {BASS,"x..x..x..x.."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[8][3] = { "Chicago",     "Blues",  96, makeGroove (4,4,R8T, { {BASS,"x.....x....."},{SNR,"...X.....X.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[8][4] = { "Boogie Woogie","Blues",140, makeGroove (4,4,R8T, { {BASS,"x..x..x..x.."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[8][5] = { "Jump Blues",  "Blues", 160, makeGroove (4,4,R8T, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[8][6] = { "Delta",       "Blues",  80, makeGroove (4,4,R8T, { {BASS,"x.....x....."},{TMB,"...x.....x.."} }) };
        b[8][7] = { "Blues Rock",  "Blues", 110, makeGroove (4,4,R16, { {BASS,"x.....x.x......."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[8][8] = { "Slow Drag",   "Blues",  54, makeGroove (12,8,R8, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"x..x..x..x.."} }) };
        b[8][9] = { "Swamp",       "Blues",  88, makeGroove (4,4,R8T, { {BASS,"x..x..x..x.."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };

        // === Bank 10: FOLK =======================================================
        b[9][0] = { "Folk Pop",    "Folk", 108, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....x.......x..."},{TMB,"x.x.x.x.x.x.x.x."} }) };
        b[9][1] = { "Train Beat",  "Folk", 120, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"x.xXx.xXx.xXx.xX"} }) };
        b[9][2] = { "Brushes",     "Folk",  84, makeGroove (4,4,R8T, { {BASS,"x.....x....."},{SNR,"...x.....x.."},{HAT,"x.xx.xx.xx.x"} }) };
        b[9][3] = { "Campfire",    "Folk",  96, makeGroove (4,4,R16, { {BASS,"x.......x......."},{CLP,"....x.......x..."},{TMB,"x.x.x.x.x.x.x.x."} }) };
        b[9][4] = { "Americana",   "Folk", 104, makeGroove (4,4,R16, { {BASS,"x.......x...x..."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[9][5] = { "Folk Rock",   "Folk", 112, makeGroove (4,4,R16, { {BASS,"x.......x......."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{TMB,"..x...x...x...x."} }) };
        b[9][6] = { "Indie Folk",  "Folk", 110, makeGroove (4,4,R16, { {BASS,"x.......x......."},{TML,"x...x...x...x..."},{CLP,"....x.......x..."} }) };
        b[9][7] = { "Ballad",      "Folk",  72, makeGroove (6,8,R8, { {BASS,"x....."},{SNR,"...x.."},{TMB,"xxxxxx"} }) };
        b[9][8] = { "Stomp & Clap","Folk", 125, makeGroove (4,4,R16, { {BASS,"x...x...x...x..."},{CLP,"....x.......x..."},{TMB,"x.x.x.x.x.x.x.x."} }) };
        b[9][9] = { "Songwriter",  "Folk",  88, makeGroove (3,4,R16, { {BASS,"x..........."},{SNR,"....x...x..."},{TMB,"x.x.x.x.x.x."} }) };

        return b;
    }
}

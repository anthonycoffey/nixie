#pragma once

#include <JuceHeader.h>
#include <array>
#include <utility>
#include "Pattern.h"
#include "DrumKit.h"

//==============================================================================
// Factory groove library: 5 banks x 8 patterns (40), preloaded into the preset
// library (banks 1-5). Patterns are written as 16-char step strings per lane:
//   'X' = accent, 'x' = normal hit, 'o' = ghost, '.' = off.
//
// Lane indices match LMOne::kVoiceDefs (0 Bass .. 11 Clap, 12 Open Hat).
//==============================================================================
namespace FactoryGrooves
{
    struct Groove { juce::String name; int tempo; Pattern pattern; };

    inline Pattern makeGroove (std::initializer_list<std::pair<int, const char*>> lanes)
    {
        Pattern p;
        p.clear();
        p.numSteps = 16;
        p.numLanes = DrumKit::kNumVoices;

        for (const auto& ls : lanes)
        {
            const int   lane = ls.first;
            const char* s    = ls.second;
            if (lane < 0 || lane >= Pattern::kMaxLanes)
                continue;

            for (int i = 0; i < 16 && s[i] != '\0'; ++i)
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

    inline std::array<std::array<Groove, 8>, 5> build()
    {
        enum { BASS = 0, SNR = 1, HAT = 2, CAB = 3, TMB = 4, TML = 5, TMH = 6,
               CGL = 7, CGH = 8, COW = 9, CLV = 10, CLP = 11, OHH = 12 };

        std::array<std::array<Groove, 8>, 5> b;

        // --- Bank 1: Rock & Pop --------------------------------------------------
        b[0][0] = { "Basic Rock",   120, makeGroove ({ {BASS,"x.......x......."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[0][1] = { "Driving Rock", 128, makeGroove ({ {BASS,"x.....x.x.....x."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[0][2] = { "Half-Time",     92, makeGroove ({ {BASS,"x..............."},{SNR,"........x......."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[0][3] = { "Pop 4-Floor",  118, makeGroove ({ {BASS,"x...x...x...x..."},{SNR,"....x.......x..."},{HAT,"..x...x...x...x."},{CLP,"....x.......x..."} }) };
        b[0][4] = { "Tom Beat",     110, makeGroove ({ {BASS,"x.......x......."},{SNR,"....x.......x..."},{TML,".............x.."},{TMH,"..............x."} }) };
        b[0][5] = { "Stadium",      132, makeGroove ({ {BASS,"x...x...x...x..."},{CLP,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[0][6] = { "Anthem",       100, makeGroove ({ {BASS,"x.......x...x..."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{OHH,"..............x."} }) };
        b[0][7] = { "Ballad",        76, makeGroove ({ {BASS,"x.......x......."},{SNR,"....x.......x..."},{HAT,"x...x...x...x..."} }) };

        // --- Bank 2: Funk & Soul -------------------------------------------------
        b[1][0] = { "Funk One",     100, makeGroove ({ {BASS,"x..x..x...x..x.."},{SNR,"....x.......x..."},{HAT,"xxxxxxxxxxxxxxxx"} }) };
        b[1][1] = { "Ghost Funk",    96, makeGroove ({ {BASS,"x.....x..x......"},{SNR,"....X..o....X..o"},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[1][2] = { "Soul",          92, makeGroove ({ {BASS,"x.......x..x...."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{TMB,"..x...x...x...x."} }) };
        b[1][3] = { "16th Funk",    104, makeGroove ({ {BASS,"x..x...x..x..x.."},{SNR,"....x.......x..."},{HAT,"xxxxxxxxxxxxxxxx"},{OHH,"..............x."} }) };
        b[1][4] = { "Clav Funk",    108, makeGroove ({ {BASS,"x..x..x...x.x..."},{SNR,"....x.......x..."},{HAT,"x.xxx.xxx.xxx.xx"} }) };
        b[1][5] = { "Second Line",   95, makeGroove ({ {BASS,"x..x..x..x..x..x"},{SNR,"....x..x....x..x"},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[1][6] = { "Disco Funk",   116, makeGroove ({ {BASS,"x...x...x...x..."},{SNR,"....x.......x..."},{OHH,"..x...x...x...x."} }) };
        b[1][7] = { "Slow Jam",      84, makeGroove ({ {BASS,"x.......x......."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{CLP,"............x..."} }) };

        // --- Bank 3: Hip-Hop -----------------------------------------------------
        b[2][0] = { "Boom Bap",      90, makeGroove ({ {BASS,"x......x.x......"},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[2][1] = { "Lo-Fi",         84, makeGroove ({ {BASS,"x.....x...x....."},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[2][2] = { "Trap",         140, makeGroove ({ {BASS,"x.....x.....x..."},{SNR,"........x......."},{HAT,"x.x.xxx.x.x.xxxx"} }) };
        b[2][3] = { "West Coast",    94, makeGroove ({ {BASS,"x.......x...x..."},{SNR,"....x.......x..."},{COW,"..x...x...x...x."} }) };
        b[2][4] = { "Drill",        142, makeGroove ({ {BASS,"x....x....x..x.."},{SNR,"........x......."},{HAT,"x.xx.x.xx.x.xx.x"} }) };
        b[2][5] = { "Old School",   102, makeGroove ({ {BASS,"x...x..x.x...x.."},{SNR,"....x.......x..."},{CLP,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[2][6] = { "Swung",         88, makeGroove ({ {BASS,"x.....x..x......"},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[2][7] = { "Cloud",         76, makeGroove ({ {BASS,"x.......x......."},{SNR,"........x......."},{HAT,"x.x.x.x.x.x.x.x."} }) };

        // --- Bank 4: House & Techno ---------------------------------------------
        b[3][0] = { "Classic House",124, makeGroove ({ {BASS,"x...x...x...x..."},{CLP,"....x.......x..."},{OHH,"..x...x...x...x."},{HAT,"xxxxxxxxxxxxxxxx"} }) };
        b[3][1] = { "Deep House",   122, makeGroove ({ {BASS,"x...x...x...x..."},{SNR,"....x.......x..."},{OHH,"..x...x...x...x."} }) };
        b[3][2] = { "Techno",       130, makeGroove ({ {BASS,"x...x...x...x..."},{HAT,"..x...x...x...x."},{CLP,"....x.......x..."} }) };
        b[3][3] = { "Tech House",   126, makeGroove ({ {BASS,"x...x...x...x..."},{OHH,"..x...x...x...x."},{CLP,"....x.......x..."},{CAB,"xxxxxxxxxxxxxxxx"} }) };
        b[3][4] = { "Acid",         128, makeGroove ({ {BASS,"x...x...x...x..."},{HAT,"x.x.x.x.x.x.x.x."},{CLP,"....x.......x..."} }) };
        b[3][5] = { "Garage",       132, makeGroove ({ {BASS,"x.....x.x.....x."},{SNR,"....x.......x..."},{HAT,"xxxxxxxxxxxxxxxx"} }) };
        b[3][6] = { "Minimal",      125, makeGroove ({ {BASS,"x...x...x...x..."},{HAT,"..x...x...x...x."},{CLV,"x.......x......."} }) };
        b[3][7] = { "Trance",       138, makeGroove ({ {BASS,"x...x...x...x..."},{OHH,"..x...x...x...x."},{CLP,"....x.......x..."} }) };

        // --- Bank 5: Latin / Disco / World --------------------------------------
        b[4][0] = { "Disco",        120, makeGroove ({ {BASS,"x...x...x...x..."},{SNR,"....x.......x..."},{OHH,"..x...x...x...x."},{HAT,"x.x.x.x.x.x.x.x."} }) };
        b[4][1] = { "Bossa",        130, makeGroove ({ {BASS,"x.....x.x.....x."},{CLV,"x..x..x.x..x..x."},{CGH,"..x...x...x...x."} }) };
        b[4][2] = { "Songo",        104, makeGroove ({ {BASS,"x..x..x...x..x.."},{CGL,"x..x..x...x..x.."},{CGH,"..x.x...x.x...x."},{COW,"x...x...x...x..."} }) };
        b[4][3] = { "Cha-Cha",      124, makeGroove ({ {COW,"x...x...x...x..."},{CGL,"..x...x...x...x."},{CLV,"x..x..x.x..x..x."},{SNR,"....x.......x..."} }) };
        b[4][4] = { "Mambo",        110, makeGroove ({ {BASS,"x..x..x...x..x.."},{COW,"x.x.x.x.x.x.x.x."},{CGH,"..x...x...x...x."} }) };
        b[4][5] = { "Afrobeat",     112, makeGroove ({ {BASS,"x..x..x..x..x..x"},{SNR,"....x.......x..."},{HAT,"x.x.x.x.x.x.x.x."},{CGL,"..x..x..x..x..x."} }) };
        b[4][6] = { "Reggae",        80, makeGroove ({ {BASS,"........x......."},{SNR,"........x......."},{HAT,"..x...x...x...x."} }) };
        b[4][7] = { "Samba",        132, makeGroove ({ {BASS,"x..x..x...x..x.."},{TMB,"xxxxxxxxxxxxxxxx"},{CGH,"..x.x.x...x.x.x."},{CLV,"x..x..x.x..x..x."} }) };

        return b;
    }
}

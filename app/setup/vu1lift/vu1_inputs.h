#pragma once
// Where DBZIW's translated VU1 programs sit in the player's own executable (SLES_553.47): offsets, sizes and checksums
// only, never instruction bytes (issue 012; DW5 025 Step 6's format). A program's entries come from
// `dbziw_vu1lift --find-range <census image> <entry pc> <name> [disc files]` on a PS2X_VU1_CENSUS image;
// notes/issues/012-*/port/make_inputs.py assembles this table from a census (one target per distinct reachable code).
#include <array>
#include <cstdint>

namespace dbziw::vu1lift {

struct Chunk
{
    uint32_t pc, offset, size;
    const char *source; // nullptr: the executable
};
struct Range
{
    const char *key;
    uint32_t start, end;
    const char *name;
    const Chunk *chunks;
    uint32_t chunkCount;
    const char *sha256;
};
struct ImageRegistration
{
    uint64_t hash;
    uint32_t pc;
    const char *name;
};
struct Target
{
    const char *key;
    uint32_t pc, end;
    const char *name;
};
struct SourceFile
{
    const char *name;
    const char *sha256;
};

inline constexpr const char *kElfSha256 = "afe8c7b99f31e635750a17618ae8248d1a64fe3ad0669996990abdf33e1819f4";
// Disc files other than the executable that hold VU1 code: none for DBZIW (012 M0: every program is in SLES_553.47).
inline constexpr std::array<SourceFile, 0> kSourceFiles{};
// DW5 kept whole-image registrations it had observed; DBZIW registers programs by their reachable code only.
inline constexpr std::array<ImageRegistration, 0> kImageRegistrations{};
// BEGIN GENERATED (make_inputs.py)
inline constexpr Chunk kChunks0[] = {{288u, 2815080u, 600u, nullptr}, {1488u, 2769176u, 568u, nullptr}, {2056u, 2816856u, 2040u, nullptr}, {4096u, 2635848u, 240u, nullptr}};
inline constexpr Chunk kChunks1[] = {{1016u, 3046400u, 1032u, nullptr}, {2048u, 3047440u, 2048u, nullptr}, {4096u, 2662088u, 472u, nullptr}};
inline constexpr Chunk kChunks2[] = {{64u, 2814856u, 224u, nullptr}, {312u, 2815104u, 312u, nullptr}, {664u, 2815456u, 560u, nullptr}, {1264u, 2816056u, 784u, nullptr}, {2048u, 2816848u, 2048u, nullptr}, {4096u, 2635848u, 240u, nullptr}};
inline constexpr Chunk kChunks3[] = {{304u, 3045688u, 704u, nullptr}, {1712u, 2643176u, 504u, nullptr}, {2216u, 3047608u, 1880u, nullptr}, {4096u, 2662088u, 472u, nullptr}};
inline constexpr Chunk kChunks4[] = {{936u, 2642568u, 1112u, nullptr}, {2048u, 2643688u, 2048u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks5[] = {{64u, 3045448u, 240u, nullptr}, {328u, 3045712u, 416u, nullptr}, {784u, 3046168u, 656u, nullptr}, {1480u, 3046864u, 568u, nullptr}, {2048u, 3047440u, 2048u, nullptr}, {4096u, 2662088u, 472u, nullptr}};
inline constexpr Chunk kChunks6[] = {{304u, 2641936u, 624u, nullptr}, {1544u, 2643176u, 504u, nullptr}, {2048u, 2643688u, 2048u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks7[] = {{880u, 2801592u, 1168u, nullptr}, {2048u, 2802768u, 2048u, nullptr}, {4096u, 2635872u, 216u, nullptr}};
inline constexpr Chunk kChunks8[] = {{424u, 3074400u, 600u, nullptr}, {1632u, 2769176u, 568u, nullptr}, {2200u, 3076184u, 1896u, nullptr}, {4096u, 2662104u, 456u, nullptr}};
inline constexpr Chunk kChunks9[] = {{64u, 2641696u, 240u, nullptr}, {328u, 2641960u, 336u, nullptr}, {704u, 2642336u, 576u, nullptr}, {1320u, 2642952u, 728u, nullptr}, {2048u, 2643688u, 2048u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks10[] = {{288u, 2801000u, 584u, nullptr}, {1464u, 2754888u, 600u, nullptr}, {2064u, 2802784u, 2032u, nullptr}, {4096u, 2635872u, 216u, nullptr}};
inline constexpr Chunk kChunks11[] = {{880u, 3065272u, 1168u, nullptr}, {2048u, 3066448u, 2048u, nullptr}, {4096u, 2635864u, 224u, nullptr}};
inline constexpr Chunk kChunks12[] = {{64u, 2800776u, 224u, nullptr}, {312u, 2801024u, 296u, nullptr}, {648u, 2801360u, 552u, nullptr}, {1240u, 2801952u, 808u, nullptr}, {2048u, 2802768u, 2048u, nullptr}, {4096u, 2635872u, 216u, nullptr}};
inline constexpr Chunk kChunks13[] = {{0u, 2641632u, 64u, nullptr}, {4376u, 2646024u, 456u, nullptr}};
inline constexpr Chunk kChunks14[] = {{0u, 2729616u, 64u, nullptr}, {4312u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks15[] = {{64u, 2810296u, 224u, nullptr}, {312u, 2810544u, 296u, nullptr}, {648u, 2810880u, 520u, nullptr}, {1208u, 2811440u, 840u, nullptr}, {2048u, 2812288u, 2048u, nullptr}};
inline constexpr Chunk kChunks16[] = {{64u, 3074040u, 360u, nullptr}, {440u, 3074416u, 320u, nullptr}, {800u, 3074776u, 568u, nullptr}, {1408u, 3075384u, 640u, nullptr}, {2048u, 3076032u, 2048u, nullptr}, {4096u, 2662104u, 456u, nullptr}};
inline constexpr Chunk kChunks17[] = {{896u, 2815688u, 1152u, nullptr}, {2048u, 2816848u, 2048u, nullptr}, {4096u, 2635848u, 240u, nullptr}};
inline constexpr Chunk kChunks18[] = {{288u, 3064680u, 584u, nullptr}, {1464u, 2754888u, 600u, nullptr}, {2064u, 3066464u, 2032u, nullptr}, {4096u, 2635864u, 224u, nullptr}};
inline constexpr Chunk kChunks19[] = {{0u, 2758240u, 64u, nullptr}, {4336u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks20[] = {{64u, 2800776u, 224u, nullptr}, {312u, 3064704u, 296u, nullptr}, {648u, 3065040u, 552u, nullptr}, {1240u, 3065632u, 808u, nullptr}, {2048u, 3066448u, 2048u, nullptr}, {4096u, 2635864u, 224u, nullptr}};
inline constexpr Chunk kChunks21[] = {{288u, 2805768u, 584u, nullptr}, {1464u, 2759680u, 608u, nullptr}, {2072u, 2807560u, 2024u, nullptr}, {4096u, 2635888u, 200u, nullptr}};
inline constexpr Chunk kChunks22[] = {{288u, 3069464u, 584u, nullptr}, {1464u, 2759680u, 608u, nullptr}, {2072u, 3071256u, 2024u, nullptr}, {4096u, 2635840u, 248u, nullptr}};
inline constexpr Chunk kChunks23[] = {{0u, 2805480u, 64u, nullptr}, {4296u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks24[] = {{880u, 2811112u, 1168u, nullptr}, {2048u, 2812288u, 2048u, nullptr}};
inline constexpr Chunk kChunks25[] = {{0u, 3045384u, 64u, nullptr}, {4568u, 3040056u, 608u, nullptr}};
inline constexpr Chunk kChunks26[] = {{1032u, 3075008u, 1016u, nullptr}, {2048u, 3076032u, 2048u, nullptr}, {4096u, 2662104u, 456u, nullptr}};
inline constexpr Chunk kChunks27[] = {{64u, 2800776u, 224u, nullptr}, {312u, 2805792u, 296u, nullptr}, {648u, 2806128u, 552u, nullptr}, {1240u, 2806720u, 808u, nullptr}, {2048u, 2807536u, 2048u, nullptr}, {4096u, 2635888u, 200u, nullptr}};
inline constexpr Chunk kChunks28[] = {{0u, 3064392u, 64u, nullptr}, {4320u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks29[] = {{64u, 2800776u, 224u, nullptr}, {312u, 3069488u, 296u, nullptr}, {648u, 3069824u, 552u, nullptr}, {1240u, 3070416u, 808u, nullptr}, {2048u, 3071232u, 2048u, nullptr}, {4096u, 2635840u, 248u, nullptr}};
inline constexpr Chunk kChunks30[] = {{0u, 2753440u, 64u, nullptr}, {4344u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks31[] = {{880u, 2806360u, 1168u, nullptr}, {2048u, 2807536u, 2048u, nullptr}, {4096u, 2635888u, 200u, nullptr}};
inline constexpr Chunk kChunks32[] = {{880u, 3070056u, 1168u, nullptr}, {2048u, 3071232u, 2048u, nullptr}, {4096u, 2635840u, 248u, nullptr}};
inline constexpr Chunk kChunks33[] = {{0u, 2705312u, 64u, nullptr}, {4096u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks34[] = {{0u, 3073976u, 64u, nullptr}, {4552u, 3078544u, 448u, nullptr}};
inline constexpr Chunk kChunks35[] = {{0u, 2791608u, 64u, nullptr}, {4080u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks36[] = {{64u, 2791672u, 224u, nullptr}, {312u, 2791920u, 280u, nullptr}, {632u, 2792240u, 520u, nullptr}, {1192u, 2792800u, 856u, nullptr}, {2048u, 2793664u, 2032u, nullptr}};
inline constexpr Chunk kChunks37[] = {{64u, 3055352u, 224u, nullptr}, {312u, 3055600u, 272u, nullptr}, {624u, 3055912u, 512u, nullptr}, {1176u, 3056464u, 872u, nullptr}, {2048u, 3057344u, 2048u, nullptr}};
inline constexpr Chunk kChunks38[] = {{0u, 3089016u, 64u, nullptr}, {4408u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks39[] = {{928u, 3089944u, 1120u, nullptr}, {2048u, 3091072u, 2048u, nullptr}, {4096u, 2635776u, 312u, nullptr}};
inline constexpr Chunk kChunks40[] = {{0u, 2651616u, 64u, nullptr}, {1088u, 2652704u, 448u, nullptr}};
inline constexpr Chunk kChunks41[] = {{64u, 3089080u, 240u, nullptr}, {328u, 3089344u, 328u, nullptr}, {696u, 3089712u, 576u, nullptr}, {1312u, 3090328u, 736u, nullptr}, {2048u, 3091072u, 2048u, nullptr}, {4096u, 2635776u, 312u, nullptr}};
inline constexpr Chunk kChunks42[] = {{64u, 3027792u, 176u, nullptr}, {272u, 3029568u, 280u, nullptr}, {592u, 3029888u, 424u, nullptr}, {1056u, 3030352u, 32u, nullptr}};
inline constexpr Chunk kChunks43[] = {{0u, 2631584u, 64u, nullptr}, {4488u, 2636088u, 520u, nullptr}};
inline constexpr Chunk kChunks44[] = {{64u, 2631648u, 240u, nullptr}, {328u, 2631912u, 392u, nullptr}, {760u, 2632344u, 632u, nullptr}, {1432u, 2638000u, 704u, nullptr}, {2136u, 2638712u, 2048u, nullptr}, {4184u, 2635784u, 304u, nullptr}};
inline constexpr Chunk kChunks45[] = {{304u, 2631888u, 680u, nullptr}, {1656u, 2643176u, 504u, nullptr}, {2160u, 2638736u, 2024u, nullptr}, {4184u, 2635784u, 304u, nullptr}};
inline constexpr Chunk kChunks46[] = {{992u, 2632576u, 1056u, nullptr}, {2048u, 2633640u, 2048u, nullptr}, {4096u, 2635696u, 392u, nullptr}};
inline constexpr Chunk kChunks47[] = {{0u, 2636656u, 64u, nullptr}, {4400u, 2641072u, 520u, nullptr}};
inline constexpr Chunk kChunks48[] = {{64u, 2636720u, 224u, nullptr}, {312u, 2636968u, 360u, nullptr}, {712u, 2637368u, 592u, nullptr}, {1344u, 2638000u, 704u, nullptr}, {2048u, 2638712u, 2048u, nullptr}, {4096u, 2635784u, 304u, nullptr}};
inline constexpr Chunk kChunks49[] = {{288u, 2636944u, 648u, nullptr}, {1568u, 2643176u, 504u, nullptr}, {2072u, 2638736u, 2024u, nullptr}, {4096u, 2635784u, 304u, nullptr}};
inline constexpr Chunk kChunks50[] = {{944u, 2637600u, 1104u, nullptr}, {2048u, 2638712u, 2048u, nullptr}, {4096u, 2635784u, 304u, nullptr}};
inline constexpr Chunk kChunks51[] = {{0u, 2646528u, 64u, nullptr}, {4528u, 2641072u, 520u, nullptr}};
inline constexpr Chunk kChunks52[] = {{64u, 2646592u, 256u, nullptr}, {344u, 2646872u, 400u, nullptr}, {784u, 2647312u, 648u, nullptr}, {1472u, 2638000u, 704u, nullptr}, {2176u, 2648712u, 1920u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks53[] = {{320u, 2646848u, 688u, nullptr}, {1696u, 2643176u, 504u, nullptr}, {2200u, 2648736u, 1896u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks54[] = {{1016u, 2647544u, 1032u, nullptr}, {2048u, 2648584u, 2048u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks55[] = {{64u, 2651680u, 176u, nullptr}, {272u, 2651888u, 280u, nullptr}, {592u, 2652208u, 424u, nullptr}, {1056u, 2652208u, 32u, nullptr}};
inline constexpr Chunk kChunks56[] = {{240u, 2651856u, 384u, nullptr}};
inline constexpr Chunk kChunks57[] = {{632u, 2652248u, 456u, nullptr}};
inline constexpr Chunk kChunks58[] = {{64u, 2651680u, 176u, nullptr}, {272u, 2653456u, 280u, nullptr}, {592u, 2653776u, 424u, nullptr}, {1056u, 2652208u, 32u, nullptr}};
inline constexpr Chunk kChunks59[] = {{0u, 2654752u, 64u, nullptr}, {1128u, 2652704u, 448u, nullptr}};
inline constexpr Chunk kChunks60[] = {{64u, 2654816u, 160u, nullptr}, {256u, 2655008u, 312u, nullptr}, {608u, 2655360u, 448u, nullptr}, {1096u, 2655360u, 32u, nullptr}};
inline constexpr Chunk kChunks61[] = {{224u, 2654976u, 416u, nullptr}};
inline constexpr Chunk kChunks62[] = {{648u, 2655400u, 480u, nullptr}};
inline constexpr Chunk kChunks63[] = {{64u, 2654816u, 160u, nullptr}, {256u, 2656608u, 312u, nullptr}, {608u, 2656960u, 448u, nullptr}, {1096u, 2655360u, 32u, nullptr}};
inline constexpr Chunk kChunks64[] = {{0u, 2657952u, 64u, nullptr}, {4592u, 2641072u, 520u, nullptr}};
inline constexpr Chunk kChunks65[] = {{64u, 2658016u, 256u, nullptr}, {344u, 2658296u, 432u, nullptr}, {816u, 2658768u, 672u, nullptr}, {1528u, 2659480u, 520u, nullptr}, {2048u, 2660008u, 2048u, nullptr}, {4096u, 2662064u, 496u, nullptr}};
inline constexpr Chunk kChunks66[] = {{320u, 2658272u, 720u, nullptr}, {1752u, 2643176u, 504u, nullptr}, {2256u, 2660216u, 1840u, nullptr}, {4096u, 2662064u, 496u, nullptr}};
inline constexpr Chunk kChunks67[] = {{1048u, 2659000u, 1000u, nullptr}, {2048u, 2660008u, 2048u, nullptr}, {4096u, 2662064u, 496u, nullptr}};
inline constexpr Chunk kChunks68[] = {{0u, 2641632u, 64u, nullptr}, {4376u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks69[] = {{64u, 2663184u, 256u, nullptr}, {344u, 2663464u, 408u, nullptr}, {792u, 2663912u, 648u, nullptr}, {1480u, 2664600u, 568u, nullptr}, {2048u, 2665176u, 2048u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks70[] = {{320u, 2663440u, 696u, nullptr}, {1704u, 3037360u, 360u, nullptr}, {2064u, 2665192u, 2032u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks71[] = {{1024u, 2664144u, 1024u, nullptr}, {2048u, 2665176u, 2048u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks72[] = {{0u, 2667952u, 64u, nullptr}, {4504u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks73[] = {{64u, 2668016u, 256u, nullptr}, {344u, 2668296u, 384u, nullptr}, {768u, 2668720u, 624u, nullptr}, {1432u, 2669384u, 616u, nullptr}, {2048u, 2670008u, 2048u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks74[] = {{320u, 2668272u, 672u, nullptr}, {1656u, 2643176u, 504u, nullptr}, {2160u, 2670120u, 1936u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks75[] = {{1000u, 2668952u, 1048u, nullptr}, {2048u, 2670008u, 2048u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks76[] = {{64u, 2663184u, 256u, nullptr}, {344u, 2673256u, 408u, nullptr}, {792u, 2673704u, 648u, nullptr}, {1480u, 2664600u, 568u, nullptr}, {2048u, 2674968u, 2048u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks77[] = {{320u, 2673232u, 696u, nullptr}, {1704u, 3037360u, 360u, nullptr}, {2064u, 2674984u, 2032u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks78[] = {{1024u, 2673936u, 1024u, nullptr}, {2048u, 2674968u, 2048u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks79[] = {{0u, 2677760u, 64u, nullptr}, {4368u, 2682144u, 416u, nullptr}};
inline constexpr Chunk kChunks80[] = {{64u, 2677824u, 256u, nullptr}, {344u, 2678104u, 416u, nullptr}, {800u, 2678560u, 648u, nullptr}, {1488u, 2679248u, 560u, nullptr}, {2048u, 2679816u, 2048u, nullptr}, {4096u, 2635816u, 272u, nullptr}};
inline constexpr Chunk kChunks81[] = {{320u, 2678080u, 704u, nullptr}, {1712u, 3037360u, 360u, nullptr}, {2072u, 2679840u, 2024u, nullptr}, {4096u, 2635816u, 272u, nullptr}};
inline constexpr Chunk kChunks82[] = {{1032u, 2678792u, 1016u, nullptr}, {2048u, 2679816u, 2048u, nullptr}, {4096u, 2635816u, 272u, nullptr}};
inline constexpr Chunk kChunks83[] = {{0u, 2682608u, 64u, nullptr}, {4536u, 2682144u, 416u, nullptr}};
inline constexpr Chunk kChunks84[] = {{64u, 2682672u, 256u, nullptr}, {344u, 2682952u, 384u, nullptr}, {768u, 2683376u, 624u, nullptr}, {1432u, 2684040u, 616u, nullptr}, {2048u, 2684664u, 2048u, nullptr}, {4096u, 2662120u, 440u, nullptr}};
inline constexpr Chunk kChunks85[] = {{320u, 2682928u, 672u, nullptr}, {1656u, 2754888u, 600u, nullptr}, {2256u, 2684872u, 1840u, nullptr}, {4096u, 2662120u, 440u, nullptr}};
inline constexpr Chunk kChunks86[] = {{1000u, 2683608u, 1048u, nullptr}, {2048u, 2684664u, 2048u, nullptr}, {4096u, 2662120u, 440u, nullptr}};
inline constexpr Chunk kChunks87[] = {{0u, 2687600u, 64u, nullptr}, {4424u, 2682144u, 416u, nullptr}};
inline constexpr Chunk kChunks88[] = {{64u, 2677824u, 256u, nullptr}, {344u, 2687944u, 416u, nullptr}, {800u, 2688400u, 656u, nullptr}, {1496u, 2788216u, 704u, nullptr}, {2200u, 2689808u, 1896u, nullptr}, {4096u, 2635760u, 328u, nullptr}};
inline constexpr Chunk kChunks89[] = {{320u, 2687920u, 704u, nullptr}, {1720u, 3061232u, 664u, nullptr}, {2384u, 2689992u, 1712u, nullptr}, {4096u, 2635760u, 328u, nullptr}};
inline constexpr Chunk kChunks90[] = {{1032u, 2688632u, 1016u, nullptr}, {2048u, 2689656u, 2048u, nullptr}, {4096u, 2635760u, 328u, nullptr}};
inline constexpr Chunk kChunks91[] = {{0u, 2692480u, 64u, nullptr}, {472u, 2652704u, 448u, nullptr}};
inline constexpr Chunk kChunks92[] = {{64u, 2692544u, 104u, nullptr}, {200u, 2692680u, 232u, nullptr}};
inline constexpr Chunk kChunks93[] = {{168u, 2692648u, 304u, nullptr}};
inline constexpr Chunk kChunks94[] = {{0u, 2693424u, 64u, nullptr}, {528u, 2652704u, 448u, nullptr}};
inline constexpr Chunk kChunks95[] = {{64u, 2693488u, 120u, nullptr}, {216u, 2693640u, 272u, nullptr}};
inline constexpr Chunk kChunks96[] = {{184u, 2693608u, 344u, nullptr}};
inline constexpr Chunk kChunks97[] = {{64u, 2692544u, 104u, nullptr}, {200u, 2694632u, 232u, nullptr}};
inline constexpr Chunk kChunks98[] = {{64u, 2693488u, 120u, nullptr}, {216u, 2695592u, 272u, nullptr}};
inline constexpr Chunk kChunks99[] = {{0u, 2696384u, 64u, nullptr}, {4008u, 2709424u, 416u, nullptr}};
inline constexpr Chunk kChunks100[] = {{64u, 2696448u, 208u, nullptr}, {288u, 2696672u, 272u, nullptr}, {600u, 2696984u, 680u, nullptr}, {1320u, 2697704u, 728u, nullptr}, {2048u, 2698440u, 1960u, nullptr}};
inline constexpr Chunk kChunks101[] = {{272u, 2696656u, 664u, nullptr}, {1680u, 2698064u, 368u, nullptr}, {2048u, 2698440u, 1960u, nullptr}};
inline constexpr Chunk kChunks102[] = {{944u, 2697328u, 1104u, nullptr}, {2048u, 2698440u, 1960u, nullptr}};
inline constexpr Chunk kChunks103[] = {{64u, 2696448u, 208u, nullptr}, {288u, 2701136u, 272u, nullptr}, {600u, 2701448u, 680u, nullptr}, {1320u, 2702168u, 728u, nullptr}, {2048u, 2702904u, 1960u, nullptr}};
inline constexpr Chunk kChunks104[] = {{272u, 2701120u, 664u, nullptr}, {1680u, 2698064u, 368u, nullptr}, {2048u, 2702904u, 1960u, nullptr}};
inline constexpr Chunk kChunks105[] = {{944u, 2701792u, 1104u, nullptr}, {2048u, 2702904u, 1960u, nullptr}};
inline constexpr Chunk kChunks106[] = {{0u, 2705312u, 64u, nullptr}, {4096u, 2709424u, 416u, nullptr}};
inline constexpr Chunk kChunks107[] = {{64u, 2705376u, 208u, nullptr}, {288u, 2705600u, 296u, nullptr}, {624u, 2705936u, 704u, nullptr}, {1368u, 2706680u, 680u, nullptr}, {2048u, 2707368u, 2048u, nullptr}};
inline constexpr Chunk kChunks108[] = {{272u, 2705584u, 688u, nullptr}, {1728u, 2698064u, 368u, nullptr}, {2096u, 2707416u, 2000u, nullptr}};
inline constexpr Chunk kChunks109[] = {{968u, 2706280u, 1080u, nullptr}, {2048u, 2707368u, 2048u, nullptr}};
inline constexpr Chunk kChunks110[] = {{64u, 2705376u, 208u, nullptr}, {288u, 2710160u, 296u, nullptr}, {624u, 2710496u, 704u, nullptr}, {1368u, 2711240u, 680u, nullptr}, {2048u, 2711928u, 2048u, nullptr}};
inline constexpr Chunk kChunks111[] = {{272u, 2710144u, 688u, nullptr}, {1728u, 2698064u, 368u, nullptr}, {2096u, 2711976u, 2000u, nullptr}};
inline constexpr Chunk kChunks112[] = {{968u, 2710840u, 1080u, nullptr}, {2048u, 2711928u, 2048u, nullptr}};
inline constexpr Chunk kChunks113[] = {{0u, 2657952u, 64u, nullptr}, {4592u, 2636088u, 520u, nullptr}};
inline constexpr Chunk kChunks114[] = {{64u, 2714512u, 240u, nullptr}, {328u, 2714776u, 416u, nullptr}, {784u, 2715232u, 656u, nullptr}, {1480u, 3105128u, 576u, nullptr}, {2056u, 2716512u, 2040u, nullptr}, {4096u, 2662064u, 496u, nullptr}};
inline constexpr Chunk kChunks115[] = {{304u, 2714752u, 704u, nullptr}, {1704u, 2754888u, 600u, nullptr}, {2304u, 2716760u, 1792u, nullptr}, {4096u, 2662064u, 496u, nullptr}};
inline constexpr Chunk kChunks116[] = {{1016u, 2715464u, 1032u, nullptr}, {2048u, 2716504u, 2048u, nullptr}, {4096u, 2662064u, 496u, nullptr}};
inline constexpr Chunk kChunks117[] = {{0u, 2719616u, 64u, nullptr}, {4456u, 2641072u, 520u, nullptr}};
inline constexpr Chunk kChunks118[] = {{64u, 2719680u, 224u, nullptr}, {312u, 2719928u, 360u, nullptr}, {712u, 2720328u, 584u, nullptr}, {1336u, 2720952u, 712u, nullptr}, {2048u, 2721672u, 2048u, nullptr}, {4096u, 2635728u, 360u, nullptr}};
inline constexpr Chunk kChunks119[] = {{288u, 2719904u, 648u, nullptr}, {1560u, 2754888u, 600u, nullptr}, {2160u, 2721784u, 1936u, nullptr}, {4096u, 2635728u, 360u, nullptr}};
inline constexpr Chunk kChunks120[] = {{944u, 2720560u, 1104u, nullptr}, {2048u, 2721672u, 2048u, nullptr}, {4096u, 2635728u, 360u, nullptr}};
inline constexpr Chunk kChunks121[] = {{0u, 2719616u, 64u, nullptr}, {4456u, 2646024u, 456u, nullptr}};
inline constexpr Chunk kChunks122[] = {{64u, 2724720u, 240u, nullptr}, {328u, 2724984u, 368u, nullptr}, {736u, 2725392u, 608u, nullptr}, {1384u, 2726040u, 664u, nullptr}, {2048u, 2726712u, 2048u, nullptr}, {4096u, 2635728u, 360u, nullptr}};
inline constexpr Chunk kChunks123[] = {{304u, 2724960u, 656u, nullptr}, {1608u, 2643176u, 504u, nullptr}, {2112u, 2726776u, 1984u, nullptr}, {4096u, 2635728u, 360u, nullptr}};
inline constexpr Chunk kChunks124[] = {{968u, 2725624u, 1080u, nullptr}, {2048u, 2726712u, 2048u, nullptr}, {4096u, 2635728u, 360u, nullptr}};
inline constexpr Chunk kChunks125[] = {{0u, 2729616u, 64u, nullptr}, {4312u, 2733944u, 416u, nullptr}};
inline constexpr Chunk kChunks126[] = {{64u, 2729680u, 240u, nullptr}, {328u, 2729944u, 360u, nullptr}, {728u, 2730344u, 600u, nullptr}, {1368u, 2730984u, 680u, nullptr}, {2048u, 2731672u, 2048u, nullptr}, {4096u, 2635872u, 216u, nullptr}};
inline constexpr Chunk kChunks127[] = {{304u, 2729920u, 648u, nullptr}, {1592u, 3061232u, 664u, nullptr}, {2256u, 2731880u, 1840u, nullptr}, {4096u, 2635872u, 216u, nullptr}};
inline constexpr Chunk kChunks128[] = {{960u, 2730576u, 1088u, nullptr}, {2048u, 2731672u, 2048u, nullptr}, {4096u, 2635872u, 216u, nullptr}};
inline constexpr Chunk kChunks129[] = {{0u, 2667952u, 64u, nullptr}, {4504u, 2733944u, 416u, nullptr}};
inline constexpr Chunk kChunks130[] = {{64u, 2724720u, 240u, nullptr}, {328u, 2734728u, 368u, nullptr}, {736u, 2735136u, 608u, nullptr}, {1384u, 2720952u, 712u, nullptr}, {2096u, 2736504u, 2000u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks131[] = {{304u, 2734704u, 656u, nullptr}, {1608u, 2754888u, 600u, nullptr}, {2208u, 2736616u, 1888u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks132[] = {{968u, 2735368u, 1080u, nullptr}, {2048u, 2736456u, 2048u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks133[] = {{0u, 2739360u, 64u, nullptr}, {4328u, 2733944u, 416u, nullptr}};
inline constexpr Chunk kChunks134[] = {{64u, 2739424u, 240u, nullptr}, {328u, 2739688u, 368u, nullptr}, {736u, 2740096u, 608u, nullptr}, {1384u, 2730984u, 680u, nullptr}, {2064u, 2741432u, 2032u, nullptr}, {4096u, 2635856u, 232u, nullptr}};
inline constexpr Chunk kChunks135[] = {{304u, 2739664u, 656u, nullptr}, {1608u, 3061232u, 664u, nullptr}, {2272u, 2741640u, 1824u, nullptr}, {4096u, 2635856u, 232u, nullptr}};
inline constexpr Chunk kChunks136[] = {{968u, 2740328u, 1080u, nullptr}, {2048u, 2741416u, 2048u, nullptr}, {4096u, 2635856u, 232u, nullptr}};
inline constexpr Chunk kChunks137[] = {{0u, 2744144u, 64u, nullptr}, {4176u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks138[] = {{64u, 2744208u, 224u, nullptr}, {312u, 2744456u, 296u, nullptr}, {648u, 2744792u, 528u, nullptr}, {1216u, 2745360u, 832u, nullptr}, {2048u, 2746200u, 2048u, nullptr}, {4096u, 2636008u, 80u, nullptr}};
inline constexpr Chunk kChunks139[] = {{288u, 2744432u, 584u, nullptr}, {1440u, 2745584u, 608u, nullptr}, {2048u, 2746200u, 2048u, nullptr}, {4096u, 2636008u, 80u, nullptr}};
inline constexpr Chunk kChunks140[] = {{880u, 2745024u, 1168u, nullptr}, {2048u, 2746200u, 2048u, nullptr}, {4096u, 2636008u, 80u, nullptr}};
inline constexpr Chunk kChunks141[] = {{64u, 2748848u, 224u, nullptr}, {312u, 2749096u, 296u, nullptr}, {648u, 2749432u, 528u, nullptr}, {1216u, 2750000u, 832u, nullptr}, {2048u, 2750840u, 2048u, nullptr}, {4096u, 2636008u, 80u, nullptr}};
inline constexpr Chunk kChunks142[] = {{288u, 2749072u, 584u, nullptr}, {1440u, 2750224u, 608u, nullptr}, {2048u, 2750840u, 2048u, nullptr}, {4096u, 2636008u, 80u, nullptr}};
inline constexpr Chunk kChunks143[] = {{880u, 2749664u, 1168u, nullptr}, {2048u, 2750840u, 2048u, nullptr}, {4096u, 2636008u, 80u, nullptr}};
inline constexpr Chunk kChunks144[] = {{64u, 2748848u, 224u, nullptr}, {312u, 2753752u, 296u, nullptr}, {648u, 2754088u, 536u, nullptr}, {1224u, 2754664u, 824u, nullptr}, {2048u, 2755496u, 2048u, nullptr}, {4096u, 2635840u, 248u, nullptr}};
inline constexpr Chunk kChunks145[] = {{288u, 2753728u, 584u, nullptr}, {1448u, 2754888u, 600u, nullptr}, {2048u, 2755496u, 2048u, nullptr}, {4096u, 2635840u, 248u, nullptr}};
inline constexpr Chunk kChunks146[] = {{880u, 2754320u, 1168u, nullptr}, {2048u, 2755496u, 2048u, nullptr}, {4096u, 2635840u, 248u, nullptr}};
inline constexpr Chunk kChunks147[] = {{64u, 2748848u, 224u, nullptr}, {312u, 2758552u, 296u, nullptr}, {648u, 2758888u, 528u, nullptr}, {1216u, 2759456u, 832u, nullptr}, {2048u, 2760296u, 2048u, nullptr}, {4096u, 2635848u, 240u, nullptr}};
inline constexpr Chunk kChunks148[] = {{288u, 2758528u, 584u, nullptr}, {1440u, 2759680u, 608u, nullptr}, {2048u, 2760296u, 2048u, nullptr}, {4096u, 2635848u, 240u, nullptr}};
inline constexpr Chunk kChunks149[] = {{880u, 2759120u, 1168u, nullptr}, {2048u, 2760296u, 2048u, nullptr}, {4096u, 2635848u, 240u, nullptr}};
inline constexpr Chunk kChunks150[] = {{0u, 2763040u, 64u, nullptr}, {4192u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks151[] = {{64u, 2763104u, 224u, nullptr}, {312u, 2763352u, 312u, nullptr}, {664u, 2763704u, 528u, nullptr}, {1232u, 2750000u, 832u, nullptr}, {2064u, 2765112u, 2032u, nullptr}, {4096u, 2635992u, 96u, nullptr}};
inline constexpr Chunk kChunks152[] = {{288u, 2763328u, 600u, nullptr}, {1456u, 2750224u, 608u, nullptr}, {2064u, 2765112u, 2032u, nullptr}, {4096u, 2635992u, 96u, nullptr}};
inline constexpr Chunk kChunks153[] = {{896u, 2763936u, 1152u, nullptr}, {2048u, 2765096u, 2048u, nullptr}, {4096u, 2635992u, 96u, nullptr}};
inline constexpr Chunk kChunks154[] = {{64u, 2763104u, 224u, nullptr}, {312u, 2768008u, 312u, nullptr}, {664u, 2768360u, 552u, nullptr}, {1256u, 2768952u, 792u, nullptr}, {2048u, 2769752u, 2048u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks155[] = {{288u, 2767984u, 600u, nullptr}, {1480u, 2769176u, 568u, nullptr}, {2048u, 2769752u, 2048u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks156[] = {{896u, 2768592u, 1152u, nullptr}, {2048u, 2769752u, 2048u, nullptr}, {4096u, 2635808u, 280u, nullptr}};
inline constexpr Chunk kChunks157[] = {{64u, 2763104u, 224u, nullptr}, {312u, 2772840u, 312u, nullptr}, {664u, 2773192u, 528u, nullptr}, {1232u, 2750000u, 832u, nullptr}, {2064u, 2774600u, 2032u, nullptr}, {4096u, 2635992u, 96u, nullptr}};
inline constexpr Chunk kChunks158[] = {{288u, 2772816u, 600u, nullptr}, {1456u, 2750224u, 608u, nullptr}, {2064u, 2774600u, 2032u, nullptr}, {4096u, 2635992u, 96u, nullptr}};
inline constexpr Chunk kChunks159[] = {{896u, 2773424u, 1152u, nullptr}, {2048u, 2774584u, 2048u, nullptr}, {4096u, 2635992u, 96u, nullptr}};
inline constexpr Chunk kChunks160[] = {{0u, 2777192u, 64u, nullptr}, {4280u, 2733944u, 416u, nullptr}};
inline constexpr Chunk kChunks161[] = {{64u, 2777256u, 240u, nullptr}, {328u, 2777520u, 352u, nullptr}, {720u, 2777912u, 592u, nullptr}, {1352u, 2788216u, 704u, nullptr}, {2056u, 2788928u, 2048u, nullptr}, {4104u, 2635912u, 176u, nullptr}};
inline constexpr Chunk kChunks162[] = {{304u, 2777496u, 640u, nullptr}, {1576u, 3061232u, 664u, nullptr}, {2240u, 2789112u, 1864u, nullptr}, {4104u, 2635912u, 176u, nullptr}};
inline constexpr Chunk kChunks163[] = {{952u, 2778144u, 1096u, nullptr}, {2048u, 2779248u, 2048u, nullptr}, {4096u, 2635904u, 184u, nullptr}};
inline constexpr Chunk kChunks164[] = {{0u, 2781944u, 64u, nullptr}, {4472u, 2733944u, 416u, nullptr}};
inline constexpr Chunk kChunks165[] = {{64u, 2782008u, 240u, nullptr}, {328u, 2782272u, 360u, nullptr}, {728u, 2782672u, 600u, nullptr}, {1368u, 2783312u, 680u, nullptr}, {2048u, 2784000u, 2048u, nullptr}, {4096u, 2635712u, 376u, nullptr}};
inline constexpr Chunk kChunks166[] = {{304u, 2782248u, 648u, nullptr}, {1592u, 2754888u, 600u, nullptr}, {2192u, 2784144u, 1904u, nullptr}, {4096u, 2635712u, 376u, nullptr}};
inline constexpr Chunk kChunks167[] = {{960u, 2782904u, 1088u, nullptr}, {2048u, 2784000u, 2048u, nullptr}, {4096u, 2635712u, 376u, nullptr}};
inline constexpr Chunk kChunks168[] = {{0u, 2786872u, 64u, nullptr}, {4272u, 2733944u, 416u, nullptr}};
inline constexpr Chunk kChunks169[] = {{64u, 2777256u, 240u, nullptr}, {328u, 2787200u, 352u, nullptr}, {720u, 2787592u, 584u, nullptr}, {1344u, 2788216u, 704u, nullptr}, {2048u, 2788928u, 2048u, nullptr}, {4096u, 2635912u, 176u, nullptr}};
inline constexpr Chunk kChunks170[] = {{304u, 2787176u, 640u, nullptr}, {1568u, 3061232u, 664u, nullptr}, {2232u, 2789112u, 1864u, nullptr}, {4096u, 2635912u, 176u, nullptr}};
inline constexpr Chunk kChunks171[] = {{952u, 2787824u, 1096u, nullptr}, {2048u, 2788928u, 2048u, nullptr}, {4096u, 2635912u, 176u, nullptr}};
inline constexpr Chunk kChunks172[] = {{288u, 2791896u, 568u, nullptr}, {1416u, 3056688u, 648u, nullptr}, {2064u, 2793680u, 2016u, nullptr}};
inline constexpr Chunk kChunks173[] = {{864u, 2792472u, 1184u, nullptr}, {2048u, 2793664u, 2032u, nullptr}};
inline constexpr Chunk kChunks174[] = {{64u, 2796216u, 224u, nullptr}, {312u, 2796464u, 280u, nullptr}, {632u, 2796784u, 520u, nullptr}, {1192u, 2797344u, 856u, nullptr}, {2048u, 2798208u, 2032u, nullptr}};
inline constexpr Chunk kChunks175[] = {{288u, 2796440u, 568u, nullptr}, {1416u, 3061232u, 664u, nullptr}, {2080u, 2798240u, 2000u, nullptr}};
inline constexpr Chunk kChunks176[] = {{864u, 2797016u, 1184u, nullptr}, {2048u, 2798208u, 2032u, nullptr}};
inline constexpr Chunk kChunks177[] = {{288u, 2810520u, 584u, nullptr}, {1432u, 3061232u, 664u, nullptr}, {2096u, 2812336u, 2000u, nullptr}};
inline constexpr Chunk kChunks178[] = {{0u, 2819584u, 64u, nullptr}, {4112u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks179[] = {{64u, 2810296u, 224u, nullptr}, {312u, 2819896u, 296u, nullptr}, {648u, 2820232u, 520u, nullptr}, {1208u, 2820792u, 840u, nullptr}, {2048u, 2821640u, 2048u, nullptr}, {4096u, 2635480u, 16u, nullptr}};
inline constexpr Chunk kChunks180[] = {{288u, 2819872u, 584u, nullptr}, {1432u, 3061232u, 664u, nullptr}, {2096u, 2821688u, 2000u, nullptr}, {4096u, 2635480u, 16u, nullptr}};
inline constexpr Chunk kChunks181[] = {{880u, 2820464u, 1168u, nullptr}, {2048u, 2821640u, 2048u, nullptr}, {4096u, 2635480u, 16u, nullptr}};
inline constexpr Chunk kChunks182[] = {{64u, 3024592u, 160u, nullptr}, {256u, 3024784u, 312u, nullptr}, {608u, 3025136u, 448u, nullptr}, {1096u, 2655360u, 32u, nullptr}};
inline constexpr Chunk kChunks183[] = {{64u, 3024592u, 160u, nullptr}, {256u, 3026384u, 312u, nullptr}, {608u, 3026736u, 448u, nullptr}, {1096u, 2655360u, 32u, nullptr}};
inline constexpr Chunk kChunks184[] = {{64u, 3027792u, 176u, nullptr}, {272u, 3028000u, 280u, nullptr}, {592u, 3028320u, 424u, nullptr}, {1056u, 2652208u, 32u, nullptr}};
inline constexpr Chunk kChunks185[] = {{632u, 3029928u, 456u, nullptr}};
inline constexpr Chunk kChunks186[] = {{64u, 3030936u, 224u, nullptr}, {312u, 3031184u, 344u, nullptr}, {696u, 3031568u, 600u, nullptr}, {1336u, 3163080u, 736u, nullptr}, {2072u, 3163824u, 2048u, nullptr}, {4120u, 2635864u, 224u, nullptr}};
inline constexpr Chunk kChunks187[] = {{288u, 3031160u, 640u, nullptr}, {1568u, 3056688u, 648u, nullptr}, {2216u, 3163968u, 1904u, nullptr}, {4120u, 2635864u, 224u, nullptr}};
inline constexpr Chunk kChunks188[] = {{936u, 3031808u, 1112u, nullptr}, {2048u, 3032928u, 2048u, nullptr}, {4096u, 2635840u, 248u, nullptr}};
inline constexpr Chunk kChunks189[] = {{0u, 2677760u, 64u, nullptr}, {4368u, 3040056u, 608u, nullptr}};
inline constexpr Chunk kChunks190[] = {{64u, 3035736u, 240u, nullptr}, {328u, 3036000u, 408u, nullptr}, {776u, 3036448u, 640u, nullptr}, {1456u, 3037128u, 592u, nullptr}, {2048u, 3037728u, 2048u, nullptr}, {4096u, 2635816u, 272u, nullptr}};
inline constexpr Chunk kChunks191[] = {{304u, 3035976u, 696u, nullptr}, {1688u, 3037360u, 360u, nullptr}, {2048u, 3037728u, 2048u, nullptr}, {4096u, 2635816u, 272u, nullptr}};
inline constexpr Chunk kChunks192[] = {{1008u, 3036680u, 1040u, nullptr}, {2048u, 3037728u, 2048u, nullptr}, {4096u, 2635816u, 272u, nullptr}};
inline constexpr Chunk kChunks193[] = {{0u, 3040696u, 64u, nullptr}, {4232u, 2733944u, 416u, nullptr}};
inline constexpr Chunk kChunks194[] = {{64u, 3040760u, 224u, nullptr}, {312u, 3041008u, 344u, nullptr}, {696u, 3041392u, 576u, nullptr}, {1312u, 3042008u, 736u, nullptr}, {2048u, 3042752u, 2048u, nullptr}, {4096u, 2635952u, 136u, nullptr}};
inline constexpr Chunk kChunks195[] = {{288u, 3040984u, 632u, nullptr}, {1536u, 3042232u, 512u, nullptr}, {2048u, 3042752u, 2048u, nullptr}, {4096u, 2635952u, 136u, nullptr}};
inline constexpr Chunk kChunks196[] = {{928u, 3041624u, 1120u, nullptr}, {2048u, 3042752u, 2048u, nullptr}, {4096u, 2635952u, 136u, nullptr}};
inline constexpr Chunk kChunks197[] = {{288u, 3055576u, 560u, nullptr}, {1400u, 3056688u, 648u, nullptr}, {2048u, 3057344u, 2048u, nullptr}};
inline constexpr Chunk kChunks198[] = {{856u, 3056144u, 1192u, nullptr}, {2048u, 3057344u, 2048u, nullptr}};
inline constexpr Chunk kChunks199[] = {{0u, 3059848u, 64u, nullptr}, {4088u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks200[] = {{64u, 3059912u, 224u, nullptr}, {312u, 3060160u, 264u, nullptr}, {616u, 3060464u, 504u, nullptr}, {1160u, 3061008u, 888u, nullptr}, {2048u, 3061904u, 2040u, nullptr}};
inline constexpr Chunk kChunks201[] = {{288u, 3060136u, 552u, nullptr}, {1384u, 3061232u, 664u, nullptr}, {2048u, 3061904u, 2040u, nullptr}};
inline constexpr Chunk kChunks202[] = {{848u, 3060696u, 1200u, nullptr}, {2048u, 3061904u, 2040u, nullptr}};
inline constexpr Chunk kChunks203[] = {{0u, 3079016u, 64u, nullptr}, {4480u, 3078544u, 448u, nullptr}};
inline constexpr Chunk kChunks204[] = {{64u, 3079080u, 384u, nullptr}, {472u, 3079488u, 288u, nullptr}, {800u, 3079816u, 560u, nullptr}, {1400u, 3080416u, 648u, nullptr}, {2048u, 3081072u, 2048u, nullptr}, {4096u, 2635704u, 384u, nullptr}};
inline constexpr Chunk kChunks205[] = {{448u, 3079464u, 576u, nullptr}, {1624u, 2754888u, 600u, nullptr}, {2224u, 3081248u, 1872u, nullptr}, {4096u, 2635704u, 384u, nullptr}};
inline constexpr Chunk kChunks206[] = {{1032u, 3080048u, 1016u, nullptr}, {2048u, 3081072u, 2048u, nullptr}, {4096u, 2635704u, 384u, nullptr}};
inline constexpr Chunk kChunks207[] = {{0u, 3083992u, 64u, nullptr}, {4528u, 3078544u, 448u, nullptr}};
inline constexpr Chunk kChunks208[] = {{64u, 3079080u, 384u, nullptr}, {472u, 3084464u, 288u, nullptr}, {800u, 3084792u, 560u, nullptr}, {1400u, 3085392u, 648u, nullptr}, {2048u, 3086048u, 2048u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks209[] = {{448u, 3084440u, 576u, nullptr}, {1624u, 2759680u, 608u, nullptr}, {2232u, 3086232u, 1864u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks210[] = {{1032u, 3085024u, 1016u, nullptr}, {2048u, 3086048u, 2048u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks211[] = {{304u, 3089320u, 616u, nullptr}, {1536u, 3090552u, 512u, nullptr}, {2048u, 3091072u, 2048u, nullptr}, {4096u, 2635776u, 312u, nullptr}};
inline constexpr Chunk kChunks212[] = {{0u, 3093880u, 64u, nullptr}, {4496u, 2733944u, 416u, nullptr}};
inline constexpr Chunk kChunks213[] = {{64u, 3093944u, 256u, nullptr}, {344u, 3094224u, 368u, nullptr}, {752u, 3094632u, 608u, nullptr}, {1400u, 3095280u, 648u, nullptr}, {2048u, 3095936u, 2048u, nullptr}, {4096u, 2650672u, 400u, nullptr}};
inline constexpr Chunk kChunks214[] = {{320u, 3094200u, 656u, nullptr}, {1624u, 3095504u, 424u, nullptr}, {2048u, 3095936u, 2048u, nullptr}, {4096u, 2650672u, 400u, nullptr}};
inline constexpr Chunk kChunks215[] = {{984u, 3094864u, 1064u, nullptr}, {2048u, 3095936u, 2048u, nullptr}, {4096u, 2650672u, 400u, nullptr}};
inline constexpr Chunk kChunks216[] = {{0u, 2687600u, 64u, nullptr}, {4424u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks217[] = {{64u, 3098896u, 240u, nullptr}, {328u, 3099160u, 328u, nullptr}, {696u, 3099528u, 576u, nullptr}, {1312u, 3100144u, 736u, nullptr}, {2048u, 3100888u, 2048u, nullptr}, {4096u, 2635760u, 328u, nullptr}};
inline constexpr Chunk kChunks218[] = {{304u, 3099136u, 616u, nullptr}, {1536u, 3090552u, 512u, nullptr}, {2048u, 3100888u, 2048u, nullptr}, {4096u, 2635760u, 328u, nullptr}};
inline constexpr Chunk kChunks219[] = {{928u, 3099760u, 1120u, nullptr}, {2048u, 3100888u, 2048u, nullptr}, {4096u, 2635760u, 328u, nullptr}};
inline constexpr Chunk kChunks220[] = {{0u, 2646528u, 64u, nullptr}, {4528u, 2733944u, 416u, nullptr}};
inline constexpr Chunk kChunks221[] = {{64u, 3103776u, 256u, nullptr}, {344u, 3104056u, 376u, nullptr}, {760u, 3104472u, 616u, nullptr}, {1416u, 3105128u, 632u, nullptr}, {2048u, 3105768u, 2048u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks222[] = {{320u, 3104032u, 664u, nullptr}, {1640u, 3095504u, 424u, nullptr}, {2064u, 3105784u, 2032u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks223[] = {{992u, 3104704u, 1056u, nullptr}, {2048u, 3105768u, 2048u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks224[] = {{0u, 3108704u, 64u, nullptr}, {4552u, 2646024u, 456u, nullptr}};
inline constexpr Chunk kChunks225[] = {{64u, 3108768u, 240u, nullptr}, {328u, 3109032u, 432u, nullptr}, {800u, 3109504u, 848u, nullptr}, {1688u, 3110392u, 32u, nullptr}};
inline constexpr Chunk kChunks226[] = {{304u, 3109008u, 528u, nullptr}};
inline constexpr Chunk kChunks227[] = {{840u, 3109544u, 880u, nullptr}};
inline constexpr Chunk kChunks228[] = {{0u, 2646528u, 64u, nullptr}, {4528u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks229[] = {{64u, 2636720u, 224u, nullptr}, {312u, 3137104u, 352u, nullptr}, {704u, 3137496u, 608u, nullptr}, {1352u, 3138144u, 696u, nullptr}, {2048u, 3138848u, 2048u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks230[] = {{288u, 3137080u, 648u, nullptr}, {1584u, 3138376u, 464u, nullptr}, {2048u, 3138848u, 2048u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks231[] = {{944u, 3137736u, 1104u, nullptr}, {2048u, 3138848u, 2048u, nullptr}, {4096u, 2650640u, 432u, nullptr}};
inline constexpr Chunk kChunks232[] = {{64u, 2719680u, 224u, nullptr}, {312u, 3142088u, 360u, nullptr}, {712u, 3142488u, 624u, nullptr}, {1376u, 3143152u, 672u, nullptr}, {2048u, 3143832u, 2048u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks233[] = {{288u, 3142064u, 648u, nullptr}, {1608u, 2769176u, 568u, nullptr}, {2176u, 3143960u, 1920u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks234[] = {{944u, 3142720u, 1104u, nullptr}, {2048u, 3143832u, 2048u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks235[] = {{0u, 2682608u, 64u, nullptr}, {4536u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks236[] = {{64u, 3146808u, 240u, nullptr}, {328u, 3147072u, 360u, nullptr}, {728u, 3147472u, 616u, nullptr}, {1384u, 3148128u, 664u, nullptr}, {2048u, 3148800u, 2048u, nullptr}, {4096u, 2662120u, 440u, nullptr}};
inline constexpr Chunk kChunks237[] = {{304u, 3147048u, 656u, nullptr}, {1616u, 3148360u, 432u, nullptr}, {2048u, 3148800u, 2048u, nullptr}, {4096u, 2662120u, 440u, nullptr}};
inline constexpr Chunk kChunks238[] = {{968u, 3147712u, 1080u, nullptr}, {2048u, 3148800u, 2048u, nullptr}, {4096u, 2662120u, 440u, nullptr}};
inline constexpr Chunk kChunks239[] = {{0u, 3108704u, 64u, nullptr}, {4552u, 2667512u, 416u, nullptr}};
inline constexpr Chunk kChunks240[] = {{64u, 3151792u, 240u, nullptr}, {328u, 3152056u, 376u, nullptr}, {744u, 3152472u, 648u, nullptr}, {1432u, 3153160u, 616u, nullptr}, {2048u, 3153784u, 2048u, nullptr}, {4096u, 2662104u, 456u, nullptr}};
inline constexpr Chunk kChunks241[] = {{304u, 3152032u, 664u, nullptr}, {1664u, 3090552u, 512u, nullptr}, {2176u, 3143912u, 1968u, nullptr}, {4144u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks242[] = {{976u, 3152704u, 1072u, nullptr}, {2048u, 3153784u, 2048u, nullptr}, {4096u, 2662104u, 456u, nullptr}};
inline constexpr Chunk kChunks243[] = {{0u, 3156736u, 64u, nullptr}, {4520u, 2646024u, 456u, nullptr}};
inline constexpr Chunk kChunks244[] = {{64u, 3156800u, 240u, nullptr}, {328u, 3157064u, 392u, nullptr}, {760u, 3157496u, 640u, nullptr}, {1440u, 3158176u, 608u, nullptr}, {2048u, 3158792u, 2048u, nullptr}, {4096u, 2650648u, 424u, nullptr}};
inline constexpr Chunk kChunks245[] = {{304u, 3157040u, 688u, nullptr}, {1672u, 2643176u, 504u, nullptr}, {2176u, 3158920u, 1920u, nullptr}, {4096u, 2650648u, 424u, nullptr}};
inline constexpr Chunk kChunks246[] = {{1000u, 3157736u, 1048u, nullptr}, {2048u, 3158792u, 2048u, nullptr}, {4096u, 2650648u, 424u, nullptr}};
inline constexpr Chunk kChunks247[] = {{64u, 3161832u, 224u, nullptr}, {312u, 3162080u, 336u, nullptr}, {688u, 3162456u, 584u, nullptr}, {1312u, 3163080u, 736u, nullptr}, {2048u, 3163824u, 2048u, nullptr}, {4096u, 2635864u, 224u, nullptr}};
inline constexpr Chunk kChunks248[] = {{288u, 3162056u, 632u, nullptr}, {1544u, 3056688u, 648u, nullptr}, {2192u, 3163968u, 1904u, nullptr}, {4096u, 2635864u, 224u, nullptr}};
inline constexpr Chunk kChunks249[] = {{928u, 3162696u, 1120u, nullptr}, {2048u, 3163824u, 2048u, nullptr}, {4096u, 2635864u, 224u, nullptr}};
inline constexpr Chunk kChunks250[] = {{64u, 3166616u, 224u, nullptr}, {312u, 3166864u, 344u, nullptr}, {696u, 3167248u, 608u, nullptr}, {1344u, 3167896u, 704u, nullptr}, {2048u, 3168608u, 2048u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks251[] = {{288u, 3166840u, 640u, nullptr}, {1576u, 2754888u, 600u, nullptr}, {2176u, 3168736u, 1920u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks252[] = {{936u, 3167488u, 1112u, nullptr}, {2048u, 3168608u, 2048u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks253[] = {{64u, 3166616u, 224u, nullptr}, {312u, 3171824u, 344u, nullptr}, {696u, 3172208u, 608u, nullptr}, {1344u, 3172856u, 704u, nullptr}, {2048u, 3173568u, 2048u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks254[] = {{288u, 3171800u, 640u, nullptr}, {1576u, 2759680u, 608u, nullptr}, {2184u, 3173704u, 1912u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr Chunk kChunks255[] = {{936u, 3172448u, 1112u, nullptr}, {2048u, 3173568u, 2048u, nullptr}, {4096u, 2650664u, 408u, nullptr}};
inline constexpr std::array<Range, 256> kRanges{{
    Range{"861b6b064f6661e4", 624u, 4328u, "lift_861b6b06_0270", kChunks0, 4u, "983e8068e06d05e1d2e424ac50e2a915cab826d73c8580b732d6bf5992451fd1"},
    Range{"3319183d22d5916c", 1440u, 4560u, "lift_3319183d_05a0", kChunks1, 3u, "2603c0bdd0b141b10a58eaf0da9e498cb48b483afa558733a3ff9a4c6ca10c8c"},
    Range{"861b6b064f6661e4", 64u, 4328u, "lift_861b6b06_0040", kChunks2, 6u, "b722d46c738f3e49cb7900c6ba7c198631cce0da0e1695bb49e88a5c90968d2c"},
    Range{"3319183d22d5916c", 744u, 4560u, "lift_3319183d_02e8", kChunks3, 4u, "fb03ad032a6d8e6b44b764b2bc02e66ea2de4d50d561aada18bafd3c4ece2319"},
    Range{"77216489d3c195e2", 1280u, 4368u, "lift_77216489_0500", kChunks4, 3u, "1299a8de01cbe13bd191c5105d1e473cdc7ad8f8e3199c0dd7493e04267fe158"},
    Range{"3319183d22d5916c", 64u, 4560u, "lift_3319183d_0040", kChunks5, 6u, "08ee65ba4bf4bf0a14b55a5b50d38231480d63adba0ae7a18269426994be007d"},
    Range{"77216489d3c195e2", 664u, 4368u, "lift_77216489_0298", kChunks6, 4u, "768b784626232634c2b7e1ab29829d8e436932362e38fba1ac38bfd55bb3f5fb"},
    Range{"028bc7209ee72601", 1200u, 4304u, "lift_028bc720_04b0", kChunks7, 3u, "e617c51a4804e5ccf5ccc83ca278b0f8415ef85e1ce5fc2b3ed2a4cce493c9fa"},
    Range{"28152de151fbea2c", 760u, 4544u, "lift_28152de1_02f8", kChunks8, 4u, "7af1941767ec4eb6e861db8311356072e63f118f671147f382684e848cb6d99a"},
    Range{"77216489d3c195e2", 64u, 4368u, "lift_77216489_0040", kChunks9, 6u, "073dc133666bc6ccaa85e8b5e2038dec6de8d328bd93f31181c9a78228be29f3"},
    Range{"028bc7209ee72601", 608u, 4304u, "lift_028bc720_0260", kChunks10, 4u, "4f6386ccd974ef4d999fd80b14f951a95727323073e4a23d952045cc156dd538"},
    Range{"36ba79dadb53b9a2", 1200u, 4312u, "lift_36ba79da_04b0", kChunks11, 3u, "c9e1a5c4ce1fd73373b811034e23905e02354bbf206d2923e8c42aca395860cd"},
    Range{"028bc7209ee72601", 64u, 4304u, "lift_028bc720_0040", kChunks12, 6u, "58ebf8e131f733165aa14c56d6c76b8221d4f148c4e359511fd14340e96d6042"},
    Range{"77216489d3c195e2", 0u, 4824u, "lift_77216489_0000", kChunks13, 2u, "7608123c4ad55c766e84846b889253f855bdb859a15265e6625c86c9cfc2790d"},
    Range{"028bc7209ee72601", 0u, 4720u, "lift_028bc720_0000", kChunks14, 2u, "adc012dab1cd78e9746c49992bb123d9e9642f7657617862ae08de7a0ba70f5d"},
    Range{"b4ffb301c16d1842", 64u, 4088u, "lift_b4ffb301_0040", kChunks15, 5u, "41b94c3b1c77e0c0bc59a2b0f62014295915f3b1f199359b79dc6c38156b6285"},
    Range{"28152de151fbea2c", 64u, 4544u, "lift_28152de1_0040", kChunks16, 6u, "61bd5c919bc46e9a9eddbed47c11e647d3dedd374954f1ec1fb766ee6bc6ec4d"},
    Range{"80a3120c4692781f", 1224u, 4328u, "lift_80a3120c_04c8", kChunks17, 3u, "9c7a5ad17e5c43b273bc3d30774ed0eff9d334fdef773d416883acd5780fe85a"},
    Range{"36ba79dadb53b9a2", 608u, 4312u, "lift_36ba79da_0260", kChunks18, 4u, "63c5266143f15f878c7969ecd8ef3f197c78f10138eeac4137971e1e3de84c4a"},
    Range{"861b6b064f6661e4", 0u, 4744u, "lift_861b6b06_0000", kChunks19, 2u, "286f5a9eb5884619303cd29ce4e643247078b2f2d77b95555879ec614d04cf16"},
    Range{"36ba79dadb53b9a2", 64u, 4312u, "lift_36ba79da_0040", kChunks20, 6u, "a60fb7312827a856ceb2b701dccf57bc1568ceed85fa4fbdb657abe89267afe4"},
    Range{"132215b679e1aa49", 608u, 4288u, "lift_132215b6_0260", kChunks21, 4u, "801a2a77c96280c7b0277dc49299c598d12ba6b43f86c0eec3c3b78f3b490941"},
    Range{"f098ac393f1da878", 608u, 4336u, "lift_f098ac39_0260", kChunks22, 4u, "c29ef0e1c33d0eb2121c42cd296ad05546f427ca20853a8a59f6bcac1814931a"},
    Range{"e4a20a8ab9286b0a", 0u, 4704u, "lift_e4a20a8a_0000", kChunks23, 2u, "ba7f582bcb95028728c863fe0b5c49bd5ddcba48ec17db354967fa8384739d67"},
    Range{"b4ffb301c16d1842", 1168u, 4088u, "lift_b4ffb301_0490", kChunks24, 2u, "e0a2b3d91c65ee72c9d7c0038b30bc56982f6eb4bdbb09c5650f2b96bb0a6e75"},
    Range{"3319183d22d5916c", 0u, 5168u, "lift_3319183d_0000", kChunks25, 2u, "7577e79612140f3d8112de35472f26cbb9f88f55b9c380eac994df06d6b2d45c"},
    Range{"28152de151fbea2c", 1368u, 4544u, "lift_28152de1_0558", kChunks26, 3u, "9643cdf859523230da23adb1272e7c8469dc4f10a017fd792471a553df43ea9d"},
    Range{"e4a20a8ab9286b0a", 64u, 4288u, "lift_e4a20a8a_0040", kChunks27, 6u, "352ade12da7ccbc3513cd8015902af5621054122f10557fb352cac73119ce1e9"},
    Range{"36ba79dadb53b9a2", 0u, 4728u, "lift_36ba79da_0000", kChunks28, 2u, "02aa568216ce5c4d0841af72fe18a52ecc57c4ab7fe8dc21d07e96405d91dd5d"},
    Range{"f098ac393f1da878", 64u, 4336u, "lift_f098ac39_0040", kChunks29, 6u, "d0713e7f6398b07e0a2b086ab6a3aa86a5f5be80d8405c621d44ca3c73299c3d"},
    Range{"f098ac393f1da878", 0u, 4752u, "lift_f098ac39_0000", kChunks30, 2u, "62be005080477d485d83fea60f1ebbf92bf0d9762e96ccd7e12b690e785f6167"},
    Range{"132215b679e1aa49", 1200u, 4288u, "lift_132215b6_04b0", kChunks31, 3u, "ec08cc7acce7ccdfb3a817f5289ffcdd1760d55c923ffd65588757e15a5b89d2"},
    Range{"f098ac393f1da878", 1200u, 4336u, "lift_f098ac39_04b0", kChunks32, 3u, "d9e001ea442e0bc0f7b62013f31de6a02f76f1adca5cd2e4a91a9286fb9db884"},
    Range{"b4ffb301c16d1842", 0u, 4504u, "lift_b4ffb301_0000", kChunks33, 2u, "9b967a03fe30a4da805252208b6985d2a634a4cd9e62118033311f0d1b16d38a"},
    Range{"28152de151fbea2c", 0u, 4992u, "lift_28152de1_0000", kChunks34, 2u, "7563b83f299df8895c501b9afc7eac1dc1650081b6e0901fed9530bd2c527c8a"},
    Range{"5e9c28ca679e97bf", 0u, 4488u, "lift_5e9c28ca_0000", kChunks35, 2u, "a95306b3c7f4642ce7ca4fe82c4f5ca15c9c5bb5a63fc6bf7708d893d0bef555"},
    Range{"5e9c28ca679e97bf", 64u, 4072u, "lift_5e9c28ca_0040", kChunks36, 5u, "3adcc2e90aecf297f55407523f6a90e6fbc7da39888ce453c0fe9847f6300d5c"},
    Range{"65d23cf872f398f9", 64u, 4088u, "lift_65d23cf8_0040", kChunks37, 5u, "47aac1fc2088df6e5ea53d1f316414eb80d58fe34778a259864acd6e32394d0c"},
    Range{"e9c4677aeaae0f09", 0u, 4816u, "lift_e9c4677a_0000", kChunks38, 2u, "5213dc89a77f867e031a0c5ca1238fc771327b522863f7b79c64c5e67fdffaf6"},
    Range{"e9c4677aeaae0f09", 1272u, 4400u, "lift_e9c4677a_04f8", kChunks39, 3u, "b52c119a4ec8c9ddb026831257087616e55a74dfd9c0302e689dddeb7f180be3"},
    Range{"61fe8c48bbb8373b", 0u, 1528u, "lift_61fe8c48_0000", kChunks40, 2u, "31787ff8e9ad933248481a877ec58475b78e7a17c42af542c423054c9f25d0b7"},
    Range{"e9c4677aeaae0f09", 64u, 4400u, "lift_e9c4677a_0040", kChunks41, 6u, "a1800fb11b34cbb061d2933bc4a394d6603371cf43fa0ff441b104c4f496c6e5"},
    Range{"61fe8c48bbb8373b", 64u, 1080u, "lift_61fe8c48_0040", kChunks42, 4u, "90ae1c2c3fc81fe6dbcc986a5745c35bc5cd91b4518f77152effb09fcad9d185"},
    Range{"5c01ebb4d16c6ec7", 0u, 5000u, "lift_s000_0000", kChunks43, 2u, "c41dff7b50a1e6979fc4bb9f7f3efbcf99f960acf558c2a7bb735013a95ae5d8"},
    Range{"5c01ebb4d16c6ec7", 64u, 4480u, "lift_s000_0040", kChunks44, 6u, "6e053f683eaca92da333d783951baaba1ecaaba93c271467ca276de3a17f066e"},
    Range{"5c01ebb4d16c6ec7", 720u, 4480u, "lift_s000_02d0", kChunks45, 4u, "8b08b5ad623d9380b79b4ba76716dbfd85d98025eafa4dd3cc24f73b29d7b47d"},
    Range{"5c01ebb4d16c6ec7", 1392u, 4480u, "lift_s000_0570", kChunks46, 3u, "475f458e2cdaa4482077a5a149dc55e4fadf36e2df5e8cef752976ce6043ecab"},
    Range{"0e8e6b07f6aa2032", 0u, 4912u, "lift_s001_0000", kChunks47, 2u, "e06473bff02d053f5063998a533fad1e0c89130fd15e7afc14dd0366fca7ef38"},
    Range{"0e8e6b07f6aa2032", 64u, 4392u, "lift_s001_0040", kChunks48, 6u, "bdd8f37d8b46ba311f13926152a32b9565f939f7944043e112bce3260eae5e01"},
    Range{"0e8e6b07f6aa2032", 672u, 4392u, "lift_s001_02a0", kChunks49, 4u, "57613d3864a7d6f9e782b7b80e25db30e797d8028d317abfb9e2c177476eea2a"},
    Range{"0e8e6b07f6aa2032", 1304u, 4392u, "lift_s001_0518", kChunks50, 3u, "f133871d44028a9d34f857fd0d52860770c6be5996114fa037280a88d2f4f389"},
    Range{"015e21d85c2bd0e9", 0u, 5040u, "lift_s003_0000", kChunks51, 2u, "25732d7a989b8a32fc96b1b5f9195abee7d5ea86caf359337b294af725409b4c"},
    Range{"015e21d85c2bd0e9", 64u, 4520u, "lift_s003_0040", kChunks52, 6u, "97413c246cf0a2f956ed6a96a7b3a2faaeab148c90d3c90f46abad91b0b16cf7"},
    Range{"015e21d85c2bd0e9", 744u, 4520u, "lift_s003_02e8", kChunks53, 4u, "19ebd042a58c1885fd93c600968e175bf0a6ec9f28c1117b9efd1ac4c5e56ed5"},
    Range{"015e21d85c2bd0e9", 1432u, 4520u, "lift_s003_0598", kChunks54, 3u, "77a71f50a894f45a21b4f66736e7bc2429ebd9340534a7f2a46a40999569523c"},
    Range{"310a1b32431bf0ce", 64u, 1080u, "lift_s004_0040", kChunks55, 4u, "1e8b841fc6f215f8dd31186d4f569ff9fed114e1fe1ac4166a56a3421ff20bd4"},
    Range{"310a1b32431bf0ce", 552u, 616u, "lift_s004_0228", kChunks56, 1u, "dd5ac3d1c2c0e7c17c9bb040ff66a5299cab2a69d8159d04b49293221d88f259"},
    Range{"310a1b32431bf0ce", 1016u, 1080u, "lift_s004_03f8", kChunks57, 1u, "f48186ac952bb2e0a21131e38377ce85e77e4fd79dc658de97824bac1fd3ab63"},
    Range{"5069d87213eba9f0", 64u, 1080u, "lift_s005_0040", kChunks58, 4u, "d3a345bee4a16e06667be37788f24f81ff184ad326d7385cb64799828131a8db"},
    Range{"dfd11e091d859098", 0u, 1568u, "lift_s006_0000", kChunks59, 2u, "ec66ebcb0fd06ff5a70f8f3717ffc741ef8f3b40a5f3e55dc60cf112e984a33d"},
    Range{"dfd11e091d859098", 64u, 1120u, "lift_s006_0040", kChunks60, 4u, "14eeda03560b70f4cd5bd9beea4d732f651ea3d7d3b17be1eb495b95579befa6"},
    Range{"dfd11e091d859098", 568u, 632u, "lift_s006_0238", kChunks61, 1u, "d3f17db30a5c608b31124f0a46891c07740ecddb28171fdaddf4e61c45ef46d0"},
    Range{"dfd11e091d859098", 1056u, 1120u, "lift_s006_0420", kChunks62, 1u, "559a8f713343661892416f2991fd62e4ba9473aabe076b18cb62278f3080e4c5"},
    Range{"566e6614caa7ed2a", 64u, 1120u, "lift_s007_0040", kChunks63, 4u, "379d6527ae3b4dbeccc3b4040fce983fd634da8212276049ea3451326ecb8217"},
    Range{"799945d251643717", 0u, 5104u, "lift_s008_0000", kChunks64, 2u, "20e463e5b16207de83af048f7c266157bbcdb8befe0cece2fbba85b7f384613d"},
    Range{"799945d251643717", 64u, 4584u, "lift_s008_0040", kChunks65, 6u, "a0803f795eb213fc7eb30831594402543deb1b6a46bdb7386d70866fed0f4113"},
    Range{"799945d251643717", 776u, 4584u, "lift_s008_0308", kChunks66, 4u, "f436bb957fdd36a39308739e9a4948c00b4a51a2d2f251232539942bbdcdb1cc"},
    Range{"799945d251643717", 1488u, 4584u, "lift_s008_05d0", kChunks67, 3u, "ac83601c8980e691eea3a6891c4f3f89883a0a54176aadc21612d4fae1ff5f17"},
    Range{"976f5cf104f5947c", 0u, 4784u, "lift_s009_0000", kChunks68, 2u, "8181966e7a78ec079a9180a472f515373ecc54cc8d32ee150676c01ad9c137c5"},
    Range{"976f5cf104f5947c", 64u, 4368u, "lift_s009_0040", kChunks69, 6u, "a561573705086d4f4961d5c6c79aac2c8e5ce2e29576d3e238c2f1e465047707"},
    Range{"976f5cf104f5947c", 752u, 4368u, "lift_s009_02f0", kChunks70, 4u, "419ec2412fa3a4be79879acd37c902fb6edcb84b451dbe1e88e2df17789cc967"},
    Range{"976f5cf104f5947c", 1440u, 4368u, "lift_s009_05a0", kChunks71, 3u, "464d39590b89f66aefc79ba5a382b3379956cc846b77b5f8fd192e7d55baf092"},
    Range{"e2fd11a94db446b1", 0u, 4912u, "lift_s010_0000", kChunks72, 2u, "0ba3299bfb91d1b2da0b011660ae22a61c061c9d392755972959a1128381fe91"},
    Range{"e2fd11a94db446b1", 64u, 4496u, "lift_s010_0040", kChunks73, 6u, "b752c62fefd02e0cc795e95910cab12bc2c24482c4683667883f2a803a2a77aa"},
    Range{"e2fd11a94db446b1", 728u, 4496u, "lift_s010_02d8", kChunks74, 4u, "f10b358e525767ee162ea2caea751e35493ee63e02056cf06d9396d4158aff61"},
    Range{"e2fd11a94db446b1", 1392u, 4496u, "lift_s010_0570", kChunks75, 3u, "d7bb6c2566bdae6bdf5c35feb73529e5dd4d0e1928c891bf79b2f9eef6d83f09"},
    Range{"f8fd3b56e398e241", 64u, 4368u, "lift_s011_0040", kChunks76, 6u, "c1477ed1c679dc63a36ef471ff5d7f748055fda014114d08412a108cfca6cbb7"},
    Range{"f8fd3b56e398e241", 752u, 4368u, "lift_s011_02f0", kChunks77, 4u, "ffe874f6b791e9559b64bd7fad9e1d4258fd01fac978b9e7cf8eaecf452853ce"},
    Range{"f8fd3b56e398e241", 1440u, 4368u, "lift_s011_05a0", kChunks78, 3u, "9e6e58b5759b5f5684cf1f0056cd53513af8f1bc128461a6a2fe145fba302866"},
    Range{"2b4c83a962f06171", 0u, 4776u, "lift_s012_0000", kChunks79, 2u, "5b554edf3c4dd561395f149a1cfebb2fabe92806d4e09965a67d85e6eb607d55"},
    Range{"2b4c83a962f06171", 64u, 4360u, "lift_s012_0040", kChunks80, 6u, "74a671b1eb6622378af66329c623d3b51ccab3c3b87d38c9101dc87fac62667f"},
    Range{"2b4c83a962f06171", 760u, 4360u, "lift_s012_02f8", kChunks81, 4u, "d87dbf1b58fb818d167ca7b51f762e741b52dff50e3fc7360113c907a010974e"},
    Range{"2b4c83a962f06171", 1448u, 4360u, "lift_s012_05a8", kChunks82, 3u, "bab6a8c5029f37e7dc2e4db97fb2b9904d579166e6a998e2ec16b0e89ef6a18c"},
    Range{"f334ce7c727956f3", 0u, 4944u, "lift_s013_0000", kChunks83, 2u, "1bcbce507ccb4d998c9c222751650c90be7d3bc1451fe2b9d25a09f7c9f1c9b1"},
    Range{"f334ce7c727956f3", 64u, 4528u, "lift_s013_0040", kChunks84, 6u, "2f8c8dc589da6254df0b4c30c4dc8f1fe28ae8711c13725d7776500298c7d996"},
    Range{"f334ce7c727956f3", 728u, 4528u, "lift_s013_02d8", kChunks85, 4u, "5bf34e6ce3b0f433d7fc8e980ecb22a82eff8d6980baa83adde877cd003f815f"},
    Range{"f334ce7c727956f3", 1392u, 4528u, "lift_s013_0570", kChunks86, 3u, "f304adf46beaf3991dedbc0eef659b12df945bbd154bfd0617da54deb026c590"},
    Range{"94d63c1e63486363", 0u, 4832u, "lift_s014_0000", kChunks87, 2u, "5a761c0dc4284215cd596fd04e482203ef10de44d3496fd7bb363a241667f34f"},
    Range{"94d63c1e63486363", 64u, 4416u, "lift_s014_0040", kChunks88, 6u, "ff7bf84b4201dec89e1d41036b30912703c65b1915e7245ab4a7bab7716b47ab"},
    Range{"94d63c1e63486363", 760u, 4416u, "lift_s014_02f8", kChunks89, 4u, "a3a45b05e8e47a303fc53f0e406bdffeb813f2595789faea1fcec0ccac53290d"},
    Range{"94d63c1e63486363", 1456u, 4416u, "lift_s014_05b0", kChunks90, 3u, "64f652671bf26116cd7786bdfbb092a07bc49ebfe3a832b7213b6a1db7381cde"},
    Range{"2131ce900ef7b800", 0u, 912u, "lift_s015_0000", kChunks91, 2u, "19826a11b74232dda78cdb4fa0d8d5befb50081927da8abd62f5afdbc70f10cb"},
    Range{"2131ce900ef7b800", 64u, 424u, "lift_s015_0040", kChunks92, 2u, "7144cb5cf4c4bd0cb664e91074548777aa6f9ac749ac21d68fa5ba271fde5682"},
    Range{"2131ce900ef7b800", 432u, 464u, "lift_s015_01b0", kChunks93, 1u, "51c8545e09a7df91db28b4c940e273fa04822cc719f3432a14dd543efa569a3b"},
    Range{"32a8f5196be21450", 0u, 968u, "lift_s016_0000", kChunks94, 2u, "21d0d255f5b28dcd9f2c2f3fb19340d570ed36a0e3a2aebbac2c43614ca6b402"},
    Range{"32a8f5196be21450", 64u, 480u, "lift_s016_0040", kChunks95, 2u, "76ee482d8220ab197d4d411c511fe71096dc3330b54dff9da80f219f530f120a"},
    Range{"32a8f5196be21450", 488u, 520u, "lift_s016_01e8", kChunks96, 1u, "cc4b53ce6942b58aa251033259a49432a7d5fd7b15bc35bbeb0ed9b991f16d3a"},
    Range{"a486a0036d481f44", 64u, 424u, "lift_s017_0040", kChunks97, 2u, "c31cd83fe26f1bee146209b4342ece03f25632aabdbe071102de2686f38aee94"},
    Range{"a5c682adf57a7c90", 64u, 480u, "lift_s018_0040", kChunks98, 2u, "62776277add72ba3c6d45105471984ead2a2622d287b9b3bbb4eec0f966fdc90"},
    Range{"856664375decb405", 0u, 4416u, "lift_s019_0000", kChunks99, 2u, "6a2dcf3f8e025365eec17713c3b9ccb3eebd19be5eb46aca558fd72cf56a51b1"},
    Range{"856664375decb405", 64u, 4000u, "lift_s019_0040", kChunks100, 5u, "e6754deb1af9ce8e807e684d35cb303da9e3dd5628052f2be47488c89fdad903"},
    Range{"856664375decb405", 560u, 4000u, "lift_s019_0230", kChunks101, 3u, "9fa3df1a3859923f49accdc728e15df52af2824a6c83cf5f50ceb0d9868d098f"},
    Range{"856664375decb405", 1280u, 4000u, "lift_s019_0500", kChunks102, 2u, "c5d684c53a71899a2a2ebe0b70fb1a84476b9507f573a3327a0763b105fa44e9"},
    Range{"f0c9678466fd7c7a", 64u, 4000u, "lift_s020_0040", kChunks103, 5u, "dadd69f3675d4e177f112ccdf7046d6c51ffe7669c19b6f5c5a304d3d25e45d4"},
    Range{"f0c9678466fd7c7a", 560u, 4000u, "lift_s020_0230", kChunks104, 3u, "0b2080b64470b628369f70a6c160da0ce9889b641e0e6303203891f20c1b9b71"},
    Range{"f0c9678466fd7c7a", 1280u, 4000u, "lift_s020_0500", kChunks105, 2u, "2b2bbc7526a42a04a9db48dea80abaa7b60463cf74ed7d2cb194a9959a6abb7b"},
    Range{"9e4b3cbb9f9c8a25", 0u, 4504u, "lift_s021_0000", kChunks106, 2u, "07753df3cfc2095adb2e456d1994355c31996665c9d49c8bdbcc8d5f016426a0"},
    Range{"9e4b3cbb9f9c8a25", 64u, 4088u, "lift_s021_0040", kChunks107, 5u, "29107e01f1e3f16a2e9f1d27f0b8e5493b78551618680feb9515c6bc44f53bfe"},
    Range{"9e4b3cbb9f9c8a25", 584u, 4088u, "lift_s021_0248", kChunks108, 3u, "837a3c613ee3f670d39b286172b9aa866d1cc345152d28cbafb97b897e4ccd08"},
    Range{"9e4b3cbb9f9c8a25", 1328u, 4088u, "lift_s021_0530", kChunks109, 2u, "e93c72c88a55d44cf585ffb2a6c4d86f1b68e3ff28bd65bd421539c26e223c8a"},
    Range{"7d12bd54f475fd9a", 64u, 4088u, "lift_s022_0040", kChunks110, 5u, "361b46fe9559b780750bd0277d44fa3ef437727b991ba0a1af41b2a573d13486"},
    Range{"7d12bd54f475fd9a", 584u, 4088u, "lift_s022_0248", kChunks111, 3u, "004a868374613f28a0d22f06c9e5bb75f370b8279328799cb5809aec93047abf"},
    Range{"7d12bd54f475fd9a", 1328u, 4088u, "lift_s022_0530", kChunks112, 2u, "63234617bed9caf0bf4f4e962e46a7a90da0252c9bd4084a9899469cf2b317ef"},
    Range{"0f4d0dc5e900946e", 0u, 5104u, "lift_s023_0000", kChunks113, 2u, "a27f5290f6db7f879c038670df25be49d4275b2be8900433e47ced42e9908fcc"},
    Range{"0f4d0dc5e900946e", 64u, 4584u, "lift_s023_0040", kChunks114, 6u, "fe0687860f4c9d97127fec334df6feee3c57feaa8193ca673e031525e405dfb1"},
    Range{"0f4d0dc5e900946e", 744u, 4584u, "lift_s023_02e8", kChunks115, 4u, "439d044a5471eb289d5ff1e13f7264634bae3fcf463f828c7ff3817bbabb42cf"},
    Range{"0f4d0dc5e900946e", 1440u, 4584u, "lift_s023_05a0", kChunks116, 3u, "85f3c33516cfa910f786eb65b57d2b636c4d4569ff2b99a625bc48d02a2451db"},
    Range{"f716d887c43d4339", 0u, 4968u, "lift_s024_0000", kChunks117, 2u, "ca401f109d8d2416c7ce59b56a479a6b28a3a17eaa787451bcb0e6a633b5f168"},
    Range{"f716d887c43d4339", 64u, 4448u, "lift_s024_0040", kChunks118, 6u, "54161c0bcf29d8f2681f72340666d36a4d9d593ce84814f0c09abd5ec61d28df"},
    Range{"f716d887c43d4339", 672u, 4448u, "lift_s024_02a0", kChunks119, 4u, "b5e8367ceb008127608f80f7a64bc0823c7ed3fb60c22e48d876a9b724ee99a9"},
    Range{"f716d887c43d4339", 1296u, 4448u, "lift_s024_0510", kChunks120, 3u, "db563bfe68325a5575609d7fd1b527545395288fcb995ed808776625e541a978"},
    Range{"9fec4fa08195a3ad", 0u, 4904u, "lift_s025_0000", kChunks121, 2u, "4e5b52b1c81c7b89f58717658c8768d6443b79802c37f32aae9098e1f54605a8"},
    Range{"9fec4fa08195a3ad", 64u, 4448u, "lift_s025_0040", kChunks122, 6u, "97d6a092ea42328621bac9c2e26f05750df71c5622a62919c93f219293991c7d"},
    Range{"9fec4fa08195a3ad", 696u, 4448u, "lift_s025_02b8", kChunks123, 4u, "b8905457b8dea722cd5b8596104eb87eaac3b325ac428fd69caeb6a271cd0e1d"},
    Range{"9fec4fa08195a3ad", 1344u, 4448u, "lift_s025_0540", kChunks124, 3u, "04f773f67b642d57f822044038f4c0f4851335efabd0c57abf76fb1907d6197f"},
    Range{"c0d89980f21c0025", 0u, 4720u, "lift_s026_0000", kChunks125, 2u, "ce78844f985aeb890430635527ef45c0404112a1077d909806288b3af51b1473"},
    Range{"c0d89980f21c0025", 64u, 4304u, "lift_s026_0040", kChunks126, 6u, "d7f147b6b769ee794d5f95931c27aa7944ebdc0ac85c73eca8957851fb50623d"},
    Range{"c0d89980f21c0025", 688u, 4304u, "lift_s026_02b0", kChunks127, 4u, "9cae34bcf6740475b2632b39e49abdb1f1c86cfe534a7d56cd78e37f0dbe3bf6"},
    Range{"c0d89980f21c0025", 1328u, 4304u, "lift_s026_0530", kChunks128, 3u, "0464e762e1ac813a436ffde93ac5158453ce25f56eccd2c081758bb9c6b4ed0c"},
    Range{"66fddc4c20bdff04", 0u, 4912u, "lift_s027_0000", kChunks129, 2u, "3f05105a22eb50921940b067040577d8e24a69c17bd1c5b44511a9548437d75d"},
    Range{"66fddc4c20bdff04", 64u, 4496u, "lift_s027_0040", kChunks130, 6u, "8845f63a97f33a0e7ebe8c368f74895cac538e64c010bce6c27a86bc3c45be93"},
    Range{"66fddc4c20bdff04", 696u, 4496u, "lift_s027_02b8", kChunks131, 4u, "c35d3f1daebf201dd0c49e2d91d2ea5dba7f57518d3854cd51b60be4672228e3"},
    Range{"66fddc4c20bdff04", 1344u, 4496u, "lift_s027_0540", kChunks132, 3u, "9653d8e805aba039168e0f0ed29d67576ef293ac2ed563763bb132c49f9fbc39"},
    Range{"4006c8ef8227a119", 0u, 4736u, "lift_s028_0000", kChunks133, 2u, "2d74ec25fe88daad5b03bdf8f3063fc23e94fc230a2221538d235fb5bd651e09"},
    Range{"4006c8ef8227a119", 64u, 4320u, "lift_s028_0040", kChunks134, 6u, "bc2411614e70740d4e57ce6a160210aee37b3a6d7f9b51e991f41fb04c269dc4"},
    Range{"4006c8ef8227a119", 696u, 4320u, "lift_s028_02b8", kChunks135, 4u, "1a4cbd8499905e578c89a0e05302377c7119998cfaa503cc77ace85f701b4add"},
    Range{"4006c8ef8227a119", 1344u, 4320u, "lift_s028_0540", kChunks136, 3u, "6cb7f5c60ae0f29d328f2c544f9ab7b68881ead765e1f1f97cea99c1c7e530ac"},
    Range{"172d602b11bdb592", 0u, 4584u, "lift_s029_0000", kChunks137, 2u, "38981fec53a5ac8940d6d02890edc89971e6fba740f8d941397d014c3f1cf2fc"},
    Range{"172d602b11bdb592", 64u, 4168u, "lift_s029_0040", kChunks138, 6u, "6485b8dbdb3f234fea3e3a9d4da6cd25e65ff518dbcfda0ab59b13a50df5ea01"},
    Range{"172d602b11bdb592", 608u, 4168u, "lift_s029_0260", kChunks139, 4u, "5266852af815a97bfd1456b6edd246ddcae4e556afa3f216379d4059952e4c24"},
    Range{"172d602b11bdb592", 1176u, 4168u, "lift_s029_0498", kChunks140, 3u, "23676e6fdbbd7c5c23a49632bc0bff19b6b51a8e4d21d0230f9f285741ec5884"},
    Range{"64a30e46fbca6c72", 64u, 4168u, "lift_s030_0040", kChunks141, 6u, "382c5b5db219cd98b9c386f4132718ab735b694e3ab50f9d3e4de594b770721e"},
    Range{"64a30e46fbca6c72", 608u, 4168u, "lift_s030_0260", kChunks142, 4u, "319c944ba6f1b8e1743be688bf02924c31081ef26e18e8a13ba70fc4b9b4becd"},
    Range{"64a30e46fbca6c72", 1176u, 4168u, "lift_s030_0498", kChunks143, 3u, "be8bd83e7b0ca4740d3659d6d5dcce1e0f1d12a2517da08473241bff15d9aeb1"},
    Range{"084dd7f2fe53ff3d", 64u, 4336u, "lift_s031_0040", kChunks144, 6u, "d6dadac0a3423f9cf5aa7c3cd42a03d451c6d3a98e5bc6575488b60c0f1a0f16"},
    Range{"084dd7f2fe53ff3d", 608u, 4336u, "lift_s031_0260", kChunks145, 4u, "c9b8a50c29ba44583b15c19b2c0b498c8dbd8ddf13b9137eb314dff3a7dc17a4"},
    Range{"084dd7f2fe53ff3d", 1184u, 4336u, "lift_s031_04a0", kChunks146, 3u, "69ff60bd5a386c2f102b6081ce8825d72de836e66ff486ba1590d2505c03aee9"},
    Range{"e5cab341b7c91e52", 64u, 4328u, "lift_s032_0040", kChunks147, 6u, "61f15071c0f3e94ddc77c59a00cef75a7e85f3fab2e2e0cf8f700462a2cf3f90"},
    Range{"e5cab341b7c91e52", 608u, 4328u, "lift_s032_0260", kChunks148, 4u, "cb5ebd10c18033fe307ce497a6b08063b05987e1f99c55120ea066776831c6fb"},
    Range{"e5cab341b7c91e52", 1176u, 4328u, "lift_s032_0498", kChunks149, 3u, "ced0de8872d5ec88f660d4919157f646ee04597ef5ff807d7a70097eec680e14"},
    Range{"caf6f0ac3d1d07cd", 0u, 4600u, "lift_s033_0000", kChunks150, 2u, "acaf4e6ad997fc23d35c2928159df9c6b587e7e11768b486b69f00d8d1670784"},
    Range{"caf6f0ac3d1d07cd", 64u, 4184u, "lift_s033_0040", kChunks151, 6u, "72a4bbc13fe736fc056cf4b0428a6fa6fff11f5f73358cb3077957404430b1fb"},
    Range{"caf6f0ac3d1d07cd", 624u, 4184u, "lift_s033_0270", kChunks152, 4u, "76ac52fe0d9cd300018048997ed92a9664fd042fa0a220e6a1f905e6f4dd0e92"},
    Range{"caf6f0ac3d1d07cd", 1192u, 4184u, "lift_s033_04a8", kChunks153, 3u, "2783a20878dd982c9932650bb10fcf2cebd14525324ef5e0fc6e45b280825dda"},
    Range{"be41d8807a9332cc", 64u, 4368u, "lift_s034_0040", kChunks154, 6u, "2a62f3ae217f8e7ec508c8c46f40b3330af636ffe8f1223b75ba545cba7fd764"},
    Range{"be41d8807a9332cc", 624u, 4368u, "lift_s034_0270", kChunks155, 4u, "a84086e8447a5d9a0681ffd3df7da60e757e389d3057bdf0e42caf07f9bde98b"},
    Range{"be41d8807a9332cc", 1216u, 4368u, "lift_s034_04c0", kChunks156, 3u, "d7ba57c4863e44166df7b90cd5e7c5808de44647ee674a645601791eae40f60e"},
    Range{"d1d25e50d61ce39f", 64u, 4184u, "lift_s035_0040", kChunks157, 6u, "b8e53f5b46c5515f4fc72a0b04c15e5afde32f4f5ace0bf686af32ad2ebaa344"},
    Range{"d1d25e50d61ce39f", 624u, 4184u, "lift_s035_0270", kChunks158, 4u, "d37142878ece93da706e779d13d6dd3ea62d8181b7cddc8ece66fe3d5e501873"},
    Range{"d1d25e50d61ce39f", 1192u, 4184u, "lift_s035_04a8", kChunks159, 3u, "aaa3ff9cdf1d020982ad8f2d18a918932dd77da509d78134df6ac7cfbbf61ba8"},
    Range{"f9164378c256d28a", 0u, 4688u, "lift_s036_0000", kChunks160, 2u, "9c88922eca23136c89b36e829a2def9e9445a3391a2b0fdb94eb0183f4cea492"},
    Range{"f9164378c256d28a", 64u, 4272u, "lift_s036_0040", kChunks161, 6u, "10d4e15d248682153436a969187ebf7b9879bbdaea74ea55d1ba86b06562beee"},
    Range{"f9164378c256d28a", 680u, 4272u, "lift_s036_02a8", kChunks162, 4u, "4bbe334d3a2c596084b004e557ef2bec8775ea5ff299ac606f0f696dbeefb026"},
    Range{"f9164378c256d28a", 1312u, 4272u, "lift_s036_0520", kChunks163, 3u, "9739d74b9cd3c3829d897a6eefd230254f2db7ec1388789710d6fb77fcc91308"},
    Range{"ed40aa8c7e58d728", 0u, 4880u, "lift_s037_0000", kChunks164, 2u, "39d7274143d11d35f1c8df20f54f39b569a27340c0c5a97ce6f5c63487f72195"},
    Range{"ed40aa8c7e58d728", 64u, 4464u, "lift_s037_0040", kChunks165, 6u, "77e83aa3810ff1399c81705c0712d77d9b2c4bac035135046b18bd9c04618edc"},
    Range{"ed40aa8c7e58d728", 688u, 4464u, "lift_s037_02b0", kChunks166, 4u, "4a2a161fe53bd793db602628c771cf8ccf79d57ed8179607a316d15ee359a79e"},
    Range{"ed40aa8c7e58d728", 1328u, 4464u, "lift_s037_0530", kChunks167, 3u, "cbff7d31e0d8099b32da1a3a1deb6b6a8059af756ef66eecd26001119bd573d7"},
    Range{"72cad04fa3a64430", 0u, 4680u, "lift_s038_0000", kChunks168, 2u, "7e66f72bff2689a30dc94809a715fd9f931ea0d524ec4740e68056155e861de4"},
    Range{"72cad04fa3a64430", 64u, 4264u, "lift_s038_0040", kChunks169, 6u, "66259d9a958671df05b6d0daa9e0e0ff94d340f04bfabeca3bfe636614be7640"},
    Range{"72cad04fa3a64430", 680u, 4264u, "lift_s038_02a8", kChunks170, 4u, "0bd1cb434397519e753b2d8b1ffc6a44ccaeb274336598bf54a66a6c4f27185f"},
    Range{"72cad04fa3a64430", 1304u, 4264u, "lift_s038_0518", kChunks171, 3u, "0c89e69a1c3865978cec9b3a0651c9026b9305d5d7e99b7919b2e9fff305792b"},
    Range{"cd97201d4e356465", 592u, 4072u, "lift_s039_0250", kChunks172, 3u, "053a61cf1eb9ab9e7468ca40503846c8385a38016b78d1f89bf02988c0f53009"},
    Range{"cd97201d4e356465", 1152u, 4072u, "lift_s039_0480", kChunks173, 2u, "49c4fa2e4866e91424c62c31128e1b7a02b3ae082060f9b417dc9caf979de2c8"},
    Range{"3f15273b21329947", 64u, 4072u, "lift_s040_0040", kChunks174, 5u, "4818b3d9c395e4b432fc09aafd6d8d782af7297ae55e1029ee7efa9fd4e768b8"},
    Range{"3f15273b21329947", 592u, 4072u, "lift_s040_0250", kChunks175, 3u, "b825524055d540e6932159d00127609c1e4ed8ad22172d439e3583adf595e62a"},
    Range{"3f15273b21329947", 1152u, 4072u, "lift_s040_0480", kChunks176, 2u, "b84495ebd02bdbf22d532ecfb341b6d857c401273619d3a4aa84c039dc053013"},
    Range{"ddcf20f3c414cdb8", 608u, 4088u, "lift_s043_0260", kChunks177, 3u, "ea5b8b09e11c50fccf5594451782486db98dc5d5d26162055ec2a1e3cfbbb8dd"},
    Range{"1fc777c6df8717fd", 0u, 4520u, "lift_s045_0000", kChunks178, 2u, "fc3848019fa45a889a0036144989fa0885b00334bbda89f1624e6645d8cc8db3"},
    Range{"1fc777c6df8717fd", 64u, 4104u, "lift_s045_0040", kChunks179, 6u, "e3247e6d9df823f4984462b7bb5f61379cc3aa92e2f94b98fd3e6646a212d188"},
    Range{"1fc777c6df8717fd", 608u, 4104u, "lift_s045_0260", kChunks180, 4u, "7d99e24a6fd1d81c956c9637332489c54d8e4be1c9c401249ee18918d5909de8"},
    Range{"1fc777c6df8717fd", 1168u, 4104u, "lift_s045_0490", kChunks181, 3u, "ef3187ed80376b41c92bb651514ac7e9b7a9e9f495c8aaec75f330e10c51c59f"},
    Range{"5b44d896f9e8cf5e", 64u, 1120u, "lift_s046_0040", kChunks182, 4u, "bf3db8d9de70b76679c05ec1ee605e5886008eaa6fc1a136a87411c7843a132b"},
    Range{"7d58af6fba4a214a", 64u, 1120u, "lift_s047_0040", kChunks183, 4u, "2d9e42ba2478da89732223bb3a6b33a16f405bc893a6f091d76aafda825ad4c0"},
    Range{"2ee98e4e496de5ae", 64u, 1080u, "lift_s048_0040", kChunks184, 4u, "4a605206485b10e1b9f6d024953c6ff3d6592c880adbc42902e404600799ce82"},
    Range{"43816a26b39fd3f3", 1016u, 1080u, "lift_s049_03f8", kChunks185, 1u, "13fe8549187fcb438cd623af50cfa269e094c567cf74f5c0d6cebbd41b24519e"},
    Range{"8e478e396a680f3e", 64u, 4336u, "lift_s050_0040", kChunks186, 6u, "978d3ddfd816981756ed0fa819ea6b24b7435fac99edd1952fbe0bf334161866"},
    Range{"8e478e396a680f3e", 656u, 4336u, "lift_s050_0290", kChunks187, 4u, "57855b85251e12b7048251f557ecedf6824eaa80a0d6492be0e94a57b2407c7d"},
    Range{"8e478e396a680f3e", 1296u, 4336u, "lift_s050_0510", kChunks188, 3u, "32e019b3d81e48681001e7b96743a115690f88c2382963d5fd15c33694fe1391"},
    Range{"10c56c16dbd67bd8", 0u, 4968u, "lift_s051_0000", kChunks189, 2u, "b285c340e425d7aad6e4af45b1ceefd137b219692357b47a90c5af67ef98203c"},
    Range{"10c56c16dbd67bd8", 64u, 4360u, "lift_s051_0040", kChunks190, 6u, "cb285ac087b507df247f01bd50251f689694e7519e7ef4e2b70fc2d13aea98b7"},
    Range{"10c56c16dbd67bd8", 736u, 4360u, "lift_s051_02e0", kChunks191, 4u, "32f91d47071ac693881799a57be11c39ec33b74be102b8990ef37bfa9d043a09"},
    Range{"10c56c16dbd67bd8", 1416u, 4360u, "lift_s051_0588", kChunks192, 3u, "6ac74adeef78cdf28966301eb11521306b07052d84ed762ec275aa2b99ac25c2"},
    Range{"e992f45026be9de3", 0u, 4640u, "lift_s052_0000", kChunks193, 2u, "824d4d7121ee6af320ad3e86ade3d5701e70cf142d8b2919bc8db456218ac630"},
    Range{"e992f45026be9de3", 64u, 4224u, "lift_s052_0040", kChunks194, 6u, "242269ae14d57a2faa68694a4036c64d1ab7fd8c970603b4def66ec05a4a8250"},
    Range{"e992f45026be9de3", 656u, 4224u, "lift_s052_0290", kChunks195, 4u, "144a88691d9cb2eeb7d3c110c08468bf7a10832929c012e61ea0287293e448e7"},
    Range{"e992f45026be9de3", 1272u, 4224u, "lift_s052_04f8", kChunks196, 3u, "20555898746e6766cf2663453bdb46b63f3c0c821f959e0700be10fd668d3551"},
    Range{"725cf495f19b3df0", 584u, 4088u, "lift_s055_0248", kChunks197, 3u, "77043de143405ea858d43e944dfab564431e018b47e0270bbc9df2662d9dc1ba"},
    Range{"725cf495f19b3df0", 1136u, 4088u, "lift_s055_0470", kChunks198, 2u, "4a0925ccba324e39d9593adaf4b77ab68a7a1de20ca02d81510002db3c9ca923"},
    Range{"ca47f4fb5b538a87", 0u, 4496u, "lift_s056_0000", kChunks199, 2u, "4274ad10e9f39b5e733e2f99b06ca109a0da6596436a273a2046e0a32c4f40ca"},
    Range{"ca47f4fb5b538a87", 64u, 4080u, "lift_s056_0040", kChunks200, 5u, "c01d14f640d12fdcf3ef390f69a25eb2f06e2807413ed8857aa0da834affebec"},
    Range{"ca47f4fb5b538a87", 576u, 4080u, "lift_s056_0240", kChunks201, 3u, "fed49fad7b7e97a7d378e7b20b70b62cf9f4419d71c978c608a51cc9a56df595"},
    Range{"ca47f4fb5b538a87", 1120u, 4080u, "lift_s056_0460", kChunks202, 2u, "5d9afc46a4c1920a4b19b5aba14a691ba54b2053e33e7d6f89b935f46793771f"},
    Range{"dbb6d3e10d18d094", 0u, 4920u, "lift_s060_0000", kChunks203, 2u, "0716a2f7828ec0b48741ec35c43e00acd603d3cde54aa6bc5aef56d9002f5cf6"},
    Range{"dbb6d3e10d18d094", 64u, 4472u, "lift_s060_0040", kChunks204, 6u, "9ef9d265f3aae4d2cf3829d3b19b2feebbb89a652c6d6097ee6802f75dfcfc8b"},
    Range{"dbb6d3e10d18d094", 760u, 4472u, "lift_s060_02f8", kChunks205, 4u, "a44578158667d28bb541e81773841e241e6d9f27aed484c15a422a95ea70e32a"},
    Range{"dbb6d3e10d18d094", 1360u, 4472u, "lift_s060_0550", kChunks206, 3u, "feb0a227fab39592259775b097cbe83024986e6be1e81fcb0a9cd395b420418e"},
    Range{"ac6e703573e1105f", 0u, 4968u, "lift_s061_0000", kChunks207, 2u, "27656624268a5a9fa9c48eb525266fecfbd634c7759d65d165d4ed9359732337"},
    Range{"ac6e703573e1105f", 64u, 4520u, "lift_s061_0040", kChunks208, 6u, "72c35ee63c6ef9212d4b88f43f1dbcac93e63705b692c06f7f16ff350dba0242"},
    Range{"ac6e703573e1105f", 760u, 4520u, "lift_s061_02f8", kChunks209, 4u, "a541dafec6914feefb32d4f2b321b1434a303daffc4e8868fc838bf1f3775300"},
    Range{"ac6e703573e1105f", 1360u, 4520u, "lift_s061_0550", kChunks210, 3u, "028dbd5ba9d929d7912f219d9ffa2e3ae41f2309896206556dcd11e4dcc16f9b"},
    Range{"9377d41e206193e8", 656u, 4400u, "lift_s062_0290", kChunks211, 4u, "a4c3e87d36c964d40f48048ee9d1ef3a2a9b021b50cc205daf3b55cef8b03d76"},
    Range{"8c50335f7202e95a", 0u, 4904u, "lift_s063_0000", kChunks212, 2u, "68966fb96f944a5fa01b595f7dda77403418aa50eb4500bf2ef128fce86f97ef"},
    Range{"8c50335f7202e95a", 64u, 4488u, "lift_s063_0040", kChunks213, 6u, "90c12cefdab768bef1a36b13f2e22ab3c41ee1850ed1daface11ad3a6cb1cd47"},
    Range{"8c50335f7202e95a", 712u, 4488u, "lift_s063_02c8", kChunks214, 4u, "ba2e1460403b850f180089c9dcf12d9575adf39a79e045c45f9c422ee787a57d"},
    Range{"8c50335f7202e95a", 1360u, 4488u, "lift_s063_0550", kChunks215, 3u, "5754ec71a8ffc2dad27c1ddae2e34c7c93c42f71e549173c5f9e6b800b6698f7"},
    Range{"d7807fb8f414577e", 0u, 4832u, "lift_s064_0000", kChunks216, 2u, "ba7e8752016b4550a6f240dc0e9cbd82b19fd196d4a5fc749809030e2e394dc4"},
    Range{"d7807fb8f414577e", 64u, 4416u, "lift_s064_0040", kChunks217, 6u, "612089e4633d9494eb4110e25ca4b0c6e4c9f99b7090198d44920ae48b5ac2a6"},
    Range{"d7807fb8f414577e", 656u, 4416u, "lift_s064_0290", kChunks218, 4u, "9556b34d31f826424b70c2ca7439ad246ec777112df687ea811799561f2ca38f"},
    Range{"d7807fb8f414577e", 1272u, 4416u, "lift_s064_04f8", kChunks219, 3u, "74930f7b83c6e892b2e1c1717cacb187fa0e3f78e6ce8ac4212dddee74384fef"},
    Range{"ab5620dd11543e3b", 0u, 4936u, "lift_s065_0000", kChunks220, 2u, "7b9bc06063f8d7c594afc3876c276c10828c42c45821695d238da1dec1dfdb4f"},
    Range{"ab5620dd11543e3b", 64u, 4520u, "lift_s065_0040", kChunks221, 6u, "e033bc04a976f802dbc92896a284a0b59bf4f5ebffac8031c8ad2dd8f000328f"},
    Range{"ab5620dd11543e3b", 720u, 4520u, "lift_s065_02d0", kChunks222, 4u, "adb7c557e5f059f68e47d9639fa031c120e09ba52512b1882669e30d649ab10d"},
    Range{"ab5620dd11543e3b", 1376u, 4520u, "lift_s065_0560", kChunks223, 3u, "5fb9d443dcf5055722ff691b210b0861c18b2c75d27f71b6a558d2905f5aa4dd"},
    Range{"15c69ecb9984e122", 0u, 5000u, "lift_s066_0000", kChunks224, 2u, "a13cc6a61dfb1f0b2dc6fe0ac7e02d5c7d5a13dc2ea607f25733c0327975d817"},
    Range{"15c69ecb9984e122", 64u, 1712u, "lift_s066_0040", kChunks225, 4u, "5ca09afe406f4d67de521e6c163db92380825bc2141aef110af9c9f549cdf00c"},
    Range{"15c69ecb9984e122", 760u, 824u, "lift_s066_02f8", kChunks226, 1u, "1e80e4a6ed90d06d367278e4aa280ba7ce05fd02cf4f183fd5d412cc4dac97d4"},
    Range{"15c69ecb9984e122", 1648u, 1712u, "lift_s066_0670", kChunks227, 1u, "0665cf9881ac97b282ce42015c81c0a82c2073a3b1962d1bc71931137e696b1b"},
    Range{"ee7ae3e6ebe3d915", 0u, 4936u, "lift_s067_0000", kChunks228, 2u, "42b89427ec04e5cbcb5006ab57fee03740127ccfd2d3fcb3183e0d4ce6eec777"},
    Range{"ee7ae3e6ebe3d915", 64u, 4520u, "lift_s067_0040", kChunks229, 6u, "6b111d59d65d691280cbe5905da50cad30204c32aefbca533f97a125449565cb"},
    Range{"ee7ae3e6ebe3d915", 664u, 4520u, "lift_s067_0298", kChunks230, 4u, "4c60b88fca60f2dceee12206e85d992cac25826619acdb5b222ae640c0eaa384"},
    Range{"ee7ae3e6ebe3d915", 1312u, 4520u, "lift_s067_0520", kChunks231, 3u, "8ea46595bde5c70121204189aa0d29d45046adb953dbff9b3831763d9f6d697c"},
    Range{"8a60915b412129ab", 64u, 4496u, "lift_s068_0040", kChunks232, 6u, "b706be2f63bde6dd89cf83147ea0e46c3a1874606b7f5a212e35d95e2dd42e42"},
    Range{"8a60915b412129ab", 672u, 4496u, "lift_s068_02a0", kChunks233, 4u, "903d8e1d6550cb579d5c2074af850b1b36791fdd3427df152b3665639e3cd1d1"},
    Range{"8a60915b412129ab", 1336u, 4496u, "lift_s068_0538", kChunks234, 3u, "d11d92b42ed9def2ab79609406ba36e0a0ff4cc6a4cbb2d5fc8a43b24c429efc"},
    Range{"9d1d9c3b0b640eb9", 0u, 4944u, "lift_s069_0000", kChunks235, 2u, "e3962220ed74be8e62fd0e228f3df159ebcd15ec1c24801289ff83cbc232fce8"},
    Range{"9d1d9c3b0b640eb9", 64u, 4528u, "lift_s069_0040", kChunks236, 6u, "be552a0badc724bbad4b6c46f94649b58e6e0d33e1970d37417e079488678e1a"},
    Range{"9d1d9c3b0b640eb9", 688u, 4528u, "lift_s069_02b0", kChunks237, 4u, "4d801c92a441a26c07c051ba807a1d89853d72ce0f4d51288d11426942bb2c72"},
    Range{"9d1d9c3b0b640eb9", 1344u, 4528u, "lift_s069_0540", kChunks238, 3u, "30a772ec62d67636dccf2b5fb2b8f78807aa138dbaad001ed50859987a29ceb1"},
    Range{"8d21280e7601fea8", 0u, 4960u, "lift_s070_0000", kChunks239, 2u, "b2185d3c34c172edee8efe4cac3352231599932e89c1524726fdff1dcaef9af3"},
    Range{"8d21280e7601fea8", 64u, 4544u, "lift_s070_0040", kChunks240, 6u, "9170f41ed04ee0a7f3f27bda5b4ee190c9e46efbdad8e2c3e494ba8db5d85da2"},
    Range{"8d21280e7601fea8", 704u, 4544u, "lift_s070_02c0", kChunks241, 4u, "eaa7d527c3dcfa18bf85e435df52e4b0e9e246415a9eb66deb6fd8bc0b9c6ef2"},
    Range{"8d21280e7601fea8", 1392u, 4544u, "lift_s070_0570", kChunks242, 3u, "2c1d09f23b25cb67b80263cfcb5df2aed18cb087d51019cc48751810e755e273"},
    Range{"a92d1decf91da153", 0u, 4968u, "lift_s071_0000", kChunks243, 2u, "104e7fc3e5b5932a9e0a117bbc7f60927be6c53047946e97f6ccd893e7aaebe6"},
    Range{"a92d1decf91da153", 64u, 4512u, "lift_s071_0040", kChunks244, 6u, "d047729565cd07b73ccd7028e9805cc5ac9791c1d826b8c4490f4819263b89a8"},
    Range{"a92d1decf91da153", 720u, 4512u, "lift_s071_02d0", kChunks245, 4u, "08b6a80c9aab0abf201c5ea6e7422f7084d09d7e3fbeb1c3fb51b521e642c781"},
    Range{"a92d1decf91da153", 1400u, 4512u, "lift_s071_0578", kChunks246, 3u, "1b7040aaa9a829ee0477282bd03872d8625d34450e569b32d53e0ed4d6e39368"},
    Range{"47ebed33fbdf59e7", 64u, 4312u, "lift_s072_0040", kChunks247, 6u, "cefffffd8802b6076762bbb73d2e4a12e24df529f610b54e7603e836f2a5fb20"},
    Range{"47ebed33fbdf59e7", 648u, 4312u, "lift_s072_0288", kChunks248, 4u, "024769e1a447a464f9bdff5721a7465f4926e0b47eeeb459cf5741e8a42c52e2"},
    Range{"47ebed33fbdf59e7", 1272u, 4312u, "lift_s072_04f8", kChunks249, 3u, "ed23741bdf324c5d8118551ee9f2c69ff4ef9e00426fe45e0f654f6fee795f9c"},
    Range{"969094bca859cc71", 64u, 4496u, "lift_s073_0040", kChunks250, 6u, "20ecab6abce06ad58bd842e55b2f5503ec08958a006c894ab36acd69a2eab622"},
    Range{"969094bca859cc71", 656u, 4496u, "lift_s073_0290", kChunks251, 4u, "58c4b1da664b6e0b0929bfffc4efc8a7531f2058c527d8d6874a6faada670a9e"},
    Range{"969094bca859cc71", 1304u, 4496u, "lift_s073_0518", kChunks252, 3u, "254dcb3b05997ac7c019c6bbd3a6e8d33fcdb032bd3c240f8ff88936686c9b49"},
    Range{"d33a7ae6ebdbd992", 64u, 4496u, "lift_s074_0040", kChunks253, 6u, "39c4e628c1e07ba1e5906b5be53283f1057c4fb131b0e1a700c3bd8ab95d6175"},
    Range{"d33a7ae6ebdbd992", 656u, 4496u, "lift_s074_0290", kChunks254, 4u, "a04718fb165bd797580a79695ac96657de878689214d8ae293fc798abb4bb81d"},
    Range{"d33a7ae6ebdbd992", 1304u, 4496u, "lift_s074_0518", kChunks255, 3u, "e5d8de19333d681ce64a66e2ca71803e12a6c8d15af8374c78753442e346465f"},
}};
inline constexpr std::array<Target, 256> kTargets{{
    Target{"861b6b064f6661e4", 624u, 4328u, "lift_861b6b06_0270"},
    Target{"3319183d22d5916c", 1440u, 4560u, "lift_3319183d_05a0"},
    Target{"861b6b064f6661e4", 64u, 4328u, "lift_861b6b06_0040"},
    Target{"3319183d22d5916c", 744u, 4560u, "lift_3319183d_02e8"},
    Target{"77216489d3c195e2", 1280u, 4368u, "lift_77216489_0500"},
    Target{"3319183d22d5916c", 64u, 4560u, "lift_3319183d_0040"},
    Target{"77216489d3c195e2", 664u, 4368u, "lift_77216489_0298"},
    Target{"028bc7209ee72601", 1200u, 4304u, "lift_028bc720_04b0"},
    Target{"28152de151fbea2c", 760u, 4544u, "lift_28152de1_02f8"},
    Target{"77216489d3c195e2", 64u, 4368u, "lift_77216489_0040"},
    Target{"028bc7209ee72601", 608u, 4304u, "lift_028bc720_0260"},
    Target{"36ba79dadb53b9a2", 1200u, 4312u, "lift_36ba79da_04b0"},
    Target{"028bc7209ee72601", 64u, 4304u, "lift_028bc720_0040"},
    Target{"77216489d3c195e2", 0u, 4824u, "lift_77216489_0000"},
    Target{"028bc7209ee72601", 0u, 4720u, "lift_028bc720_0000"},
    Target{"b4ffb301c16d1842", 64u, 4088u, "lift_b4ffb301_0040"},
    Target{"28152de151fbea2c", 64u, 4544u, "lift_28152de1_0040"},
    Target{"80a3120c4692781f", 1224u, 4328u, "lift_80a3120c_04c8"},
    Target{"36ba79dadb53b9a2", 608u, 4312u, "lift_36ba79da_0260"},
    Target{"861b6b064f6661e4", 0u, 4744u, "lift_861b6b06_0000"},
    Target{"36ba79dadb53b9a2", 64u, 4312u, "lift_36ba79da_0040"},
    Target{"132215b679e1aa49", 608u, 4288u, "lift_132215b6_0260"},
    Target{"f098ac393f1da878", 608u, 4336u, "lift_f098ac39_0260"},
    Target{"e4a20a8ab9286b0a", 0u, 4704u, "lift_e4a20a8a_0000"},
    Target{"b4ffb301c16d1842", 1168u, 4088u, "lift_b4ffb301_0490"},
    Target{"3319183d22d5916c", 0u, 5168u, "lift_3319183d_0000"},
    Target{"28152de151fbea2c", 1368u, 4544u, "lift_28152de1_0558"},
    Target{"e4a20a8ab9286b0a", 64u, 4288u, "lift_e4a20a8a_0040"},
    Target{"36ba79dadb53b9a2", 0u, 4728u, "lift_36ba79da_0000"},
    Target{"f098ac393f1da878", 64u, 4336u, "lift_f098ac39_0040"},
    Target{"f098ac393f1da878", 0u, 4752u, "lift_f098ac39_0000"},
    Target{"132215b679e1aa49", 1200u, 4288u, "lift_132215b6_04b0"},
    Target{"f098ac393f1da878", 1200u, 4336u, "lift_f098ac39_04b0"},
    Target{"b4ffb301c16d1842", 0u, 4504u, "lift_b4ffb301_0000"},
    Target{"28152de151fbea2c", 0u, 4992u, "lift_28152de1_0000"},
    Target{"5e9c28ca679e97bf", 0u, 4488u, "lift_5e9c28ca_0000"},
    Target{"5e9c28ca679e97bf", 64u, 4072u, "lift_5e9c28ca_0040"},
    Target{"65d23cf872f398f9", 64u, 4088u, "lift_65d23cf8_0040"},
    Target{"e9c4677aeaae0f09", 0u, 4816u, "lift_e9c4677a_0000"},
    Target{"e9c4677aeaae0f09", 1272u, 4400u, "lift_e9c4677a_04f8"},
    Target{"61fe8c48bbb8373b", 0u, 1528u, "lift_61fe8c48_0000"},
    Target{"e9c4677aeaae0f09", 64u, 4400u, "lift_e9c4677a_0040"},
    Target{"61fe8c48bbb8373b", 64u, 1080u, "lift_61fe8c48_0040"},
    Target{"5c01ebb4d16c6ec7", 0u, 5000u, "lift_s000_0000"},
    Target{"5c01ebb4d16c6ec7", 64u, 4480u, "lift_s000_0040"},
    Target{"5c01ebb4d16c6ec7", 720u, 4480u, "lift_s000_02d0"},
    Target{"5c01ebb4d16c6ec7", 1392u, 4480u, "lift_s000_0570"},
    Target{"0e8e6b07f6aa2032", 0u, 4912u, "lift_s001_0000"},
    Target{"0e8e6b07f6aa2032", 64u, 4392u, "lift_s001_0040"},
    Target{"0e8e6b07f6aa2032", 672u, 4392u, "lift_s001_02a0"},
    Target{"0e8e6b07f6aa2032", 1304u, 4392u, "lift_s001_0518"},
    Target{"015e21d85c2bd0e9", 0u, 5040u, "lift_s003_0000"},
    Target{"015e21d85c2bd0e9", 64u, 4520u, "lift_s003_0040"},
    Target{"015e21d85c2bd0e9", 744u, 4520u, "lift_s003_02e8"},
    Target{"015e21d85c2bd0e9", 1432u, 4520u, "lift_s003_0598"},
    Target{"310a1b32431bf0ce", 64u, 1080u, "lift_s004_0040"},
    Target{"310a1b32431bf0ce", 552u, 616u, "lift_s004_0228"},
    Target{"310a1b32431bf0ce", 1016u, 1080u, "lift_s004_03f8"},
    Target{"5069d87213eba9f0", 64u, 1080u, "lift_s005_0040"},
    Target{"dfd11e091d859098", 0u, 1568u, "lift_s006_0000"},
    Target{"dfd11e091d859098", 64u, 1120u, "lift_s006_0040"},
    Target{"dfd11e091d859098", 568u, 632u, "lift_s006_0238"},
    Target{"dfd11e091d859098", 1056u, 1120u, "lift_s006_0420"},
    Target{"566e6614caa7ed2a", 64u, 1120u, "lift_s007_0040"},
    Target{"799945d251643717", 0u, 5104u, "lift_s008_0000"},
    Target{"799945d251643717", 64u, 4584u, "lift_s008_0040"},
    Target{"799945d251643717", 776u, 4584u, "lift_s008_0308"},
    Target{"799945d251643717", 1488u, 4584u, "lift_s008_05d0"},
    Target{"976f5cf104f5947c", 0u, 4784u, "lift_s009_0000"},
    Target{"976f5cf104f5947c", 64u, 4368u, "lift_s009_0040"},
    Target{"976f5cf104f5947c", 752u, 4368u, "lift_s009_02f0"},
    Target{"976f5cf104f5947c", 1440u, 4368u, "lift_s009_05a0"},
    Target{"e2fd11a94db446b1", 0u, 4912u, "lift_s010_0000"},
    Target{"e2fd11a94db446b1", 64u, 4496u, "lift_s010_0040"},
    Target{"e2fd11a94db446b1", 728u, 4496u, "lift_s010_02d8"},
    Target{"e2fd11a94db446b1", 1392u, 4496u, "lift_s010_0570"},
    Target{"f8fd3b56e398e241", 64u, 4368u, "lift_s011_0040"},
    Target{"f8fd3b56e398e241", 752u, 4368u, "lift_s011_02f0"},
    Target{"f8fd3b56e398e241", 1440u, 4368u, "lift_s011_05a0"},
    Target{"2b4c83a962f06171", 0u, 4776u, "lift_s012_0000"},
    Target{"2b4c83a962f06171", 64u, 4360u, "lift_s012_0040"},
    Target{"2b4c83a962f06171", 760u, 4360u, "lift_s012_02f8"},
    Target{"2b4c83a962f06171", 1448u, 4360u, "lift_s012_05a8"},
    Target{"f334ce7c727956f3", 0u, 4944u, "lift_s013_0000"},
    Target{"f334ce7c727956f3", 64u, 4528u, "lift_s013_0040"},
    Target{"f334ce7c727956f3", 728u, 4528u, "lift_s013_02d8"},
    Target{"f334ce7c727956f3", 1392u, 4528u, "lift_s013_0570"},
    Target{"94d63c1e63486363", 0u, 4832u, "lift_s014_0000"},
    Target{"94d63c1e63486363", 64u, 4416u, "lift_s014_0040"},
    Target{"94d63c1e63486363", 760u, 4416u, "lift_s014_02f8"},
    Target{"94d63c1e63486363", 1456u, 4416u, "lift_s014_05b0"},
    Target{"2131ce900ef7b800", 0u, 912u, "lift_s015_0000"},
    Target{"2131ce900ef7b800", 64u, 424u, "lift_s015_0040"},
    Target{"2131ce900ef7b800", 432u, 464u, "lift_s015_01b0"},
    Target{"32a8f5196be21450", 0u, 968u, "lift_s016_0000"},
    Target{"32a8f5196be21450", 64u, 480u, "lift_s016_0040"},
    Target{"32a8f5196be21450", 488u, 520u, "lift_s016_01e8"},
    Target{"a486a0036d481f44", 64u, 424u, "lift_s017_0040"},
    Target{"a5c682adf57a7c90", 64u, 480u, "lift_s018_0040"},
    Target{"856664375decb405", 0u, 4416u, "lift_s019_0000"},
    Target{"856664375decb405", 64u, 4000u, "lift_s019_0040"},
    Target{"856664375decb405", 560u, 4000u, "lift_s019_0230"},
    Target{"856664375decb405", 1280u, 4000u, "lift_s019_0500"},
    Target{"f0c9678466fd7c7a", 64u, 4000u, "lift_s020_0040"},
    Target{"f0c9678466fd7c7a", 560u, 4000u, "lift_s020_0230"},
    Target{"f0c9678466fd7c7a", 1280u, 4000u, "lift_s020_0500"},
    Target{"9e4b3cbb9f9c8a25", 0u, 4504u, "lift_s021_0000"},
    Target{"9e4b3cbb9f9c8a25", 64u, 4088u, "lift_s021_0040"},
    Target{"9e4b3cbb9f9c8a25", 584u, 4088u, "lift_s021_0248"},
    Target{"9e4b3cbb9f9c8a25", 1328u, 4088u, "lift_s021_0530"},
    Target{"7d12bd54f475fd9a", 64u, 4088u, "lift_s022_0040"},
    Target{"7d12bd54f475fd9a", 584u, 4088u, "lift_s022_0248"},
    Target{"7d12bd54f475fd9a", 1328u, 4088u, "lift_s022_0530"},
    Target{"0f4d0dc5e900946e", 0u, 5104u, "lift_s023_0000"},
    Target{"0f4d0dc5e900946e", 64u, 4584u, "lift_s023_0040"},
    Target{"0f4d0dc5e900946e", 744u, 4584u, "lift_s023_02e8"},
    Target{"0f4d0dc5e900946e", 1440u, 4584u, "lift_s023_05a0"},
    Target{"f716d887c43d4339", 0u, 4968u, "lift_s024_0000"},
    Target{"f716d887c43d4339", 64u, 4448u, "lift_s024_0040"},
    Target{"f716d887c43d4339", 672u, 4448u, "lift_s024_02a0"},
    Target{"f716d887c43d4339", 1296u, 4448u, "lift_s024_0510"},
    Target{"9fec4fa08195a3ad", 0u, 4904u, "lift_s025_0000"},
    Target{"9fec4fa08195a3ad", 64u, 4448u, "lift_s025_0040"},
    Target{"9fec4fa08195a3ad", 696u, 4448u, "lift_s025_02b8"},
    Target{"9fec4fa08195a3ad", 1344u, 4448u, "lift_s025_0540"},
    Target{"c0d89980f21c0025", 0u, 4720u, "lift_s026_0000"},
    Target{"c0d89980f21c0025", 64u, 4304u, "lift_s026_0040"},
    Target{"c0d89980f21c0025", 688u, 4304u, "lift_s026_02b0"},
    Target{"c0d89980f21c0025", 1328u, 4304u, "lift_s026_0530"},
    Target{"66fddc4c20bdff04", 0u, 4912u, "lift_s027_0000"},
    Target{"66fddc4c20bdff04", 64u, 4496u, "lift_s027_0040"},
    Target{"66fddc4c20bdff04", 696u, 4496u, "lift_s027_02b8"},
    Target{"66fddc4c20bdff04", 1344u, 4496u, "lift_s027_0540"},
    Target{"4006c8ef8227a119", 0u, 4736u, "lift_s028_0000"},
    Target{"4006c8ef8227a119", 64u, 4320u, "lift_s028_0040"},
    Target{"4006c8ef8227a119", 696u, 4320u, "lift_s028_02b8"},
    Target{"4006c8ef8227a119", 1344u, 4320u, "lift_s028_0540"},
    Target{"172d602b11bdb592", 0u, 4584u, "lift_s029_0000"},
    Target{"172d602b11bdb592", 64u, 4168u, "lift_s029_0040"},
    Target{"172d602b11bdb592", 608u, 4168u, "lift_s029_0260"},
    Target{"172d602b11bdb592", 1176u, 4168u, "lift_s029_0498"},
    Target{"64a30e46fbca6c72", 64u, 4168u, "lift_s030_0040"},
    Target{"64a30e46fbca6c72", 608u, 4168u, "lift_s030_0260"},
    Target{"64a30e46fbca6c72", 1176u, 4168u, "lift_s030_0498"},
    Target{"084dd7f2fe53ff3d", 64u, 4336u, "lift_s031_0040"},
    Target{"084dd7f2fe53ff3d", 608u, 4336u, "lift_s031_0260"},
    Target{"084dd7f2fe53ff3d", 1184u, 4336u, "lift_s031_04a0"},
    Target{"e5cab341b7c91e52", 64u, 4328u, "lift_s032_0040"},
    Target{"e5cab341b7c91e52", 608u, 4328u, "lift_s032_0260"},
    Target{"e5cab341b7c91e52", 1176u, 4328u, "lift_s032_0498"},
    Target{"caf6f0ac3d1d07cd", 0u, 4600u, "lift_s033_0000"},
    Target{"caf6f0ac3d1d07cd", 64u, 4184u, "lift_s033_0040"},
    Target{"caf6f0ac3d1d07cd", 624u, 4184u, "lift_s033_0270"},
    Target{"caf6f0ac3d1d07cd", 1192u, 4184u, "lift_s033_04a8"},
    Target{"be41d8807a9332cc", 64u, 4368u, "lift_s034_0040"},
    Target{"be41d8807a9332cc", 624u, 4368u, "lift_s034_0270"},
    Target{"be41d8807a9332cc", 1216u, 4368u, "lift_s034_04c0"},
    Target{"d1d25e50d61ce39f", 64u, 4184u, "lift_s035_0040"},
    Target{"d1d25e50d61ce39f", 624u, 4184u, "lift_s035_0270"},
    Target{"d1d25e50d61ce39f", 1192u, 4184u, "lift_s035_04a8"},
    Target{"f9164378c256d28a", 0u, 4688u, "lift_s036_0000"},
    Target{"f9164378c256d28a", 64u, 4272u, "lift_s036_0040"},
    Target{"f9164378c256d28a", 680u, 4272u, "lift_s036_02a8"},
    Target{"f9164378c256d28a", 1312u, 4272u, "lift_s036_0520"},
    Target{"ed40aa8c7e58d728", 0u, 4880u, "lift_s037_0000"},
    Target{"ed40aa8c7e58d728", 64u, 4464u, "lift_s037_0040"},
    Target{"ed40aa8c7e58d728", 688u, 4464u, "lift_s037_02b0"},
    Target{"ed40aa8c7e58d728", 1328u, 4464u, "lift_s037_0530"},
    Target{"72cad04fa3a64430", 0u, 4680u, "lift_s038_0000"},
    Target{"72cad04fa3a64430", 64u, 4264u, "lift_s038_0040"},
    Target{"72cad04fa3a64430", 680u, 4264u, "lift_s038_02a8"},
    Target{"72cad04fa3a64430", 1304u, 4264u, "lift_s038_0518"},
    Target{"cd97201d4e356465", 592u, 4072u, "lift_s039_0250"},
    Target{"cd97201d4e356465", 1152u, 4072u, "lift_s039_0480"},
    Target{"3f15273b21329947", 64u, 4072u, "lift_s040_0040"},
    Target{"3f15273b21329947", 592u, 4072u, "lift_s040_0250"},
    Target{"3f15273b21329947", 1152u, 4072u, "lift_s040_0480"},
    Target{"ddcf20f3c414cdb8", 608u, 4088u, "lift_s043_0260"},
    Target{"1fc777c6df8717fd", 0u, 4520u, "lift_s045_0000"},
    Target{"1fc777c6df8717fd", 64u, 4104u, "lift_s045_0040"},
    Target{"1fc777c6df8717fd", 608u, 4104u, "lift_s045_0260"},
    Target{"1fc777c6df8717fd", 1168u, 4104u, "lift_s045_0490"},
    Target{"5b44d896f9e8cf5e", 64u, 1120u, "lift_s046_0040"},
    Target{"7d58af6fba4a214a", 64u, 1120u, "lift_s047_0040"},
    Target{"2ee98e4e496de5ae", 64u, 1080u, "lift_s048_0040"},
    Target{"43816a26b39fd3f3", 1016u, 1080u, "lift_s049_03f8"},
    Target{"8e478e396a680f3e", 64u, 4336u, "lift_s050_0040"},
    Target{"8e478e396a680f3e", 656u, 4336u, "lift_s050_0290"},
    Target{"8e478e396a680f3e", 1296u, 4336u, "lift_s050_0510"},
    Target{"10c56c16dbd67bd8", 0u, 4968u, "lift_s051_0000"},
    Target{"10c56c16dbd67bd8", 64u, 4360u, "lift_s051_0040"},
    Target{"10c56c16dbd67bd8", 736u, 4360u, "lift_s051_02e0"},
    Target{"10c56c16dbd67bd8", 1416u, 4360u, "lift_s051_0588"},
    Target{"e992f45026be9de3", 0u, 4640u, "lift_s052_0000"},
    Target{"e992f45026be9de3", 64u, 4224u, "lift_s052_0040"},
    Target{"e992f45026be9de3", 656u, 4224u, "lift_s052_0290"},
    Target{"e992f45026be9de3", 1272u, 4224u, "lift_s052_04f8"},
    Target{"725cf495f19b3df0", 584u, 4088u, "lift_s055_0248"},
    Target{"725cf495f19b3df0", 1136u, 4088u, "lift_s055_0470"},
    Target{"ca47f4fb5b538a87", 0u, 4496u, "lift_s056_0000"},
    Target{"ca47f4fb5b538a87", 64u, 4080u, "lift_s056_0040"},
    Target{"ca47f4fb5b538a87", 576u, 4080u, "lift_s056_0240"},
    Target{"ca47f4fb5b538a87", 1120u, 4080u, "lift_s056_0460"},
    Target{"dbb6d3e10d18d094", 0u, 4920u, "lift_s060_0000"},
    Target{"dbb6d3e10d18d094", 64u, 4472u, "lift_s060_0040"},
    Target{"dbb6d3e10d18d094", 760u, 4472u, "lift_s060_02f8"},
    Target{"dbb6d3e10d18d094", 1360u, 4472u, "lift_s060_0550"},
    Target{"ac6e703573e1105f", 0u, 4968u, "lift_s061_0000"},
    Target{"ac6e703573e1105f", 64u, 4520u, "lift_s061_0040"},
    Target{"ac6e703573e1105f", 760u, 4520u, "lift_s061_02f8"},
    Target{"ac6e703573e1105f", 1360u, 4520u, "lift_s061_0550"},
    Target{"9377d41e206193e8", 656u, 4400u, "lift_s062_0290"},
    Target{"8c50335f7202e95a", 0u, 4904u, "lift_s063_0000"},
    Target{"8c50335f7202e95a", 64u, 4488u, "lift_s063_0040"},
    Target{"8c50335f7202e95a", 712u, 4488u, "lift_s063_02c8"},
    Target{"8c50335f7202e95a", 1360u, 4488u, "lift_s063_0550"},
    Target{"d7807fb8f414577e", 0u, 4832u, "lift_s064_0000"},
    Target{"d7807fb8f414577e", 64u, 4416u, "lift_s064_0040"},
    Target{"d7807fb8f414577e", 656u, 4416u, "lift_s064_0290"},
    Target{"d7807fb8f414577e", 1272u, 4416u, "lift_s064_04f8"},
    Target{"ab5620dd11543e3b", 0u, 4936u, "lift_s065_0000"},
    Target{"ab5620dd11543e3b", 64u, 4520u, "lift_s065_0040"},
    Target{"ab5620dd11543e3b", 720u, 4520u, "lift_s065_02d0"},
    Target{"ab5620dd11543e3b", 1376u, 4520u, "lift_s065_0560"},
    Target{"15c69ecb9984e122", 0u, 5000u, "lift_s066_0000"},
    Target{"15c69ecb9984e122", 64u, 1712u, "lift_s066_0040"},
    Target{"15c69ecb9984e122", 760u, 824u, "lift_s066_02f8"},
    Target{"15c69ecb9984e122", 1648u, 1712u, "lift_s066_0670"},
    Target{"ee7ae3e6ebe3d915", 0u, 4936u, "lift_s067_0000"},
    Target{"ee7ae3e6ebe3d915", 64u, 4520u, "lift_s067_0040"},
    Target{"ee7ae3e6ebe3d915", 664u, 4520u, "lift_s067_0298"},
    Target{"ee7ae3e6ebe3d915", 1312u, 4520u, "lift_s067_0520"},
    Target{"8a60915b412129ab", 64u, 4496u, "lift_s068_0040"},
    Target{"8a60915b412129ab", 672u, 4496u, "lift_s068_02a0"},
    Target{"8a60915b412129ab", 1336u, 4496u, "lift_s068_0538"},
    Target{"9d1d9c3b0b640eb9", 0u, 4944u, "lift_s069_0000"},
    Target{"9d1d9c3b0b640eb9", 64u, 4528u, "lift_s069_0040"},
    Target{"9d1d9c3b0b640eb9", 688u, 4528u, "lift_s069_02b0"},
    Target{"9d1d9c3b0b640eb9", 1344u, 4528u, "lift_s069_0540"},
    Target{"8d21280e7601fea8", 0u, 4960u, "lift_s070_0000"},
    Target{"8d21280e7601fea8", 64u, 4544u, "lift_s070_0040"},
    Target{"8d21280e7601fea8", 704u, 4544u, "lift_s070_02c0"},
    Target{"8d21280e7601fea8", 1392u, 4544u, "lift_s070_0570"},
    Target{"a92d1decf91da153", 0u, 4968u, "lift_s071_0000"},
    Target{"a92d1decf91da153", 64u, 4512u, "lift_s071_0040"},
    Target{"a92d1decf91da153", 720u, 4512u, "lift_s071_02d0"},
    Target{"a92d1decf91da153", 1400u, 4512u, "lift_s071_0578"},
    Target{"47ebed33fbdf59e7", 64u, 4312u, "lift_s072_0040"},
    Target{"47ebed33fbdf59e7", 648u, 4312u, "lift_s072_0288"},
    Target{"47ebed33fbdf59e7", 1272u, 4312u, "lift_s072_04f8"},
    Target{"969094bca859cc71", 64u, 4496u, "lift_s073_0040"},
    Target{"969094bca859cc71", 656u, 4496u, "lift_s073_0290"},
    Target{"969094bca859cc71", 1304u, 4496u, "lift_s073_0518"},
    Target{"d33a7ae6ebdbd992", 64u, 4496u, "lift_s074_0040"},
    Target{"d33a7ae6ebdbd992", 656u, 4496u, "lift_s074_0290"},
    Target{"d33a7ae6ebdbd992", 1304u, 4496u, "lift_s074_0518"},
}};
// END GENERATED
inline constexpr const char *kPrologue = R"DBZIW(#include "runtime/ps2_vu1_lift.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

using vl::V;

inline float vuMinMax(float left, float right, bool maximum)
{
    uint32_t a, b;
    std::memcpy(&a, &left, sizeof(a));
    std::memcpy(&b, &right, sizeof(b));
    const auto key = [](uint32_t bits) {
        return (bits & 0x80000000u) ? ~bits : (bits ^ 0x80000000u);
    };
    const bool leftFirst = key(a) < key(b);
    const uint32_t selected = maximum ? (leftFirst ? b : a) : (leftFirst ? a : b);
    float result;
    std::memcpy(&result, &selected, sizeof(result));
    return result;
}

inline V vuMinMaxV(V a, V b, bool maximum)
{
    alignas(16) float fa[4], fb[4], fr[4];
    _mm_store_ps(fa, a);
    _mm_store_ps(fb, b);
    for (int i = 0; i < 4; ++i)
        fr[i] = vuMinMax(fa[i], fb[i], maximum);
    return _mm_load_ps(fr);
}

inline int32_t vuFloatToInt(float value, float scale)
{
    const double scaled = static_cast<double>(value) * static_cast<double>(scale);
    if (scaled >= static_cast<double>(std::numeric_limits<int32_t>::max()))
        return std::numeric_limits<int32_t>::max();
    if (scaled <= static_cast<double>(std::numeric_limits<int32_t>::min()))
        return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(scaled);
}

inline V vuFtoiV(V a, float scale)
{
    alignas(16) float fa[4];
    alignas(16) uint32_t ir[4];
    _mm_store_ps(fa, a);
    for (int i = 0; i < 4; ++i)
        ir[i] = static_cast<uint32_t>(vuFloatToInt(fa[i], scale));
    return _mm_castsi128_ps(_mm_load_si128(reinterpret_cast<const __m128i *>(ir)));
}

inline V vuItofV(V a, float scale)
{
    alignas(16) uint32_t ir[4];
    alignas(16) float fr[4];
    _mm_store_si128(reinterpret_cast<__m128i *>(ir), _mm_castps_si128(a));
    for (int i = 0; i < 4; ++i)
        fr[i] = static_cast<float>(static_cast<int32_t>(ir[i])) / scale;
    return _mm_load_ps(fr);
}

} // namespace
)DBZIW";

} // namespace dbziw::vu1lift

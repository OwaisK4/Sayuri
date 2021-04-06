#pragma once

#include <cstdint>

#define MAX_BOARD_SIZE (19)

static constexpr int kDefaultBoardSize = 19;
static constexpr float kDefaultKomi = 7.5f;


static constexpr int kBoardSize = MAX_BOARD_SIZE;
static constexpr int kLetterBoxSize = kBoardSize+2;
static constexpr int kNumIntersections = kBoardSize * kBoardSize;
static constexpr int kNumVertices = kLetterBoxSize * kLetterBoxSize;

static constexpr int kNullVertex = 0;
static constexpr int kPass = kNumVertices + 1;
static constexpr int kResign = kNumVertices + 2;

static constexpr int kMaxGTPBoardSize = 25;
static constexpr int kMinGTPBoardSize = 2;

static_assert(kDefaultBoardSize >= kBoardSize, "");
static_assert(kDefaultBoardSize >= kMinGTPBoardSize, "");
static_assert(kMaxGTPBoardSize >= kBoardSize, "");
static_assert(kMinGTPBoardSize <= kBoardSize, "");

#define BLACK_NUMBER (0)
#define WHITE_NUMBER (1)
#define EMPTY_NUMBER (2)
#define INVAL_NUMBER (3)

#define NBR_SHIFT (4)
#define BLACK_NBR_SHIFT (BLACK_NUMBER * 4)
#define WHITE_NBR_SHIFT (WHITE_NUMBER * 4)
#define EMPTY_NBR_SHIFT (EMPTY_NUMBER * 4)

#define NBR_MASK (0xf)
#define BLACK_EYE_MASK (4 * (1 << BLACK_NBR_SHIFT))
#define WHITE_EYE_MASK (4 * (1 << WHITE_NBR_SHIFT))

enum VertexType : std::uint8_t {
    kBlack   = BLACK_NUMBER,
    kWhite   = WHITE_NUMBER,
    kEmpty   = EMPTY_NUMBER,
    kInvalid = INVAL_NUMBER,
};

static constexpr int kNeighborShift = NBR_SHIFT;
static constexpr int kBlackNeighborShift = BLACK_NBR_SHIFT;
static constexpr int kWhiteNeighborShift = WHITE_NBR_SHIFT;
static constexpr int kEmptyNeighborShift = EMPTY_NBR_SHIFT;

static constexpr std::uint16_t kNeighborMask = NBR_MASK;
static constexpr std::uint16_t kBlackEyeMask = BLACK_EYE_MASK;
static constexpr std::uint16_t kWhiteEyeMask = WHITE_EYE_MASK;
static constexpr int kEyeMask[2] = {BLACK_EYE_MASK, WHITE_EYE_MASK};

enum GameResult : std::uint8_t {
    kBlackWon = BLACK_NUMBER,
    kWhiteWon = WHITE_NUMBER,
    kDraw     = EMPTY_NUMBER,
    kUndecide = INVAL_NUMBER,
};

enum RuleType {
    KTrompTaylor,
    kJappenese,
};

static constexpr int kMaxLadderNodes = 2000;

enum LadderType {
    kGoodForHunter,
    kGoodForPrey,
    kGoodForNeither,

    kLadderDeath,
    kLadderEscapable,
    kLadderAtari,
    kLadderTake,
    kNotLadder,
};

static constexpr int KLibertiesReset = 16384;

#undef MAX_BOARD_SIZE
#undef BLACK_NUMBER
#undef WHITE_NUMBER
#undef EMPTY_NUMBER
#undef INVAL_NUMBER
#undef NBR_SHIFT
#undef BLACK_NBR_SHIFT
#undef WHITE_NBR_SHIFT
#undef EMPTY_NBR_SHIFT
#undef NBR_MASK
#undef BLACK_EYE_MASK
#undef WHITE_EYE_MASK

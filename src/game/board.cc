#include "game/board.h"

#include <algorithm>

int Board::ComputeScoreOnBoard(int black_bonus) const {
    const auto black = ComputeReachColor(kBlack);
    const auto white = ComputeReachColor(kWhite);
    return black - white + black_bonus;
}

float Board::ComputeFinalScore(float komi) const {
    return static_cast<float>(ComputeScoreOnBoard(0)) - komi;
}

std::vector<int> Board::GetSimpleOwnership() const {
    auto res = std::vector<int>(num_intersections_, kInvalid);

    auto black = std::vector<bool>(num_intersections_, false);
    auto white = std::vector<bool>(num_intersections_, false);
    auto PeekState = [&](int vtx) -> int {
        return state_[vtx];
    };

    ComputeReachColor(kBlack, kEmpty, black, PeekState);
    ComputeReachColor(kWhite, kEmpty, white, PeekState);

    for (int y = 0; y < board_size_; ++y) {
        for (int x = 0; x < board_size_; ++x) {
            const auto idx = GetIndex(x, y);
            const auto vtx = GetVertex(x, y);

            if ((black[vtx] && white[vtx]) || (!black[vtx] && !white[vtx])) {
                //The point is seki if there are no stones on board.
                res[idx] = kEmpty;
            } else if (black[vtx]) {
                res[idx] = kBlack;  
            } else if (white[vtx]) {
                res[idx] = kWhite;
            }
       }
    }

    return res;
}

bool Board::ValidHandicap(int handicap) {
    if (handicap < 2 || handicap > 9) {
        return false;
    }
    if (board_size_ % 2 == 0 && handicap > 4) {
        return false;
    }
    if (board_size_ == 7 && handicap > 4) {
        return false;
    }
    if (board_size_ < 7 && handicap > 0) {
        return false;
    }

    return true;
}

bool Board::SetFixdHandicap(int handicap) {
    if (!ValidHandicap(handicap)) {
        return false;
    }

    int high = board_size_ >= 13 ? 3 : 2;
    int mid = board_size_ / 2;

    int low = board_size_ - 1 - high;
    if (handicap >= 2) {
        PlayMoveAssumeLegal(GetVertex(low, low),  kBlack);
        PlayMoveAssumeLegal(GetVertex(high, high),  kBlack);
    }

    if (handicap >= 3) {
        PlayMoveAssumeLegal(GetVertex(high, low), kBlack);
    }

    if (handicap >= 4) {
        PlayMoveAssumeLegal(GetVertex(low, high), kBlack);
    }

    if (handicap >= 5 && handicap % 2 == 1) {
        PlayMoveAssumeLegal(GetVertex(mid, mid), kBlack);
    }

    if (handicap >= 6) {
        PlayMoveAssumeLegal(GetVertex(low, mid), kBlack);
        PlayMoveAssumeLegal(GetVertex(high, mid), kBlack);
    }

    if (handicap >= 8) {
        PlayMoveAssumeLegal(GetVertex(mid, low), kBlack);
        PlayMoveAssumeLegal(GetVertex(mid, high), kBlack);
    }

    SetToMove(kWhite);
    SetLastMove(kNullVertex);
    SetMoveNumber(0);

    return true;
}

bool Board::SetFreeHandicap(std::vector<int> movelist) {
    for (const auto vtx : movelist) {
        if (IsLegalMove(vtx, kBlack)) {
            PlayMoveAssumeLegal(vtx, kBlack);
        } else {
            return false;
        }
    }

    SetToMove(kWhite);
    SetLastMove(kNullVertex);
    SetMoveNumber(0);

    return true;
}

std::vector<LadderType> Board::GetLadderPlane() const {
    auto res = std::vector<LadderType>(GetNumIntersections(), LadderType::kNotLadder);
    auto ladder = std::vector<int>{};
    auto not_ladder = std::vector<int>{};

    const auto VectorFind = [](std::vector<int> &arr, int element) -> bool {
        auto begin = std::begin(arr);
        auto end = std::end(arr);
        return std::find(begin, end, element) != end;
    };

    auto boardsize = GetBoardSize();
    for (int y = 0; y < boardsize; ++y) {
        for (int x = 0; x < boardsize; ++x) {
            const auto idx = GetIndex(x, y);
            const auto vtx = GetVertex(x, y);

            auto first_found = false;
            int libs = 0;
            auto parent = strings_.GetParent(vtx);

            if (VectorFind(ladder, parent)) {
                // Found! It is a ladder.
                libs = strings_.GetLiberty(parent);
            } else if (!VectorFind(not_ladder, parent)) {
                // Not found! Now Search it.
                if (IsLadder(vtx)) {
                    // It is a ladder.
                    ladder.emplace_back(parent);
                    first_found = true; 
                    libs = strings_.GetLiberty(parent);
                } else {
                    // It is not a ladder.
                    not_ladder.emplace_back(parent);
                    continue;
                }
            } else {
                // It is not a ladder.
                continue;
            }

            assert(libs == 1 || libs == 2);
            if (libs == 1) {
                // The ladder string is already death.
                res[idx] = LadderType::kLadderDeath;
            } else {
                // The ladder string has a chance to escape.
                res[idx] = LadderType::kLadderEscapable;
            }

            if (first_found) {
                auto buf = std::vector<int>{};
                auto move_num = FindStringLiberties(vtx, buf);
            #ifdef NDEBUG
                (void)move_num;
            #else
                assert(move_num == libs);
            #endif
                for (const auto &v : buf) {
                    const auto ax = GetX(v);
                    const auto ay = GetY(v);
                    const auto aidx = GetIndex(ax, ay); 
                    if (libs == 1) {
                        // Someone can capture this ladder string.
                        res[aidx] = LadderType::kLadderTake;
                    } else {
                        // Someone can atari this ladder string.
                        res[aidx] = LadderType::kLadderAtari;
                    }
                }
            }
        }
    }

    return res;
}

std::vector<bool> Board::GetOcupiedPlane(const int color) const {
    auto res = std::vector<bool>(GetNumIntersections(), false);
    auto bsize = GetBoardSize();
    for (int y = 0; y < bsize; ++y) {
        for (int x = 0; x < bsize; ++x) {
            const auto state = GetState(x, y);
            const auto index = GetIndex(x, y);
            if (state == color) {
                res[index] = true;
            }
        }
    }

    return res;
}


#pragma once

#include <vector>

#include "game/simple_board.h"
#include "game/types.h"

class Board : public SimpleBoard {
public:
    // Compute final score with Tromp Taylor rule.
    float ComputeFinalScore(float komi) const;

    // Compute score on board(without komi) with Tromp Taylor rule.
    int ComputeScoreOnBoard() const;

    // Compute score area with Tromp Taylor rule.
    void ComputeScoreArea(std::vector<int> &result) const;

    // Get ladder type map.
    // LadderType::kLadderDeath means that the ladder string is already death.
    // LadderType::kLadderEscapable means that the ladder string has a chance to escape.
    // LadderType::kLadderTake means that someone can take the opponent's ladder strings.
    // LadderType::kLadderAtari means that someone can atari opponent's ladder string.
    std::vector<LadderType> GetLadderMap() const;

    // Compute all safe area which both players don't need to play move in.
    // Mark all seki points if mark_seki is true.
    void ComputeSafeArea(std::vector<bool> &result, bool mark_seki) const;

    // Compute seki area.
    void ComputeSekiPoints(std::vector<bool> &result) const;

private:
    // Compute all pass-alive string.
    // Mark all vital regions of pass-alive string if mark_vitals is true.
    // Mark all pass-dead regions if mark_pass_dead is true.
    void ComputePassAliveArea(std::vector<bool> &result,
                                  const int color,
                                  bool mark_vitals,
                                  bool mark_pass_dead) const;

    void ComputeReachArea(std::vector<int> &result) const;

    bool IsPassAliveString(const int vertex,
                               bool allow_sucide,
                               const std::vector<bool> &vitals,
                               const std::vector<int> &features,
                               const std::vector<int> &regions_index,
                               const std::vector<int> &regions_next,
                               const std::vector<int> &strings_index,
                               const std::vector<int> &strings_next) const;

    bool IsPassDeadRegion(const int vertex,
                               const int color,
                               bool allow_sucide,
                               std::vector<int> &features,
                               const std::vector<int> &regions_next) const;

    std::vector<int> GatherVertices(std::vector<bool> &buf) const;

    std::vector<int> ClassifyGroups(const int target,
                                        std::vector<int> &features,
                                        std::vector<int> &regions_index,
                                        std::vector<int> &regions_next) const;

    void ComputeInnerRegions(const int vtx,
                                 const int color,
                                 const std::vector<int> &regions_next,
                                 std::vector<bool> &inner_regions) const;
};

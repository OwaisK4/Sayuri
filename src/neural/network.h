#pragma once

#include "neural/network_basic.h"
#include "neural/description.h"
#include "game/game_state.h"
#include "utils/cache.h"

#include <memory>
#include <array>
#include <algorithm>
#include <cmath>
#include <string>
#include <atomic>

class Network {
public:
    enum Ensemble {
        kNone, kDirect, kRandom
    };

    using Inputs = InputData;
    using Result = OutputResult;
    using Cache = HashKeyCache<Result>;
    using PolicyVertexPair = std::pair<float, int>;

    void Initialize(const std::string &weights);
    void Destroy();
    bool Valid() const;

    int GetVertexWithPolicy(const GameState &state,
                            const float temperature,
                            const bool allow_pass);

    Result GetOutput(const GameState &state,
                     const Ensemble ensemble,
                     const float temperature = 1.f,
                     int symmetry = -1,
                     const bool read_cache = true,
                     const bool write_cache = true);

    std::string GetOutputString(const GameState &state,
                                const Ensemble ensemble,
                                int symmetry = -1);

    void Reload(int board_size);

    void SetCacheSize(size_t MiB);
    void ClearCache();

    size_t GetNumQueries() const;

private:
    void ActivatePolicy(Result &result, const float temperature) const;

    bool ProbeCache(const GameState &state, Result &result);

    Result GetOutputInternal(const GameState &state, const int symmetry);

    Network::Result DummyForward(const Network::Inputs& inputs) const;

    std::unique_ptr<NetworkForwardPipe> pipe_{nullptr};
    Cache nn_cache_;

    bool no_cache_;
    bool early_symm_cache_;
    size_t cache_memory_mib_;

    std::atomic<size_t> num_queries_;
};

#pragma once

#include <memory>
#include <string>

/// Minimal in-process sampling profiler for the calling thread (Windows only; no-op elsewhere).
/// Periodically suspends the profiled thread, records its call stack and, on `report`,
/// symbolizes the samples through the PDB to print self/inclusive hot functions.
class sampling_profiler {
public:
    sampling_profiler();
    ~sampling_profiler();
    sampling_profiler(const sampling_profiler&) = delete;
    auto operator=(const sampling_profiler&) -> sampling_profiler& = delete;

    /// Starts sampling the thread that calls this function.
    auto start() -> void;
    auto stop() -> void;
    /// Hot function table, `top_n` rows each for self and inclusive time.
    auto report(int top_n) const -> std::string;

private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

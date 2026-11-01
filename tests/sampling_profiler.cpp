#include "sampling_profiler.h"

#if defined(_WIN32) && defined(_M_X64)

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <format>
#include <ranges>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
// dbghelp.h must follow windows.h
#include <dbghelp.h>
#include <timeapi.h>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "winmm.lib")

namespace {

constexpr auto max_depth = 48;
constexpr auto max_samples = 200'000;

/// Walks the suspended thread's stack without allocating, so the target can't hold a lock we need.
auto capture_stack(CONTEXT& ctx, std::uintptr_t* out) -> int {
    auto depth = 0;
    while (depth < max_depth && ctx.Rip != 0) {
        out[depth++] = static_cast<std::uintptr_t>(ctx.Rip);
        auto image_base = DWORD64{0};
        auto* const entry = RtlLookupFunctionEntry(ctx.Rip, &image_base, nullptr);
        if (entry == nullptr) {
            // Leaf function: return address sits on top of the stack.
            ctx.Rip = *reinterpret_cast<const DWORD64*>(ctx.Rsp);
            ctx.Rsp += 8;
            continue;
        }
        auto* handler_data = static_cast<void*>(nullptr);
        auto establisher = DWORD64{0};
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, ctx.Rip, entry, &ctx, &handler_data,
                         &establisher, nullptr);
    }
    return depth;
}

struct symbol_info {
    std::uintptr_t start = 0;
    std::string name;
};

struct hot_row {
    std::string name;
    int count = 0;
};

} // namespace

struct sampling_profiler::impl {
    std::vector<std::uintptr_t> frames = std::vector<std::uintptr_t>(
        static_cast<std::size_t>(max_depth) * max_samples);
    std::vector<int> depths = std::vector<int>(max_samples);
    std::atomic<int> sample_count = 0;
    std::atomic<bool> running = false;
    HANDLE target = nullptr;
    std::thread sampler;

    auto loop() -> void {
        timeBeginPeriod(1);
        while (running.load() && sample_count.load() < max_samples) {
            Sleep(1);
            if (SuspendThread(target) == static_cast<DWORD>(-1)) { break; }
            auto ctx = CONTEXT{};
            ctx.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(target, &ctx)) {
                const auto i = sample_count.load();
                depths[i] = capture_stack(ctx, &frames[static_cast<std::size_t>(i) * max_depth]);
                sample_count.store(i + 1);
            }
            ResumeThread(target);
        }
        timeEndPeriod(1);
    }
};

sampling_profiler::sampling_profiler() : pimpl(std::make_unique<impl>()) {}

sampling_profiler::~sampling_profiler() {
    stop();
    if (pimpl->target != nullptr) { CloseHandle(pimpl->target); }
}

auto sampling_profiler::start() -> void {
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &pimpl->target,
                    THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE,
                    0);
    pimpl->running = true;
    pimpl->sampler = std::thread([this]() { pimpl->loop(); });
}

auto sampling_profiler::stop() -> void {
    pimpl->running = false;
    if (pimpl->sampler.joinable()) { pimpl->sampler.join(); }
}

auto sampling_profiler::report(const int top_n) const -> std::string {
    namespace ranges = std::ranges;

    const auto process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);

    auto cache = std::unordered_map<std::uintptr_t, symbol_info>();
    const auto resolve = [&](const std::uintptr_t addr) -> const symbol_info& {
        if (const auto it = cache.find(addr); it != cache.end()) { return it->second; }
        auto buffer = std::vector<char>(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
        auto* const sym = reinterpret_cast<SYMBOL_INFO*>(buffer.data());
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = MAX_SYM_NAME;
        auto displacement = DWORD64{0};
        auto info = symbol_info{.start = addr, .name = std::format("0x{:x}", addr)};
        if (SymFromAddr(process, addr, &displacement, sym)) {
            info = {.start = static_cast<std::uintptr_t>(sym->Address), .name = sym->Name};
        }
        return cache.emplace(addr, std::move(info)).first->second;
    };

    auto self = std::unordered_map<std::uintptr_t, hot_row>();
    auto inclusive = std::unordered_map<std::uintptr_t, hot_row>();
    // leaf function -> "parent < grandparent < ..." chain -> sample count
    auto callers = std::unordered_map<std::uintptr_t, std::unordered_map<std::string, int>>();
    const auto samples = pimpl->sample_count.load();
    for (int s = 0; s < samples; ++s) {
        const auto* const stack = &pimpl->frames[static_cast<std::size_t>(s) * max_depth];
        const auto depth = pimpl->depths[s];
        if (depth == 0) { continue; }
        const auto& leaf = resolve(stack[0]);
        auto& leaf_row = self[leaf.start];
        leaf_row.name = leaf.name;
        ++leaf_row.count;

        auto chain = std::string();
        auto chain_len = 0;
        auto prev = leaf.start;
        for (int d = 1; d < depth && chain_len < 4; ++d) {
            const auto& fn = resolve(stack[d] - 1);
            if (fn.start == prev) { continue; }
            prev = fn.start;
            chain += (chain_len++ == 0 ? "" : " < ") + fn.name.substr(0, 60);
        }
        ++callers[leaf.start][chain];

        auto seen = std::unordered_set<std::uintptr_t>();
        for (int d = 0; d < depth; ++d) {
            // Return addresses point past the call; step back so the call site resolves.
            const auto& fn = resolve(d == 0 ? stack[d] : stack[d] - 1);
            if (!seen.insert(fn.start).second) { continue; }
            auto& row = inclusive[fn.start];
            row.name = fn.name;
            ++row.count;
        }
    }
    SymCleanup(process);

    const auto table = [&](const std::unordered_map<std::uintptr_t, hot_row>& rows,
                           const std::string& title) {
        auto sorted = rows | std::views::values | ranges::to<std::vector>();
        ranges::sort(sorted, ranges::greater{}, &hot_row::count);
        auto out = std::format("== {} ({} samples) ==\n", title, samples);
        for (const auto& row : sorted | std::views::take(top_n)) {
            out += std::format("{:6.2f}%  {}\n", 100.0 * row.count / std::max(samples, 1),
                               row.name.substr(0, 150));
        }
        return out;
    };
    auto sorted_self = self | std::views::keys | ranges::to<std::vector>();
    ranges::sort(sorted_self, ranges::greater{}, [&](const auto key) { return self.at(key).count; });
    auto caller_report = std::string("== callers of hottest self functions ==\n");
    for (const auto key : sorted_self | std::views::take(12)) {
        caller_report += std::format("{}:\n", self.at(key).name.substr(0, 120));
        auto chains = callers[key] | ranges::to<std::vector<std::pair<std::string, int>>>();
        ranges::sort(chains, ranges::greater{}, &std::pair<std::string, int>::second);
        for (const auto& [chain, count] : chains | std::views::take(4)) {
            caller_report += std::format("    {:5}  {}\n", count, chain);
        }
    }
    return table(self, "self") + "\n" + table(inclusive, "inclusive") + "\n" + caller_report;
}

#else

struct sampling_profiler::impl {};

sampling_profiler::sampling_profiler() : pimpl(std::make_unique<impl>()) {}
sampling_profiler::~sampling_profiler() = default;
auto sampling_profiler::start() -> void {}
auto sampling_profiler::stop() -> void {}
auto sampling_profiler::report(const int /*top_n*/) const -> std::string {
    return "sampling profiler is only available on Windows x64\n";
}

#endif

#include "vtest.hpp"

#include "core/engine.hpp"

#include <cerrno>

using namespace vhdp::core;
using vhdp::Code;
using vhdp::Result;
using vhdp::Status;

namespace {

class FakeEngine final : public Engine {
public:
    FakeEngine(EngineKind k, bool avail) : kind_(k), avail_(avail) {}
    EngineKind kind() const noexcept override { return kind_; }
    ProbeReport probe() const override {
        ProbeReport r;
        r.available = avail_;
        r.reason = avail_ ? "ok" : "nope";
        return r;
    }
    Result<std::unique_ptr<EngineInstance>> create_instance(std::shared_ptr<const ValidatedConfig>,
                                                            EventSink&,
                                                            std::uint64_t) const override {
        return Status{Code::internal, "unused"};
    }

private:
    EngineKind kind_;
    bool avail_;
};

} // namespace

VTEST(engine_select) {
    FakeEngine rootless_ok(EngineKind::rootless, true);
    FakeEngine rootless_bad(EngineKind::rootless, false);
    FakeEngine rooted(EngineKind::rooted, false);

    std::vector<const Engine*> with_ok = {&rootless_ok, &rooted};
    auto a = select_engine(EngineKind::auto_select, with_ok);
    REQUIRE(a.is_ok());
    CHECK_EQ(a.value().engine->kind(), EngineKind::rootless);

    // auto with no available engine fails (never silently picks rooted).
    std::vector<const Engine*> none_ok = {&rootless_bad, &rooted};
    auto b = select_engine(EngineKind::auto_select, none_ok);
    CHECK(!b.is_ok());
    CHECK_EQ(b.status().code(), Code::engine_unavailable);

    // Explicit request for an unavailable engine is an error, not a fallback.
    auto c = select_engine(EngineKind::rooted, with_ok);
    CHECK(!c.is_ok());
    CHECK_EQ(c.status().code(), Code::engine_unavailable);

    // Explicit rootless when available.
    auto d = select_engine(EngineKind::rootless, with_ok);
    REQUIRE(d.is_ok());
    CHECK_EQ(d.value().engine->kind(), EngineKind::rootless);

    // Shell status mapping.
    ExitResult r;
    r.reason = ExitReason::normal;
    r.exit_code = 37;
    CHECK_EQ(shell_status(r), 37);
    r.reason = ExitReason::signaled;
    r.term_signal = 2;
    CHECK_EQ(shell_status(r), 130);
    r.reason = ExitReason::timeout;
    CHECK_EQ(shell_status(r), 124);
    r.reason = ExitReason::start_failed;
    r.exec_errno = ENOENT;
    CHECK_EQ(shell_status(r), 127);
    r.exec_errno = EACCES;
    CHECK_EQ(shell_status(r), 126);
}

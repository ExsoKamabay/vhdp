#include "vtest.hpp"

#include "core/lifecycle.hpp"

#include <chrono>
#include <thread>

using namespace vhdp::core;

VTEST(lifecycle_transitions) {
    CHECK(can_transition(State::created, State::starting));
    CHECK(can_transition(State::created, State::cancelled));
    CHECK(!can_transition(State::created, State::running));
    CHECK(can_transition(State::starting, State::running));
    CHECK(can_transition(State::starting, State::failed));
    CHECK(can_transition(State::running, State::exited));
    CHECK(can_transition(State::running, State::cancelled));
    CHECK(!can_transition(State::exited, State::running));
    CHECK(is_terminal(State::exited));
    CHECK(is_terminal(State::failed));
    CHECK(is_terminal(State::cancelled));
    CHECK(!is_terminal(State::running));

    StateMachine sm;
    CHECK_EQ(sm.get(), State::created);
    CHECK(sm.transition(State::starting).is_ok());
    CHECK(!sm.transition(State::created).is_ok()); // invalid, unchanged
    CHECK_EQ(sm.get(), State::starting);
    CHECK(sm.transition(State::running).is_ok());
    CHECK(!sm.wait_terminal(std::chrono::milliseconds(0)));

    std::thread t([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        (void)sm.transition(State::exited);
    });
    CHECK(sm.wait_terminal(std::chrono::milliseconds(2000)));
    CHECK_EQ(sm.get(), State::exited);
    t.join();
}

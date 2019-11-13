#pragma once

// Schedules a lambda to be run exactly once; normally at destruction time
// but can be explicitly run with 'Do()'
template <typename T>
struct Defer {
    explicit Defer(T Lambda) : Lambda(Lambda), Killed(false) {}
    ~Defer() { if (!Killed) Lambda(); }
    Defer(const Defer &) = delete;
    Defer& operator=(const Defer&) = delete;
    void Kill() { Killed = true; }
    void Do() { if (!Killed) Lambda(); Killed = true; }
private:
    T Lambda;
    bool Killed;
};

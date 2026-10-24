#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include <stdexcept>

struct EqualizerState {
    int preset = 0;
    int bass = 0;
    std::vector<int> bands = std::vector<int>(5, 0);
    uint8_t payloadType = 0;

    bool manual() const { return preset == (payloadType == 1 ? 0xff : 0xa0); }
    bool restorable() const {
        if (payloadType > 1 || (preset == 0xff && payloadType != 1) || (preset == 0xa0 && payloadType != 0)) return false;
        return (preset >= 0 && preset <= 7) || (preset >= 16 && preset <= 23) ||
            (preset >= 0xa0 && preset <= 0xa5) || (preset == 0xff && payloadType == 1);
    }
    bool matches(const EqualizerState& other) const {
        return preset == other.preset && payloadType == other.payloadType &&
            (!manual() || (bass == other.bass && bands == other.bands));
    }
};

// UI-thread state machine. Sending and readback verification happen separately.
class EqualizerPreview {
public:
    struct Action {
        EqualizerState target;
        bool writeCurve = false;
        bool restoring = false;
        unsigned long revision = 0;
    };
    void begin(const EqualizerState& actual) {
        *this = EqualizerPreview{};
        _baseline = _actual = _draft = actual;
        _active = _initialized = true;
    }
    void recover(const EqualizerState& original, const EqualizerState& actual) {
        begin(actual);
        _baseline = original;
        restore(true);
    }
    void edit(const EqualizerState& target, bool writeCurve, double now) {
        if (!_active || _restoring) return;
        _draft = target;
        _writeCurve = writeCurve;
        _lastEdit = now;
        ++_revision;
    }
    void restore(bool finish = false) {
        if (!_active) return;
        _finishAfterRestore |= finish;
        if (!_restoring) ++_revision;
        _restoring = true;
        _draft = _baseline;
        settleRestore();
    }
    std::optional<Action> next(double now) {
        if (!_active || _inFlight) return std::nullopt;
        settleRestore();
        if (!_active || (!_restoring && (now - _lastEdit < 0.15 || _draft.matches(_actual))))
            return std::nullopt;
        if (_restoring && _baseline.matches(_actual)) return std::nullopt;
        _inFlight = Action{_restoring ? _baseline : _draft,
            _restoring ? _baseline.manual() : _writeCurve, _restoring, _revision};
        return _inFlight;
    }
    void confirmed(const EqualizerState& actual) {
        if (!_inFlight) throw std::logic_error("No EQ preview transaction in progress");
        const auto sent = *_inFlight;
        if (sent.restoring && !sent.target.matches(actual))
            throw std::invalid_argument("EQ restore did not match the original state");
        _actual = actual;
        _inFlight.reset();
        if (!_restoring && sent.revision == _revision) _draft = actual;
        settleRestore();
    }
    bool canApply() const {
        return _active && !_restoring && !_inFlight && _draft.matches(_actual) && !_baseline.matches(_actual);
    }
    bool apply() {
        if (!canApply()) return false;
        _baseline = _actual;
        return true;
    }
    bool needsRestore() const {
        return _active && (_inFlight || !_baseline.matches(_actual) || !_baseline.matches(_draft));
    }
    bool settled() const { return _initialized && !_inFlight && !_restoring && _baseline.matches(_actual) && _baseline.matches(_draft); }
    bool curveReady() const { return _active && !_restoring && (_writeCurve || _draft.preset == _actual.preset); }
    bool active() const { return _active; }
    bool busy() const { return _inFlight.has_value(); }
    bool restoring() const { return _restoring; }
    const EqualizerState& baseline() const { return _baseline; }
    const EqualizerState& draft() const { return _draft; }
    const EqualizerState& actual() const { return _actual; }
private:
    void settleRestore() {
        if (_restoring && !_inFlight && _baseline.matches(_actual)) {
            _draft = _actual;
            _restoring = false;
            if (_finishAfterRestore) _active = false;
            _finishAfterRestore = false;
        }
    }
    bool _initialized = false;
    bool _active = false, _restoring = false, _finishAfterRestore = false, _writeCurve = false;
    unsigned long _revision = 0;
    double _lastEdit = 0;
    EqualizerState _baseline, _actual, _draft;
    std::optional<Action> _inFlight;
};

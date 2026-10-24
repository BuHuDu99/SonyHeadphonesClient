#include "EqualizerPreview.h"
#include "AppConfig.h"
#include <cassert>
#include <filesystem>
#include <chrono>

int main() {
    EqualizerState original{0, 0, {0, 0, 0, 0, 0}, 0};
    EqualizerState bright{0x10, 2, {0, 1, 2, 4, 6}, 0};
    EqualizerState custom{0xa0, -2, {-3, 1, 2, 3, 4}, 0};
    assert(original.restorable() && custom.restorable());
    auto unknown = original; unknown.preset = 0xff;
    assert(!unknown.restorable());
    unknown.payloadType = 1;
    assert(unknown.restorable() && unknown.manual());
    EqualizerPreview session;
    assert(!session.settled()); // A journal must not be cleared before any state was loaded.
    session.begin(original);
    session.edit(bright, false, 1.0);
    assert(!session.next(1.1));
    auto action = session.next(1.2);
    assert(action && !action->writeCurve && action->target.preset == 0x10);
    assert(!session.canApply());
    session.confirmed(bright);
    assert(session.draft().bands == bright.bands && session.draft().bass == bright.bass);
    assert(session.canApply());
    session.restore();
    action = session.next(1.3);
    assert(action && action->restoring && action->target.preset == original.preset);
    session.confirmed(original);
    assert(session.active() && session.settled());

    session.edit(custom, true, 2.0);
    action = session.next(2.2);
    assert(action && action->writeCurve);
    session.confirmed(custom);
    assert(session.apply());
    session.restore(true);
    assert(!session.active() && session.settled()); // Apply becomes the new rollback baseline.
    assert(!session.next(2.4));
    assert(session.baseline().bands == custom.bands);

    // Leaving before the debounce elapses cancels the draft without a device write.
    session.begin(original);
    session.edit(bright, false, 3.0);
    session.restore(true);
    assert(!session.active() && !session.next(3.2));

    // Slider changes coalesce; readback of an older curve never overwrites newer edits.
    session.begin(original);
    session.edit(custom, true, 4.0);
    action = session.next(4.2);
    auto newer = custom; newer.bass = 4; newer.bands[0] = 8;
    session.edit(newer, true, 4.21);
    assert(!session.next(4.4)); // Only one write in flight.
    session.confirmed(custom);
    assert(session.draft().bass == 4 && session.draft().bands[0] == 8);
    assert(!session.canApply());
    action = session.next(4.5);
    assert(action && action->target.bass == 4);
    session.confirmed(newer);
    assert(session.canApply());

    // Revert during an in-flight preview must wait and then restore the old curve exactly.
    session.begin(custom);
    session.edit(bright, false, 5.0);
    action = session.next(5.2);
    session.restore(true);
    assert(!session.next(5.3));
    session.confirmed(bright);
    action = session.next(5.4);
    assert(action && action->restoring && action->writeCurve);
    assert(action->target.bands == custom.bands && action->target.bass == -2);
    session.confirmed(custom);
    assert(!session.active() && session.settled());

    // Selecting Custom reads its stored curve; it does not overwrite it with the old preset's curve.
    session.begin(bright);
    session.edit(custom, false, 6.0);
    action = session.next(6.2);
    assert(action && !action->writeCurve);
    session.confirmed(newer);
    assert(session.draft().bands == newer.bands && session.draft().bass == newer.bass);

    // Crash/disconnect recovery uses the original baseline and exits after confirmed rollback.
    session.recover(custom, bright);
    action = session.next(7.0);
    assert(action && action->restoring && action->writeCurve);
    session.confirmed(custom);
    assert(!session.active() && session.settled());
    session.recover(custom, custom);
    assert(!session.active() && session.settled() && !session.next(8.0));

    const auto path = std::filesystem::temp_directory_path() /
        ("sony-eq-recovery-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".toml");
    AppConfig config;
    assert(!config.load(path.string()));
    config.equalizerRecovery = AppConfig::EqualizerRecovery{"AA:BB:CC:DD:EE:FF", custom};
    assert(config.persist()); // A missing config is created before sending a preview.
    assert(!std::filesystem::exists(path.string() + ".tmp"));
    AppConfig restored;
    assert(restored.load(path.string()));
    assert(restored.equalizerRecovery && restored.equalizerRecovery->deviceMac == "AA:BB:CC:DD:EE:FF");
    assert(restored.equalizerRecovery->original.bass == custom.bass);
    assert(restored.equalizerRecovery->original.bands == custom.bands);
    restored.equalizerRecovery.reset();
    assert(restored.persist());
    assert(config.load(path.string()) && !config.equalizerRecovery);
    std::filesystem::remove(path);
}

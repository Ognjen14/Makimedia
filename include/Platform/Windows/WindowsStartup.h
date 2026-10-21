#pragma once

namespace WindowsStartup {

enum class State {
    Off,
    On,
    OffByUser,
    OffByPolicy,
    Unavailable
};

bool packaged();

State state();
State setEnabled(bool enabled);

bool launchedAtSignIn();

}

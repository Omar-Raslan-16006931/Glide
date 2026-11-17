// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Compiled with ARC (-fobjc-arc): the static strong reference keeps the activity for the program's lifetime.
#import <Foundation/Foundation.h>

#include "MacActivity.h"

namespace pushfps {

void keepTimingPrecise() {
    static id<NSObject> activity = [[NSProcessInfo processInfo]
        beginActivityWithOptions:(NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical)
                          reason:@"Live video from a capture card"];
    (void)activity;
}

}  // namespace pushfps

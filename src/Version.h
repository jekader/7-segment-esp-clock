#pragma once

// Local and pull-request builds are intentionally distinguishable from releases.
// CI overrides VERSION through PLATFORMIO_BUILD_FLAGS for version tags.
#ifndef VERSION
#define VERSION "v0.0.0"
#endif

#pragma once

// Developer-only build flag. Shipping builds leave this at 0; `make debug`
// passes -DSH_DEV_MODE=1 and uses isolated output directories. The debug image
// adds map inspection controls without changing the release game.
#ifndef SH_DEV_MODE
#define SH_DEV_MODE 0
#endif

#if SH_DEV_MODE != 0 && SH_DEV_MODE != 1
#error "SH_DEV_MODE must be 0 or 1"
#endif

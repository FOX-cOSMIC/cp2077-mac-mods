#pragma once
//
// Internal definition of the opaque TweakSourceHandle. This header pulls in
// psiberx's Source.hpp and therefore REQUIRES the TWEAKXL_MAC_OFFLINE build
// config (Core stubs). Only translation units that are part of the tweakxl
// target and built with that flag may include it — e.g. the P1.10 applicator.
// Public consumers must use the opaque handle in parsers/Tweak.hpp instead.
//
#include "Red/TweakDB/Source/Source.hpp"

#include <memory>

namespace tweakxl_mac {
    struct TweakSourceHandle {
        std::shared_ptr<Red::TweakSource> source;
    };
}

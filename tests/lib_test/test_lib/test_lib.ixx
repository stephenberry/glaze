// Glaze Library
// For the license information refer to glaze.ixx

export module glaze.tests.test_lib;

import glaze.tests.interface;
import glaze.api.impl;
import glaze.api.std.span;

export glz::iface_fn glz_iface() noexcept { return glz::make_iface<my_api>(); }

#
#
include(MacroLibraryTargets)

set(LIBTOAST_COMPONENTS
    sources.cmake
    atm/sources.cmake
    fod/sources.cmake
    map/sources.cmake
    math/sources.cmake
    tod/sources.cmake
    capi/sources.cmake
    math/Random123/sources.cmake
    math/Random123/conventional/sources.cmake
    math/Random123/features/sources.cmake
    test/sources.cmake
    test/gtest/sources.cmake
    util/sources.cmake
)

GLOBAL_OBJECT_TARGET(NAME libtoast-object
    COMPONENTS ${LIBTOAST_COMPONENTS})

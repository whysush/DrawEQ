# All third-party code is fetched, pinned, and shallow-cloned.
# Adding anything to this file needs a conversation first (CONTEXT.md 13.1).

include (FetchContent)

set (FETCHCONTENT_QUIET OFF)

# ---------------------------------------------------------------- JUCE 8
# Pinned to a release tag, never `develop`: `develop` moves under us and
# silently changes DSP and font metrics between builds.
FetchContent_Declare (juce
    GIT_REPOSITORY  https://github.com/juce-framework/JUCE.git
    GIT_TAG         8.0.9
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE)

# ---------------------------------------------------------------- Eigen 3.4
# Header-only, used only by the fitter on the worker thread. Fetched as a
# tarball rather than a clone: the Eigen repository history is enormous.
FetchContent_Declare (eigen
    URL             https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    URL_HASH        SHA256=8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72)

# ---------------------------------------------------------------- Catch2 v3
FetchContent_Declare (Catch2
    GIT_REPOSITORY  https://github.com/catchorg/Catch2.git
    GIT_TAG         v3.5.4
    GIT_SHALLOW     TRUE)

FetchContent_MakeAvailable (juce Catch2)

# Eigen ships a CMake project that wants to install itself; we only want the
# headers, so populate without adding the subdirectory.
FetchContent_GetProperties (eigen)
if (NOT eigen_POPULATED)
    FetchContent_Populate (eigen)
endif ()

add_library (draweq_eigen INTERFACE)
# SYSTEM, so that our own -Werror-grade warning set does not fire on headers
# we do not control.
target_include_directories (draweq_eigen SYSTEM INTERFACE ${eigen_SOURCE_DIR})


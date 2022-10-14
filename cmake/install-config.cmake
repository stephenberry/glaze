include(CMakeFindDependencyMacro)
find_dependency(fmt 9)
find_dependency(nanorange)
find_dependency(frozen 1.1.1)
find_dependency(FastFloat 3.4)

include("${CMAKE_CURRENT_LIST_DIR}/glazeTargets.cmake")

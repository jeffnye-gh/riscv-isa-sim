#=========================================================================
# Toplevel configure.ac for the Modular C++ Build System
#=========================================================================

#-------------------------------------------------------------------------
# Project metadata
#-------------------------------------------------------------------------

m4_define( proj_name,         [RISC-V ISA Simulator])
m4_define( proj_maintainer,   [Andrew Waterman])
m4_define( proj_abbreviation, [spike])

#-------------------------------------------------------------------------
# Project version information
#-------------------------------------------------------------------------

m4_define( proj_version, [?])

#-------------------------------------------------------------------------
# Setup
#-------------------------------------------------------------------------

AC_INIT(proj_name,proj_version,proj_maintainer,proj_abbreviation)
AC_LANG_CPLUSPLUS
AC_CONFIG_SRCDIR([riscv/common.h])
AC_CONFIG_AUX_DIR([scripts])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

m4_include(ax_require_defined.m4)
m4_include(ax_append_flag.m4)
m4_include(ax_check_compile_flag.m4)
m4_include(ax_check_link_flag.m4)
m4_include(ax_append_link_flags.m4)
m4_include(ax_boost_base.m4)
m4_include(ax_boost_asio.m4)
m4_include(ax_boost_regex.m4)

#-------------------------------------------------------------------------
# Checks for programs
#-------------------------------------------------------------------------

AC_PROG_CC
AC_PROG_CXX
AC_CHECK_TOOL([AR],[ar])
AC_CHECK_TOOL([RANLIB],[ranlib])
AC_PATH_PROG([DTC],[dtc],[no])
AS_IF([test x"$DTC" == xno],AC_MSG_ERROR([device-tree-compiler not found]))
AC_DEFINE_UNQUOTED(DTC, ["dtc"], [Executable name of device-tree-compiler])

AC_C_BIGENDIAN

#-------------------------------------------------------------------------
# Check for the stf_lib submodule and extract Git SHA
#-------------------------------------------------------------------------

AC_MSG_CHECKING([for stf_lib submodule])

if test -d "${srcdir}/stf_lib" && test -f "${srcdir}/stf_lib/.git"; then
    AC_MSG_RESULT([found])
else
    AC_MSG_NOTICE([stf_lib submodule not found or incomplete. Initializing and updating...])
    if ! (cd "${srcdir}" && git submodule update --init --recursive); then
        AC_MSG_ERROR([Failed to initialize and update stf_lib submodule. Ensure git is available and the submodule is correctly configured.])
    fi
fi

# Extract Git SHA for STF_SPIKE and stf_lib
STF_SPIKE_GIT_SHA=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
AC_DEFINE_UNQUOTED([STF_SPIKE_GIT_SHA], ["$STF_SPIKE_GIT_SHA"], [Git SHA of STF_SPIKE repository])

if test -d "${srcdir}/stf_lib"; then
    STF_LIB_GIT_SHA=$(cd "${srcdir}/stf_lib" && git rev-parse HEAD 2>/dev/null || echo "unknown")
else
    STF_LIB_GIT_SHA="unknown"
fi
AC_DEFINE_UNQUOTED([STF_LIB_GIT_SHA], ["$STF_LIB_GIT_SHA"], [Git SHA of stf_lib submodule])

#-------------------------------------------------------------------------
# Configure and build stf_lib
#-------------------------------------------------------------------------

AC_MSG_NOTICE([Configuring and building stf_lib submodule])
STF_LIB_BUILD_DIR="${srcdir}/stf_lib/release"
if test ! -d "$STF_LIB_BUILD_DIR"; then
    mkdir -p "$STF_LIB_BUILD_DIR" || AC_MSG_ERROR([Failed to create build directory for stf_lib])
fi

(cd "$STF_LIB_BUILD_DIR" && cmake .. -DCMAKE_BUILD_TYPE=FastDebug -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF && make -j$(nproc)) || \
    AC_MSG_ERROR([Failed to build stf_lib submodule])

#-------------------------------------------------------------------------
# Add stf_lib to linker flags
#-------------------------------------------------------------------------

STF_LIB_PATH="${srcdir}/stf_lib/release/lib"
if test -f "${STF_LIB_PATH}/libstf.a"; then
    AC_MSG_NOTICE([Found libstf.a at ${STF_LIB_PATH}])
    LDFLAGS="-L${STF_LIB_PATH} $LDFLAGS"
    LIBS="-lstf $LIBS"
else
    AC_MSG_ERROR([libstf.a not found in ${STF_LIB_PATH}. Ensure stf_lib is built correctly.])
fi

#-------------------------------------------------------------------------
# Version Information
#-------------------------------------------------------------------------

STF_SPIKE_VERSION_MAJOR=1
STF_SPIKE_VERSION_MINOR=0
STF_SPIKE_VERSION_PATCH=0

AC_DEFINE_UNQUOTED([STF_SPIKE_VERSION_MAJOR], [$STF_SPIKE_VERSION_MAJOR], [STF Spike Version Major])
AC_DEFINE_UNQUOTED([STF_SPIKE_VERSION_MINOR], [$STF_SPIKE_VERSION_MINOR], [STF Spike Version Minor])
AC_DEFINE_UNQUOTED([STF_SPIKE_VERSION_PATCH], [$STF_SPIKE_VERSION_PATCH], [STF Spike Version Patch])

#-------------------------------------------------------------------------
# MCPPBS specific program checks
#-------------------------------------------------------------------------
MCPPBS_PROG_INSTALL

#-------------------------------------------------------------------------
# Checks for header files
#-------------------------------------------------------------------------

AC_HEADER_STDC

#-------------------------------------------------------------------------
# Checks for type
#-------------------------------------------------------------------------

AC_CHECK_TYPE([__int128_t],
              [AC_DEFINE([HAVE_INT128], [1], [__int128_t is supported])])

#-------------------------------------------------------------------------
# Default compiler flags
#-------------------------------------------------------------------------

AX_APPEND_LINK_FLAGS([-Wl,--export-dynamic])

AX_CHECK_COMPILE_FLAG([-relocatable-pch], AC_DEFINE([HAVE_CLANG_PCH], [1], [Clang supports PCH]))

#-------------------------------------------------------------------------
# MCPPBS subproject list
#-------------------------------------------------------------------------
MCPPBS_SUBPROJECTS([fesvr, riscv, disasm, customext, fdt, softfloat, spike_main, spike_dasm])

#-------------------------------------------------------------------------
# Output
#-------------------------------------------------------------------------

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([riscv-fesvr.pc])
AC_CONFIG_FILES([riscv-disasm.pc])
AC_CONFIG_FILES([riscv-riscv.pc])
AC_OUTPUT


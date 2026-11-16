#!/usr/bin/env bash
# Builds the KF6 frameworks required by Freesm Launcher from source and
# installs them into a prefix ($1). All frameworks are pinned to the same
# release tag.
set -euo pipefail

PREFIX="$1"
KF6_VERSION="${2:-6.32.0}"

WORK="${RUNNER_TEMP:-/tmp}/kf6-source-build"
SRC="$WORK/src"
BUILD="$WORK/build"
rm -rf "$WORK"
mkdir -p "$PREFIX" "$SRC" "$BUILD"

RUNNER_OS="${RUNNER_OS:-Linux}"
RUNNER_ARCH="${RUNNER_ARCH:-X64}"

# Locate the Qt6 prefix from install-qt-action (Qt6_DIR is exported as a
# step environment variable, e.g. .../lib/cmake/Qt6).
if [ -n "${Qt6_DIR:-}" ]; then
  QT_PREFIX="$(cd "$Qt6_DIR/../../.." && pwd)"
elif [ -n "${QT_ROOT_DIR:-}" ]; then
  QT_PREFIX="$QT_ROOT_DIR"
else
  echo "::error::Neither Qt6_DIR nor QT_ROOT_DIR is set; the Qt setup step must run before this one" >&2
  exit 1
fi
echo "Qt6 prefix: $QT_PREFIX"

# Extra prefixes the KF6 build should see (e.g. gettext/libintl from vcpkg).
case "$RUNNER_OS-$RUNNER_ARCH" in
  Windows-X64)   VCPKG_TRIPLET="x64-windows" ;;
  Windows-ARM64) VCPKG_TRIPLET="arm64-windows" ;;
  Darwin-*)      VCPKG_TRIPLET="universal-osx" ;;
  *)             VCPKG_TRIPLET="" ;;
esac
EXTRA_PREFIX=""
if [ "$RUNNER_OS" = "Darwin" ]; then
  # vcpkg's universal-osx gettext does not build (autotools --host
  # universal-apple-darwin is rejected by config.sub). Build a fat
  # (arm64 + x86_64) gettext from source instead.
  GETTEXT_PREFIX="$WORK/gettext-prefix"
  GETTEXT_VERSION=0.22.5
  echo "::group::Build gettext (universal) $GETTEXT_VERSION"
  curl -fsSL "https://ftpmirror.gnu.org/gnu/gettext/gettext-$GETTEXT_VERSION.tar.gz" | tar xz -C "$WORK"
  (
    cd "$WORK/gettext-$GETTEXT_VERSION"
    ./configure --disable-shared --prefix="$GETTEXT_PREFIX" \
      CC="clang -arch arm64 -arch x86_64" CXX="clang++ -arch arm64 -arch x86_64" \
      CFLAGS="-arch arm64 -arch x86_64 -O2" CXXFLAGS="-arch arm64 -arch x86_64 -O2"
    make -j"$(sysctl -n hw.ncpu)"
    make install
  )
  echo "::endgroup::"
  EXTRA_PREFIX="$GETTEXT_PREFIX"
elif [ -n "$VCPKG_TRIPLET" ] && [ -d "$GITHUB_WORKSPACE/vcpkg_installed/$VCPKG_TRIPLET" ]; then
  EXTRA_PREFIX="$GITHUB_WORKSPACE/vcpkg_installed/$VCPKG_TRIPLET"
fi
if [ -n "$EXTRA_PREFIX" ]; then
  echo "Extra prefix: $EXTRA_PREFIX"
fi

# Generator: Ninja everywhere except Windows, where Visual Studio is always
# present in the runner images.
GEN_ARGS=(-G Ninja)
CONFIG_BUILD_TYPE=""
if [ "$RUNNER_OS" = "Windows" ]; then
  if [ "$RUNNER_ARCH" = "ARM64" ]; then
    # windows-11-arm runners ship VS 18 (18.x), which the CMake bundled
    # on the image does not know. The VS developer shell is already in
    # the environment, so use Ninja instead of a VS generator.
    GEN_ARGS=(-G Ninja)
    CONFIG_BUILD_TYPE=Release
  else
    GEN_ARGS=(-G "Visual Studio 17 2022" -A x64)
    CONFIG_BUILD_TYPE=""
  fi
fi

MSGFMT=""
MSGMERGE=""
if [ "$RUNNER_OS" = "Windows" ] && [ -n "$EXTRA_PREFIX" ] && [ -d "$EXTRA_PREFIX/tools/gettext/bin" ]; then
  # vcpkg installs the gettext tools outside the normal bin/ layout
  [ -x "$EXTRA_PREFIX/tools/gettext/bin/msgfmt.exe" ]   && MSGFMT="$EXTRA_PREFIX/tools/gettext/bin/msgfmt.exe"
  [ -x "$EXTRA_PREFIX/tools/gettext/bin/msgmerge.exe" ] && MSGMERGE="$EXTRA_PREFIX/tools/gettext/bin/msgmerge.exe"
fi
if [ "$RUNNER_OS" = "Darwin" ] && [ -n "$EXTRA_PREFIX" ]; then
  MSGFMT="$EXTRA_PREFIX/bin/msgfmt"
  MSGMERGE="$EXTRA_PREFIX/bin/msgmerge"
fi

CONFIG_ARGS=(-DCMAKE_INSTALL_PREFIX="$PREFIX" -DBUILD_TESTING=OFF -DWITH_WAYLAND=OFF -DWITH_X11=OFF -DBUILD_PYTHON_BINDINGS=OFF ${CONFIG_BUILD_TYPE:+-DCMAKE_BUILD_TYPE=Release})
[ -n "$MSGFMT" ]   && CONFIG_ARGS+=(-DGETTEXT_MSGFMT_EXECUTABLE="$MSGFMT")
[ -n "$MSGMERGE" ] && CONFIG_ARGS+=(-DGETTEXT_MSGMERGE_EXECUTABLE="$MSGMERGE")
if [ "$RUNNER_OS" = "Darwin" ]; then
  CONFIG_ARGS+=(-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-12.0}")
fi
if [ "$RUNNER_OS" != "Windows" ] && [ -z "${CONFIG_BUILD_TYPE}" ]; then
  CONFIG_ARGS+=(-DCMAKE_BUILD_TYPE=Release)
fi
# KAuth integration is not needed (it is for editing system files) and would
# pull in extra backends (polkit on Linux).
CONFIG_ARGS+=(-DENABLE_KAUTH=OFF)

# Tools that must exist on every platform. On Windows, install them first.
FLEX_EXECUTABLE=""
BISON_EXECUTABLE=""
if [ "$RUNNER_OS" = "Windows" ]; then
  if ! command -v perl >/dev/null 2>&1; then
    choco install strawberryperl -y --no-progress
  fi
  if ! command -v bison >/dev/null 2>&1 && ! command -v win_bison >/dev/null 2>&1; then
    # winflexbison3 ships win_flex_bison 2.5.x (Flex 2.5.x / Bison 3.x);
    # the legacy "winflexbison" package ships Flex/Bison 2.4.9, which is too old
    choco install winflexbison3 -y --no-progress || choco install winflexbison -y --no-progress
  fi
  FLEX_EXECUTABLE="$(command -v flex || command -v win_flex || true)"
  BISON_EXECUTABLE="$(command -v bison || command -v win_bison || true)"
  echo "flex: $FLEX_EXECUTABLE, bison: $BISON_EXECUTABLE"
  "$BISON_EXECUTABLE" --version | head -1 || true
  [ -n "$FLEX_EXECUTABLE" ] && CONFIG_ARGS+=(-DFLEX_EXECUTABLE="$FLEX_EXECUTABLE")
  [ -n "$BISON_EXECUTABLE" ] && CONFIG_ARGS+=(-DBISON_EXECUTABLE="$BISON_EXECUTABLE")
elif [ "$RUNNER_OS" = "Darwin" ]; then
  CONFIG_ARGS+=(-DFLEX_EXECUTABLE="$(brew --prefix flex)/bin/flex")
  CONFIG_ARGS+=(-DBISON_EXECUTABLE="$(brew --prefix bison)/bin/bison")
fi

clone_repo() { # <repo-name> <version> [dest-name]
  local name="$1" version="$2" dest="${3:-$1}"
  echo "::group::Clone $name"
  git clone -q --depth 1 --branch "v$version" "https://github.com/KDE/$name.git" "$SRC/$dest"
  echo "::endgroup::"
}

build_one() { # <dir-name> [extra-cmake-args...]
  local name="$1"
  shift
  echo "::group::Build $name"
  cmake -S "$SRC/$name" -B "$BUILD/$name" "${GEN_ARGS[@]}" "${CONFIG_ARGS[@]}" \
    -DCMAKE_PREFIX_PATH="$PREFIX;$QT_PREFIX${EXTRA_PREFIX:+;$EXTRA_PREFIX}" "$@"
  if [ "$RUNNER_OS" = "Windows" ]; then
    cmake --build "$BUILD/$name" --config Release --parallel
    cmake --install "$BUILD/$name" --config Release
  else
    cmake --build "$BUILD/$name" --parallel
    cmake --install "$BUILD/$name"
  fi
  echo "::endgroup::"
}

clone_repo extra-cmake-modules "$KF6_VERSION"
build_one extra-cmake-modules

# Windows: SnoreToast is a hard requirement for KF6Notifications when
# libcanberra is not available (it never is on Windows).
if [ "$RUNNER_OS" = "Windows" ]; then
  clone_repo snoretoast 0.9.1
  build_one snoretoast
fi

# Frameworks in dependency order.
ORDER=(
  karchive
  kcodecs
  kcoreaddons
  kcrash
  kguiaddons
  ki18n
  kconfig
  kwidgetsaddons
  kcolorscheme
  kcompletion
  kitemviews
  kservice
  kbookmarks
  kglobalaccel
  kiconthemes          # USE_BreezeIcons=OFF below
  kconfigwidgets
  syntax-highlighting
  sonnet               # SONNET_NO_BACKENDS=ON below
  knotifications        # needs Canberra (Linux) or SnoreToast (Windows)
  kjobwidgets
  kwindowsystem
  solid                # needs flex + bison
  ktextwidgets
  kxmlgui
  kio
  kparts
  ktexteditor
)

for name in "${ORDER[@]}"; do
  clone_repo "$name" "$KF6_VERSION"
done

EXTRA_ARGS=()
EXTRA_ARGS+=(-DUSE_BreezeIcons=OFF)
EXTRA_ARGS+=(-DSONNET_NO_BACKENDS=ON)

for name in "${ORDER[@]}"; do
  case "$name" in
    kiconthemes|sonnet) build_one "$name" "${EXTRA_ARGS[@]}" ;;
    karchive) build_one "$name" -DWITH_BZIP2=OFF -DWITH_LIBLZMA=OFF -DWITH_OPENSSL=OFF -DWITH_LIBZSTD=OFF ;;
    *) build_one "$name" ;;
  esac
done

echo "KF6 $KF6_VERSION installed to $PREFIX"

# uLua development tasks.
#
# Run `just` with no arguments to see the recipe inventory.
# Most recipes accept extra args, forwarded to the underlying tool.

set shell := ['bash', '-eu', '-o', 'pipefail', '-c']
set dotenv-load := false

build_dir   := 'build'
build_type  := 'Release'
install_dir := '/tmp/ulua-install'

# Source roots that clang-format walks.
fmt_paths := 'include tests examples'

# Default: print the recipe list (resilient to -f / non-standard paths).
[private]
[doc('Show available recipes')]
default:
    @just --justfile {{ justfile() }} --list --unsorted

# ---- build ------------------------------------------------------------------

[group('build')]
[doc('Configure CMake into {{build_dir}} (idempotent)')]
configure *FLAGS:
    cmake -S . -B {{ build_dir }} \
      -DCMAKE_BUILD_TYPE={{ build_type }} \
      -DULUA_BUILD_TESTS=ON \
      -DULUA_BUILD_EXAMPLES=ON \
      {{ FLAGS }}

[group('build')]
[doc('Reconfigure from scratch (drops CMake cache)')]
reconfigure *FLAGS: clean configure

[group('build')]
[doc('Build all targets')]
build *FLAGS: configure
    cmake --build {{ build_dir }} -j {{ FLAGS }}

# ---- test -------------------------------------------------------------------

[group('test')]
[doc('Run the doctest suite via ctest')]
test *FLAGS: build
    ctest --test-dir {{ build_dir }} --output-on-failure {{ FLAGS }}

[group('test')]
[doc('Run only one doctest test case by name')]
test-case NAME: build
    {{ build_dir }}/tests/test_ulua --test-case='{{ NAME }}'

[group('test')]
[doc('List every doctest test case')]
test-list: build
    {{ build_dir }}/tests/test_ulua --list-test-cases

# ---- examples ---------------------------------------------------------------

[group('examples')]
[doc('Run the hello example')]
hello: build
    {{ build_dir }}/examples/ulua-hello

[group('examples')]
[doc('Run the userdata example')]
userdata-example: build
    {{ build_dir }}/examples/ulua-userdata

[group('examples')]
[doc('Run every example in turn')]
examples: hello userdata-example

# ---- install ----------------------------------------------------------------

[group('install')]
[doc('Install headers and CMake config into {{install_dir}}')]
install: build
    cmake --install {{ build_dir }} --prefix {{ install_dir }}

[group('install')]
[doc('Install and print the resulting layout')]
install-verify: install
    @echo '--- {{ install_dir }} ---'
    @find {{ install_dir }} -type f | sort

# ---- quality ----------------------------------------------------------------

[group('quality')]
[doc('Format C++ sources in place')]
fmt:
    find {{ fmt_paths }} -type f \( -name '*.hpp' -o -name '*.cpp' \) \
        -print0 | xargs -0 clang-format -i

[group('quality')]
[doc('Check formatting without modifying files')]
fmt-check:
    find {{ fmt_paths }} -type f \( -name '*.hpp' -o -name '*.cpp' \) \
        -print0 | xargs -0 clang-format --dry-run --Werror

# ---- pipelines --------------------------------------------------------------

[group('pipelines')]
[doc('Configure, build, and test (the contributor smoke test)')]
verify: test

[group('pipelines')]
[doc('Format check + build + test (mirrors CI)')]
ci: fmt-check test
    @echo 'CI passed'

# ---- housekeeping -----------------------------------------------------------

[group('housekeeping')]
[doc('Delete the build directory')]
clean:
    rm -rf {{ build_dir }}

[group('housekeeping')]
[doc('Delete the build directory AND any installed prefix')]
distclean: clean
    rm -rf {{ install_dir }}

# ---- aliases ----------------------------------------------------------------

alias b := build
alias c := configure
alias t := test
alias r := reconfigure

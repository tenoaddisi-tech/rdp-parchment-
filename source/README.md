# Intel RDRAND-seeded PRNG

A small C++17 library for Intel/AMD x86 and x86-64 targets. It obtains seed
material through `RDRAND`, then uses xoshiro256** for fast pseudorandom output.

## Important terminology

`RDRAND` is backed by Intel's on-chip digital random number generator, but its
output is conditioned by a hardware deterministic random bit generator. Intel
defines `RDSEED` as the instruction intended to provide seed material. This
module follows the requested `RDRAND` design; use `RDSEED` instead if your
security specification explicitly requires seed-grade entropy or direct access
to a hardware entropy source.

xoshiro256** is not a cryptographic PRNG. Do not use `Prng` for keys, nonces,
passwords, tokens, or other security-sensitive output. For such values, consume
an approved cryptographic RNG directly.

## Behavior

- Checks CPUID before executing `RDRAND`.
- Retries transient `RDRAND` failures (10 attempts by default).
- Throws `intel_rng::entropy_error` if hardware is unavailable or the retry
  limit is exhausted. It never silently substitutes a predictable seed.
- Collects four 64-bit hardware values and diffuses them into 256 bits of PRNG
  state.
- Provides unbiased bounded integers using rejection sampling.
- Supports 32-bit x86 by composing two successful 32-bit `RDRAND` operations.
- Does not allocate dynamically.

Each `Prng` object is independent but is not internally synchronized. Give each
thread its own instance or protect a shared instance with a lock.

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The implementation supports MSVC, GCC, and Clang on x86/x86-64. The deterministic
tests do not require `RDRAND`; the hardware smoke test runs only when CPUID says
the instruction is available.

## Local dashboard app

The project includes a dependency-free native web application. The C++ backend
executes `RDRAND` and the PRNG on the host CPU; the browser is only the user
interface. The server listens on loopback (`127.0.0.1`) and is not exposed to
other machines.

After building, change to the directory containing the executable and run:

```sh
./rdrand_dashboard
```

On Windows with a multi-configuration CMake generator, this is normally:

```powershell
cd build\Debug
.\rdrand_dashboard.exe
```

Then open <http://127.0.0.1:8787>. Pass a different port as the first argument,
for example `rdrand_dashboard 9000`.

The dashboard exposes three loopback-only endpoints:

- `GET /api/status`
- `GET /api/random?count=16&bound=100` (`bound=0` selects full-width output)
- `POST /api/reseed`

## Public download site and browser extension

`site/` is a static landing page suitable for HTTPS static hosting. It provides
download links for the Windows portable app and the Chrome/Chromium extension.
The site itself never generates random values; the extension communicates with
the user's local app at `127.0.0.1:8787`. The extension does not access an
external API or website.

On Windows with CMake and a C++17 compiler installed, create the release ZIPs
linked by the site:

```powershell
.\tools\package-extension.ps1
.\tools\package-windows-release.ps1
```

To package the source and browser extension downloads:

```powershell
.\tools\package-source.ps1
.\tools\package-extension.ps1
```

For a prebuilt Windows app ZIP, run
`.\tools\package-windows-release.ps1` on Windows with CMake and a C++17
compiler. A prebuilt executable is not included in this source release.

The `site/` folder can be deployed to an HTTPS static host. For a one-off
public deployment without a Git host, open <https://app.netlify.com/drop> and
upload the contents of `site/`. For repeatable GitHub Pages deployments, push
to `main`; `.github/workflows/deploy-pages.yml` builds the Windows bundle and
deploys the site directory.

To install the extension manually, extract
`rdrand-lab-extension.zip`, open `chrome://extensions`, enable Developer mode,
and select **Load unpacked**. The local dashboard app must be running for the
extension to connect. The Windows package is a portable ZIP, not a signed
installer.

## Use

```cpp
#include "rdrand_prng.hpp"

intel_rng::RdrandEntropy entropy;
intel_rng::Prng rng(entropy);

const std::uint64_t any_value = rng.next_u64();
const std::uint64_t die_roll = rng.bounded(6) + 1;
const double fraction = rng.uniform_double();
```

Catch `intel_rng::entropy_error` during initialization/reseeding and apply the
failure policy required by the device. `examples/basic.cpp` contains a complete
example.

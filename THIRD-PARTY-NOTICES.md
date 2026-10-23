# Third-party software in the Mead Workshop downloads

Mead Workshop's own code is under the MIT license (see `LICENSE`). The ready-made downloads (the Windows
installer, the portable zip and the Linux AppImage) also contain open-source libraries the app runs on.
Each one keeps its own license; their license files are in the `licenses` folder next to `MeadWorkshop.exe`
(Windows) or in `usr/share/licenses` inside the AppImage.

| Library | What it does in the app | License |
| --- | --- | --- |
| Qt 6 (Core, Gui, Qml, Quick, Quick Controls, Network and their plugins) | windows, screens, text, images | GNU LGPL v3 |
| GCC runtime (libgcc, libstdc++), winpthreads | the C++ runtime (Windows) | GPL v3 with the GCC Runtime Library Exception; MIT/BSD |
| ICU | text and Unicode handling | Unicode / ICU license |
| FreeType, HarfBuzz, Graphite2 | drawing text | FreeType License; MIT; LGPL 2.1 or MPL 2.0 |
| GLib, libiconv, libintl (gettext), libffi | used by Qt on Windows | LGPL 2.1+; LGPL 2.1+; LGPL 2.1+; MIT |
| PCRE2, zlib, zstd, bzip2, Brotli, libb2, double-conversion, md4c | text search, compression, numbers, Markdown | BSD / zlib / MIT-style / CC0 |
| libpng, libjpeg-turbo | images | libpng license; IJG / BSD |

**Qt and the other LGPL libraries** are used unmodified and loaded as separate files (DLLs on Windows,
`.so` files in the AppImage), so you may replace them with your own builds of the same versions. Their
source code is available from:

- Qt: https://download.qt.io/official_releases/qt/ (and https://code.qt.io)
- The Windows builds of every library above (MSYS2 packages, with their build recipes):
  https://github.com/msys2/MINGW-packages and https://repo.msys2.org/mingw/sources/
- Mead Workshop's own source: this repository.

The exact package versions in a download are listed in `licenses/PACKAGES.txt` (Windows).

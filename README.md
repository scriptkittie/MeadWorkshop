# Mead Workshop

**A desktop mead planner for Windows and Linux that explains itself.**

Tell it what you want (how strong, how sweet, which fruit) and what you have (your fermenter, your honey,
20 lb of pears from the neighbour's tree). It works out a recipe, explains every step in plain words, and
gives you a shopping list and a dated brewing schedule from brew day to bottling.

Every number and piece of advice says where it comes from: a chapter in one of three well-known mead and
fermentation books, or a maker's published spec where the books give no figure.

- Piatz, *The Complete Guide to Making Mead*
- Schramm, *The Compleat Meadmaker*
- Katz, *The Art of Fermentation*

This repository contains no text or recipes from those books, only facts, methods and short credited
quotations. See **[SOURCES.md](SOURCES.md)** for exactly what is used and where it comes from.

## Who it's for

- **First-time meadmakers** who want a recipe that works and to understand *why*: every term is explained
  in everyday words, and the app tells you what to do on which day.
- **Brewers with ingredients on hand** (a crate of fruit, a bucket of honey, a 6-gallon fermenter) who want
  the recipe fitted to what they have, not the other way round.
- **Experienced brewers** who want quick, sourced numbers: gravity, alcohol, fruit water, nutrients,
  yeast limits, backsweetening and spice amounts.

## Download and install

Get the latest version from the **[Releases](../../releases)** page.

**Windows 10 / 11**
- `MeadWorkshop-<version>-Setup.exe`: double-click to install. No admin rights needed; it adds Start-menu
  and desktop shortcuts and an uninstaller (Settings → Apps).
- Or `MeadWorkshop-<version>-windows-portable.zip`: unzip anywhere and run `MeadWorkshop.exe`.
- The installer isn't signed with a paid certificate, so Windows may say "Windows protected your PC" the
  first time: click **More info**, then **Run anyway**.

**Linux** (most distributions)
- `MeadWorkshop-<version>-x86_64.AppImage`: make it executable (right-click → Properties → Permissions, or
  `chmod +x MeadWorkshop-*.AppImage`) and run it.

**Updating:** download the newer file and install it over the old one. Your saved recipes and settings stay.

## Getting started: your first mead

1. **Plan**: fill in *What you want*: a name, the batch size in gallons, the alcohol you'd like (e.g. 12%),
   the honey, how sweet, and the yeast (or *Choose for me*). Enter your fermenter's size.
2. Under *What you have*, add any fruit (**+ Fruit**) with how many pounds you have, and any spices
   (**+ Spice**) with how strong you want them. No fruit makes a traditional mead.
3. Press **Plan it**. You get the recipe fitted to your fermenter: how much honey, how much water, the
   starting and finishing gravity, the alcohol, what you'll likely bottle, and the reasons, step by step.
   If something can't work (too much fruit for the fermenter, a yeast that can't reach your strength), it
   says so and suggests a fix.
4. Press **Open in Designer** to see it in detail: live numbers, sweetness and strength bars, review notes,
   nutrients and the math. Change anything and every number updates.
5. Set **Brew date** to the day you'll brew, then **Save** (Ctrl+S). Your recipe is now in *My recipes*.
6. Open **Directions**: a shopping list, what to do before brew day, numbered brew-day steps, and a dated
   timeline after it (stirring, nutrient doses, racking, adding spices, checking it's finished).

Not sure what a word means? Click any underlined word, or open the **Words** tab.

## What's in the app

| Screen | What it does |
| --- | --- |
| **Plan** | Turns a goal and what you have into a recipe that fits your fermenter, with the reasoning, yeast options rated for your goal, what you'll likely bottle, and how to make more. |
| **Designer** | The recipe in detail: honey, fruit, juice, sugar, spices and yeast; live gravity, alcohol and volume; sweetness / strength / fruit bars; review notes; nutrients; style; and the formulas behind each number. |
| **My recipes** | **Save**, **Save as…** and **Recipes…** to open a saved recipe or start from one of the books' recipes. |
| **Directions** | Shopping list, before brew day, brew day, and a dated timeline to bottling. |
| **Quick check** | A traditional mead in three numbers: honey, batch size and yeast. |
| **Words** | Every brewing term (OG, FG, ABV, must, racking, backsweetening…) in everyday words. |
| **⚙ Settings** | Dark or light; Piatz's or Schramm's sweetness scale; the honey gravity figure; your recipes folder. |

## Where your things are kept

- **Saved recipes:** `Documents/Mead Workshop/My recipes`, one readable `.json` file per recipe, easy to back
  up or share. **⚙ Settings → Open My recipes folder** opens it.
- **The recipe on screen and your settings:** the app's data folder (`%APPDATA%\MeadWorkshop` on Windows).
  The recipe on screen is saved as you work, so nothing is lost if the app closes.
- Updating or uninstalling the app leaves both alone.

### Book pack (optional, if you own the books)

The books' recipes are listed by name and chapter so you can find them in your copy. If you own the
books, you can add the full recipes for your own use: put a `recipes.json` in a folder called `book-pack`,
next to the app or in its data folder (`%APPDATA%\MeadWorkshop\book-pack` on Windows). Keep it to
yourself: don't commit or share it.

## Good to know

- **US units.** Gallons, pounds and °F, with metric shown alongside where it helps. Full metric is planned.
- **Starting points, not guarantees.** Honey, fruit and yeast vary. Use a hydrometer, taste as you go,
  and adjust. Where the books give no amount (most spices, citrus), the app says so and gives a cautious
  starting point to taste up from.
- **Brew responsibly.** Making mead at home is legal for personal use in many places but not everywhere;
  check your local rules, and sanitize everything.

## Build from source

Needs a C++17 compiler, CMake 3.21+, Ninja and Qt 6.5+ (Base and Declarative).

**Windows** ([MSYS2](https://www.msys2.org), in the *UCRT64* shell):

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-{qt6-base,qt6-declarative,gcc,cmake,ninja}
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/MeadWorkshop.exe
```

**Linux** (Arch / CachyOS; other distributions have the same packages under similar names):

```bash
sudo pacman -S --needed qt6-base qt6-declarative cmake ninja gcc
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/MeadWorkshop
```

**Tests** (run from the repository folder, since they read `data/`):

```bash
./build/engine_tests && ./build/advisor_tests && ./build/consistency_tests
```

**Packages:** `bash packaging/windows/package.sh` (MSYS2 UCRT64; also needs `qt6-tools`, `nsis` and `zip`)
makes the Windows installer and zip in `dist/`; `bash packaging/linux/make-appimage.sh` makes the AppImage.
GitHub Actions runs the tests on every push and, for a version tag (`v0.1.0`), builds all three and attaches
them to a release.

## How it's built

All brewing logic and every sentence on screen live in plain C++17 with no Qt (`src/engine`), so the tests
can check them. The Qt layer (`src/app`, `qml/`) only displays what the engine hands it.

```
src/engine/mead_engine.*    brewing math: gravity, volumes, yeast limits, nutrients, sweetening
src/engine/mead_advisor.*   the advice: review notes, fruit character, styles, yeast ratings, the planner
src/engine/mead_text.*      the words on every screen: summaries, shopping list, timeline, math, file names
src/engine/mead_words.*     the word list and the underlined terms
src/engine/mead_spices.*    spice, herb and oak amounts (subtle / noticeable / strong)
src/engine/mead_json.hpp    small JSON reader/writer (data files, saved recipes)
src/app/                    hands the engine's results to the screens; saving and opening recipes
src/main.cpp                starts the app
qml/                        the screens and their shared parts
data/*.json                 fruits (USDA), yeasts (makers' specs, notes summarized), Schramm's Table 8.1,
                            honeys, juices, and an index of the books' recipes
tests/                      the math against the books' worked examples; the advice, styles, spices, dates,
                            words and file names; and ~600,000 checks of random plans against the books' rules
packaging/                  app icon, Windows installer (NSIS), Linux AppImage
.github/workflows/          build, test and release
```

The app started as a browser version, which some code comments and `tests/parity_cases.inc` (numbers only)
refer to; that version isn't part of this repository.

## Coming next

Journal and Tools screens (the calculators are already in the engine and tested), full metric units, and
printing the directions.

## Feedback

Found a wrong number, a confusing step or a bug? Open an **[issue](../../issues)** with what you entered and
what you expected. Screenshots help.

## License

MIT, see [LICENSE](LICENSE). The downloads also contain Qt and other open-source libraries under their own
licenses; see [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

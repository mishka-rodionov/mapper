# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

OpenOrienteering Mapper — a Qt/C++14 application for creating orienteering maps, targeting Linux, macOS, Windows and Android. All source code lives under the `OpenOrienteering` namespace.

## Build

The build system is CMake + Ninja. The local debug build is pre-configured in `build_debug/`.

```bash
# Build (from repo root)
cmake --build build_debug

# Or with explicit target
ninja -C build_debug Mapper
```

For a fresh configuration (e.g., new build directory):
```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -B build_debug .
cmake --build build_debug
```

Key CMake options:
- `Mapper_DEVELOPMENT_BUILD` — auto-enabled for Debug builds; enables ASan/UBSan and loads resources from the build tree
- `Mapper_USE_GDAL` — include GDAL support (default ON)
- `Mapper_WITH_COVE` — contour line vectorization (default ON on desktop)
- `Mapper_BUILD_CLIPPER=auto` — builds bundled Clipper if system library not found

## Testing

Tests are split into **unit tests** (linked to external libs only, fast to rebuild) and **system tests** (linked to `Mapper_Common`, slower). Individual test executables are in `build_debug/test/`.

```bash
# Run all auto-configured tests
ctest --test-dir build_debug

# Build and run a specific test executable
ninja -C build_debug course_t && build_debug/test/course_t

# Run all tests via CTest with output on failure
ctest --test-dir build_debug --output-on-failure
```

Test source files live in `test/` and are named `*_t.cpp` (system tests) or `tst_*.cpp` (unit tests). Each test has a corresponding `-RUN.cmake` script generated in `build_debug/test/`.

Per the project memory: don't manually UI-test Mapper — just build with ninja and report the result.

## Architecture

### Source tree (`src/`)

| Directory | Contents |
|---|---|
| `core/` | Map data model: `Map`, `MapColor`, `MapCoord`, `MapPart`, georeferencing, map printing |
| `core/objects/` | `Object` hierarchy: `PointObject`, `PathObject`, `TextObject`; boolean ops |
| `core/symbols/` | `Symbol` hierarchy: `PointSymbol`, `LineSymbol`, `AreaSymbol`, `TextSymbol`, `CombinedSymbol` |
| `core/renderables/` | `Renderable`/`RenderableImplementation` — paint objects to `QPainter` |
| `course/` | Course planning data model: `Course`, `CourseControl`, `CourseDatabase`, serialization, undo |
| `fileformats/` | File I/O: OCD import/export, IOF XML course export, KML course export, format registry |
| `gui/` | Qt widgets: `MainWindow`, `MapEditorController`, map widget, dialogs |
| `gui/map/` | `MapEditorController`, `MapWidget`, map-level dialogs |
| `gui/course/` | Course panel, control properties, ISCD symbol editor/browser, `CourseOverlay` |
| `gui/symbols/` | Symbol settings widgets (one per symbol type), `PointSymbolEditorWidget` |
| `templates/` | Template system: image, map, and track templates |
| `tools/` | Editor tools (draw path, cut, rotate, scale, etc.) |
| `undo/` | Undo/redo framework |
| `util/` | Utilities: encoding, coordinate XML, key-value container, etc. |
| `gdal/` | GDAL integration layer |
| `sensors/` | GPS/positioning integration |

### Key relationships

- `Map` owns `MapPart`s, each containing `Object`s. Objects reference `Symbol`s and hold `MapCoord` coordinates.
- `Symbol::createRenderables()` produces `Renderable` objects that `MapWidget` paints via `QPainter`.
- `MapEditorController` manages the active `MapEditorTool` and routes user input.
- The course module (`src/course/`, `src/gui/course/`) is a self-contained feature layered on top of the map: `CourseDatabase` holds `CourseControl`s, `Course`s hold ordered `CourseEntry` references, and `CourseOverlay` renders them onto the map canvas.
- File format detection and dispatch go through `FileFormatRegistry` → `FileFormat` → `FileImporter`/`FileExporter`.

### Conventions

- C++14, Qt 5 (AUTOMOC enabled). Use `QStringBuilder` (`QT_USE_QSTRINGBUILDER`); string literals require `QLatin1String(...)` or `u"..."_qs` due to `QT_NO_CAST_FROM_ASCII`.
- All classes are in the `OpenOrienteering` namespace.
- Headers use IWYU pragma annotations; keep includes minimal.
- Test files follow the naming `<name>_t.{cpp,h}` (system tests) or `tst_<name>.{cpp,h}` (unit tests).

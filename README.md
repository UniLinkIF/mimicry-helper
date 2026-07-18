# mimicry-helper

A thin CLI driver over `mimicry` (github.com/Baltram/rmtools, GPL-3.0) — the library that
understands Risen/Gothic/Elex's material and mesh property system. Built as a **strictly
separate, out-of-process helper** so that [RisenLab](../RisenLab) (MIT-ish) never links GPL
code directly — RisenLab just shells out to `mimicry-helper.exe`. See
`../RisenLab/docs/formats/content-layer.md` for the reasoning.

Because this binary links the GPL mimicry library, the resulting executable is a GPL-3.0
derivative work — see `LICENSE.txt` (copied from rmtools).

## Status (2026-07-18)

- ✅ `material-dump` — fully working. Verified against a real `.xmat` from the game: correctly
  walks the entire shader graph (samplers, combiners, constants) and prints every property
  name + type. This is the ~230-class property table baked into mimicry — the whole reason
  this project reuses it instead of re-deriving it.
- ✅ `mesh-to-obj` — fully working. Verified against a real `.xmsh` (`UI/Crosshair._xmsh`):
  produces a valid, Blender-openable `.obj` with correct vertices and texture coordinates.
- ✅ `obj-to-mesh` (import direction) — **fixed 2026-07-18**, now round-trips correctly
  (`obj-to-mesh` → `mesh-to-obj` on a synthetic triangle produces a valid `.obj` with the
  right vertex/face/material data — see `.github/workflows/build-and-test.yml`, which runs
  this exact round-trip on every push). Three real bugs in `mi_xmshwriter.cpp`, all found via
  a real MinGW build (this couldn't be diagnosed from source alone) plus hex-diffing the
  actual compiled binary's output against what `mi_xmshreader.cpp` expects:
  1. The vertex-declaration table wrote only 9 of the 16 entries the reader unconditionally
     reads (72 vs. 128 bytes) — every byte after it was shifted by the missing 56.
  2. The `MaterialName` property's size field mixed a `MIU32` with `sizeof(MIU16)` (a
     `size_t`) in one addition — on this 64-bit MinGW build that silently widened the whole
     expression to 8 bytes where the reader expects exactly 4 (harmless on the original
     32-bit MSVC target, where `size_t` was 4 bytes too).
  3. A stray, pre-existing 56-byte zero block sat between the declaration table and the
     vertex-stream-size field, which the reader never skips there (it reads that field
     immediately after the table) — this alone caused `uVertexStreamSize`/`uVertexSize` to
     both read as 0, an integer division by zero.
  Still worth real-world testing against an actual `._xmsh` (not just the synthetic CI
  fixture) and, ultimately, the real game engine before trusting it for mesh replacement.
- Not attempted yet: animations (`.xact`), collision meshes (`.xcom` — excluded from the build
  entirely, needs the PhysX-dependent `mi_cooking.cpp` which we don't have the SDK for).

## Porting notes

`mimicry` targets an old MSVC (VS2008/2010-era `.vcproj` files) and doesn't compile as-is under
a modern, standards-strict GCC. All fixes are minimal, mechanical, and documented inline where
applied, in `vendor/mimicry-source/Mimicry/`:

1. **Missing `typename`** (`mi_array.inl`): `mSTypeTagHelper<T>::msTag` used as a template
   argument without `typename` — old MSVC didn't enforce two-phase lookup here, GCC does.
2. **Unqualified dependent-base-class member access** (`mi_array.inl`, `mi_map.inl`):
   `CIterator` inherits `m_pElements`/`m_uIndex` (or `m_pBuckets`) from a template-dependent
   base (`CConstIterator`) but accessed them unqualified — needs `this->` per the standard;
   without it, GCC actually resolved the name to the *enclosing* class's same-named member,
   producing confusing "invalid use of non-static data member" errors.
3. **Bare base-class name in a derived template's mem-initializer-list** (`mi_map.inl`):
   `mTStringMap`/`mTNameMap`/`mTIDMap` all did `mTMap( a_uMinCapacity )` to delegate to their
   base constructor, but `mTMap` is a template name, not the specific base instantiation —
   needed the full `mTMap< K, T, C >( ... )`. Under `-fpermissive` this silently degrades to a
   *skipped* base-constructor call rather than an error, which would have been a real,
   silent correctness bug (uninitialized base state) if left as a warning.
4. **Specialization declared after implicit instantiation** (`mi_ostream.h`): added forward
   declarations of every `mTOStream<M>::operator<<` specialization right after the class,
   before anything could implicitly instantiate the primary (nonexistent) template.
5. **`g_min(float, double)` ambiguity** (`mi_vertexmatcher.cpp`): `sqrt()` on a float promoted
   to `double`; added an explicit cast back to `MIFloat`.
6. **Missing semicolon** (`mi_array.inl`, `operator!=`) — a plain bug, unrelated to porting.

`-fpermissive` is still used for the build as a whole; remaining warnings under it are in code
paths this driver doesn't exercise (not yet reviewed further).

## Building

Requires MinGW-w64 g++ (this was built with WinLibs' GCC 16.1.0, `-std=gnu++17`). From this
directory:

```sh
bash build.sh
```

Produces `mimicry-helper.exe`.

CI (`.github/workflows/build-and-test.yml`) builds this same way on a real Windows+MinGW
GitHub Actions runner on every push, then runs the `obj-to-mesh` → `mesh-to-obj` round-trip
smoke test against `test-fixtures/triangle.obj` — this exists specifically because no C++
compiler was available in the environment that found and fixed the three bugs above; without
a real build, the whole class of bug (a compiler silently doing something 4 bytes different
than the source implies) is invisible from reading the code.

## Usage

```
mimicry-helper mesh-to-obj <in.xmsh> <out.obj>
mimicry-helper obj-to-mesh <in.obj> <out.xmsh>
mimicry-helper material-dump <in.xmat> <out.txt>
```

RisenLab calls this automatically (`risenlab mesh-to-obj`/`material-dump`/`obj-to-mesh`) via
`src/content.rs`, looking for `../mimicry-helper/mimicry-helper.exe` relative to itself, or the
`RISENLAB_MIMICRY_HELPER` environment variable.

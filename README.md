# mimicry-helper

A thin CLI driver over `mimicry` (github.com/Baltram/rmtools, GPL-3.0) — the library that
understands Risen/Gothic/Elex's material and mesh property system. Built as a **strictly
separate, out-of-process helper** so that [RisenLab](../RisenLab) (MIT-ish) never links GPL
code directly — RisenLab just shells out to `mimicry-helper.exe`. See
`../RisenLab/docs/formats/content-layer.md` for the reasoning.

Because this binary links the GPL mimicry library, the resulting executable is a GPL-3.0
derivative work — see `LICENSE.txt` (copied from rmtools).

## Status (2026-07-14)

- ✅ `material-dump` — fully working. Verified against a real `.xmat` from the game: correctly
  walks the entire shader graph (samplers, combiners, constants) and prints every property
  name + type. This is the ~230-class property table baked into mimicry — the whole reason
  this project reuses it instead of re-deriving it.
- ✅ `mesh-to-obj` — fully working. Verified against a real `.xmsh` (`UI/Crosshair._xmsh`):
  produces a valid, Blender-openable `.obj` with correct vertices and texture coordinates.
- ⚠️ `obj-to-mesh` (import direction) — writes a file, but it doesn't yet re-parse via
  `mesh-to-obj`. Not yet root-caused; needs more investigation before it's safe to use for
  real mesh replacement.
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

## Usage

```
mimicry-helper mesh-to-obj <in.xmsh> <out.obj>
mimicry-helper obj-to-mesh <in.obj> <out.xmsh>      # writes, but doesn't yet re-parse — see Status
mimicry-helper material-dump <in.xmat> <out.txt>
```

RisenLab calls this automatically (`risenlab mesh-to-obj`/`material-dump`/`obj-to-mesh`) via
`src/content.rs`, looking for `../mimicry-helper/mimicry-helper.exe` relative to itself, or the
`RISENLAB_MIMICRY_HELPER` environment variable.

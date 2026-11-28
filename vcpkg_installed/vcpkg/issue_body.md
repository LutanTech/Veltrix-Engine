Package: sdl3-image:x64-windows@3.4.8

**Host Environment**

- Host: x64-windows
- Compiler: MSVC 19.44.35230.0
- CMake Version: 3.31.6-msvc6
-    vcpkg-tool version: 2025-11-19-da1f056dc0775ac651bea7e3fbbf4066146a55f3
    vcpkg-readonly: true
    vcpkg-scripts version: 3af1d1e60af2b2abf55760538cd607829029b07a

**To Reproduce**

`vcpkg install `

**Failure logs**

```
-- Using cached libsdl-org-SDL_image-release-3.4.8.tar.gz
-- Extracting source C:/Users/Administrator/AppData/Local/vcpkg/downloads/libsdl-org-SDL_image-release-3.4.8.tar.gz
-- Applying patch dependencies.diff
-- Applying patch pkgconfig-libname.diff
CMake Error at scripts/cmake/vcpkg_extract_source_archive.cmake:153 (file):
  file RENAME failed to rename

    D:/PYTHON/Veltrix Engine/vcpkg_installed/vcpkg/blds/sdl3-image/src/ease-3.4.8-208d6f9c5d.clean.tmp/SDL_image-release-3.4.8

  to

    D:/PYTHON/Veltrix Engine/vcpkg_installed/vcpkg/blds/sdl3-image/src/ease-3.4.8-208d6f9c5d.clean

  because: Access is denied.



Call Stack (most recent call first):
  scripts/cmake/vcpkg_extract_source_archive_ex.cmake:8 (vcpkg_extract_source_archive)
  scripts/cmake/vcpkg_from_github.cmake:127 (vcpkg_extract_source_archive_ex)
  C:/Users/Administrator/AppData/Local/vcpkg/registries/git-trees/d6d617a9dade08eb3d2313174fd52b4098c13f48/portfile.cmake:1 (vcpkg_from_github)
  scripts/ports.cmake:206 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "name": "veltrix",
  "version-string": "0.1.0",
  "builtin-baseline": "e1ff419d28c7f0a94abaf0a848fe0aeeb69b2de3",
  "dependencies": [
    "sdl3",
    "lexbor",
    "sdl3-image"
  ]
}

```
</details>

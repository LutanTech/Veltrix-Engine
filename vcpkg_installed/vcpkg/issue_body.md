Package: freetype[brotli,bzip2,core,png,zlib]:x64-windows@2.15.0

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
Downloading https://gitlab.freedesktop.org//freetype/freetype/-/archive/VER-2-15-0/freetype-VER-2-15-0.tar.gz -> freetype-freetype-VER-2-15-0.tar.gz
warning: Download https://gitlab.freedesktop.org//freetype/freetype/-/archive/VER-2-15-0/freetype-VER-2-15-0.tar.gz failed -- retrying after 1000ms
warning: Download https://gitlab.freedesktop.org//freetype/freetype/-/archive/VER-2-15-0/freetype-VER-2-15-0.tar.gz failed -- retrying after 2000ms
warning: Download https://gitlab.freedesktop.org//freetype/freetype/-/archive/VER-2-15-0/freetype-VER-2-15-0.tar.gz failed -- retrying after 4000ms
error: https://gitlab.freedesktop.org//freetype/freetype/-/archive/VER-2-15-0/freetype-VER-2-15-0.tar.gz: WinHttpSendRequest failed with exit code 12002. The operation timed out
note: If you are using a proxy, please ensure your proxy settings are correct.
Possible causes are:
1. You are actually using an HTTP proxy, but setting HTTPS_PROXY variable to `https://address:port`.
This is not correct, because `https://` prefix claims the proxy is an HTTPS proxy, while your proxy (v2ray, shadowsocksr, etc...) is an HTTP proxy.
Try setting `http://address:port` to both HTTP_PROXY and HTTPS_PROXY instead.
2. If you are using Windows, vcpkg will automatically use your Windows IE Proxy Settings set by your proxy software. See: https://github.com/microsoft/vcpkg-tool/pull/77
The value set by your proxy might be wrong, or have same `https://` prefix issue.
3. Your proxy's remote server is out of service.
If you believe this is not a temporary download server failure and vcpkg needs to be changed to download this file from a different location, please submit an issue to https://github.com/Microsoft/vcpkg/issues
CMake Error at scripts/cmake/vcpkg_download_distfile.cmake:136 (message):
  Download failed, halting portfile.
Call Stack (most recent call first):
  scripts/cmake/vcpkg_from_gitlab.cmake:113 (vcpkg_download_distfile)
  C:/Users/Administrator/AppData/Local/vcpkg/registries/git-trees/7ef99797b2d4d2f3c893cdcdfc18c01438fc73c7/portfile.cmake:7 (vcpkg_from_gitlab)
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
    "freetype"
  ]
}

```
</details>

# Building PvPGN

## Prerequisites

- [CMake](https://cmake.org/download/) 3.16 or later
- A C++17-capable compiler (GCC 9+, Clang 10+, or Visual Studio 2015+)
- [zlib](http://zlib.net/)

**Optional storage backends:**
- [MySQL](http://dev.mysql.com/downloads/mysql/)
- [PostgreSQL](http://www.postgresql.org/download/)
- [SQLite](https://sqlite.org/download.html)

**Optional scripting:**
- [Lua](https://www.lua.org/) 5.4

---

## Building with GCC/Clang (Linux/macOS)

> The original guide referenced an external article at http://harpywar.com/?a=articles&b=2&c=1&d=74&lang=en

For a standard Linux/macOS build using GCC or Clang, use the CMake preset workflow described in the section below.

---

## Building with Visual Studio 2015+ (Windows)

### Requirements

- [Visual Studio 2015 Community](https://www.visualstudio.com/products/visual-studio-community-vs) or later
- [CMake](https://cmake.org/download/)
- [zlib](http://zlib.net/)
- [PvPGN](https://github.com/pvpgn/pvpgn-server)

**Optional:**
- [MySQL](http://dev.mysql.com/downloads/mysql/)
- [PostgreSQL](http://www.postgresql.org/download/windows/)
- [SQLite](https://sqlite.org/download.html)

### Instructions

- Install *Visual Studio 2015* (or later)
- Install *CMake*
- Download zlib
- Download PvPGN source
- Extract PvPGN source to `C:\pvpgn\`
- Create a folder called *zlib* inside your PvPGN base folder (i.e. `C:\pvpgn\zlib\`)
- Extract the following files from `zlib128-dll.zip\include` into the *zlib* folder:
    - `zconf.h`, `zlib.h`
- Extract the following files from `zlib128-dll.zip\lib` into the *zlib* folder:
    - `zdll.lib`
- Install *MySQL* if you intend to use it as a storage backend
- Install *PostgreSQL* if you intend to use it as a storage backend
- Install *SQLite* if you intend to use it as a storage backend
- Create a folder called *build* inside your PvPGN base folder (i.e. `C:\pvpgn\build\`)
    - Run *cmake-gui.exe*
        - *Where is the source code*: `C:\pvpgn`
        - *Where to build the binaries*: `C:\pvpgn\build`
    - Click the *Configure* button
        - Select *Visual Studio 14 2015* (or your installed version) as the generator
        - Select *Use default native compilers*
        - Click the *Finish* button
- Enable optional components if required (`WITH_MYSQL`, `WITH_PGSQL`, `WITH_SQLITE3`, `WITH_LUA`, `WITH_WIN32_GUI`)
- Click the *Configure* button again
- Click the *Generate* button
- Close *CMake*
- Open `C:\pvpgn\build\pvpgn.sln` with Visual Studio
- Build the *ALL_BUILD* project
- Build the *INSTALL* project
- Open `C:\Program Files (x86)\pvpgn`
- Extract the following files from `zlib128-dll.zip` into the *pvpgn* folder:
    - `zlib1.dll`
- If required, extract the following files from `C:\Program Files\MySQL\MySQL Server 5.6.23\lib\opt\` into the *pvpgn* folder:
    - `libmysql.dll`
- If required, extract the following files from `C:\Program Files\PostgreSQL\9.4.1\bin\` into the *pvpgn* folder:
    - `libpq.dll`, `libintl-2.dll`, `libiconv-2.dll`, `krb5_32.dll`, `comerr32.dll`
- If required, extract the following files from `sqlite-dll-win32-x86-3080803.zip` into the *pvpgn* folder:
    - `sqlite3.dll`

---

## CMake Presets (Recommended)

The recommended workflow for all platforms is to use the bundled CMake presets:

```bash
cmake --preset v3-dev
cmake --build --preset v3-dev
```

This configures and builds the v3 development target with all recommended settings. Run `cmake --list-presets` to see all available presets.

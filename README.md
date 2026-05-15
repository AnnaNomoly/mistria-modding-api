# MMAPI

MMAPI is an unofficial, header-only C++ modding API for [Fields of Mistria](https://www.fieldsofmistria.com/) mods built with [YYToolkit](https://github.com/AurieFramework/YYToolkit) and [Aurie](https://github.com/AurieFramework/Aurie).

It abstracts common Fields of Mistria systems behind typed C++ helpers and hooks, including player state, items, inventory, locations, weather, dungeons, status effects, UI, text, input, and more.

MMAPI is designed to be included directly by each mod at build time. It does not load as its own DLL. At runtime, MMAPI uses the mod's YYTK and Aurie module pointers to call game scripts, install hooks, and expose game behavior through public C++ APIs.

## Installation

Install the `MMAPI` NuGet package in your native C++ mod project.

MMAPI is header-only. After installing the package, include the main header:

```cpp
#include <MMAPI/MMAPI.hpp>
```

MMAPI depends on:

- Aurie
- YYToolkit
- nlohmann.json

The NuGet package declares `nlohmann.json` as a dependency.

## Documentation

The MMAPI documentation is available on the GitHub wiki:

[MMAPI Wiki](https://github.com/AnnaNomoly/mistria-modding-api/wiki)

Start there for setup, namespace pages, hook pages, context types, and helper functions.

## License

MMAPI is licensed under AGPL-3.0-only. See [`LICENSE`](LICENSE).

Copyright (c) 2026 AnnaNomoly  
https://github.com/AnnaNomoly/mistria-modding-api
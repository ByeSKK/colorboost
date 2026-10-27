\# ColorBoost



An overlay for Windows that applies color filters (gamma, temperature,

shadows, saturation, etc.) to the desktop in real time.



\## Features

\- Capture via DXGI Desktop Duplication

\- Profiles for the active application

\- Presets, keyboard shortcuts, tray

\- Multi-monitor and DPI support

\## Assembly

1\. Install Visual Studio 2022 with "Desktop development with C++".

2\. Install vcpkg and dependencies:

vcpkg install imgui nlohmann-json

3\. Open `ColorBoost.sln', build it in `Release x64'.



\## Keyboard shortcuts

| Keys | Action |

|---|---|

| Ctrl+Alt+F10 | Show/Hide panel |

| Ctrl+Alt+F9 | On/off effect |

| Ctrl+Alt+F11 | Next preset |

| Ctrl+Alt+F8 | Reset settings |


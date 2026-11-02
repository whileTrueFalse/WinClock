# WinClock

World clocks for your Windows desktop and taskbar. Tiny, fast, and terminal-styled.

![WinClock](docs/table.png)

## Install

1. Download **[WinClock-Setup.exe](https://github.com/whileTrueFalse/WinClock/releases/latest/download/WinClock-Setup.exe)**.
2. Run it and click **Install**. No admin rights are needed.
3. Done. Your clocks appear on the desktop and next to the taskbar clock.

> Windows may show "Windows protected your PC" because the app isn't code-signed yet. Click **More info → Run anyway**.

To uninstall, go to **Settings → Apps → WinClock → Uninstall**.

## How to use

**Right-click any clock** to open the menu. Everything lives there.

| To… | Do this |
|---|---|
| Add cities or whole countries | Menu → **Add clocks / countries…** (Ctrl-click to pick several) |
| Move the widget | Drag it |
| Change the look | Menu → **Layout** or **Theme** |
| Move the taskbar clocks | Menu → **Taskbar → Position** (left, right or drag) |
| Plan a meeting | Scroll the mouse wheel over the clocks to move time; middle-click to reset |
| Set an alarm or countdown | Menu → **Alarms & countdowns** |
| Show or hide the widget | **Ctrl + Alt + W** |

## Features

- Four layouts: **table**, **analog**, **timeline planner** and **compact**
- Clocks on the desktop, on the taskbar and in a pop-up
- Shows who is **working**, **free** or **asleep** right now
- **Find next meeting slot** across all your cities
- **Jump to a time**, e.g. "3 pm in New York tomorrow"
- Alarms and countdowns in any time zone, with Windows notifications
- Seven themes, including light/dark auto
- Very light: one ~400 KB app, a few MB of memory, near-zero CPU

<details>
<summary>More screenshots</summary>

![Timeline](docs/timeline.png)
![Analog](docs/analog.png)
![Compact](docs/compact.png)
![Themes](docs/themes.png)

</details>

## Build from source

You need [MSYS2](https://www.msys2.org/) or [w64devkit](https://github.com/skeeto/w64devkit) on Windows (or `mingw-w64` on Linux).

```bat
cd src
build.bat
```

To build the installer, put `WinClock.exe` in `installer\` and run `makensis installer\winclock.nsi` ([NSIS](https://nsis.sourceforge.io)).

Releases are automatic: raise the number in `VERSION` and push to `main`. GitHub then builds the app and installer and publishes a new release.

# WinClock 2.0

A tiny native world-clock widget for Windows 10/11 that shows on the desktop **and** the taskbar.
It has a terminal / TUI look: deep navy, periwinkle and hot pink, with a monospace font, bracket-titled panels and a vim-style status bar.

![Table layout](docs/table.png)

It's one native `.exe` of about 390 KB with no installer and no runtime. Settings are stored in `WinClock.ini` next to it.

## Adding clocks
Right-click → **Add clocks / countries…**, then search a city, country or zone. **Ctrl/Shift-click** to add several at once. Entries starting with `>` add a whole country in one go: United States (7 zones), Canada, Australia, Russia (12), Brazil, Mexico, Indonesia, New Zealand, Chile, Portugal, Spain, plus Europe and Asia business-hub presets.

## Where clocks appear
* **Desktop widget:** pinned to the desktop (it stays visible with Win+D) or set to always on top. Always-on-top mode hides itself while a full-screen app is running.
* **Taskbar:** choose **Position → left edge**, **right (next to the tray)** or **custom** (drag them anywhere along the taskbar). Click them for a pop-up with every clock, which rises out of the taskbar. You can switch off the taskbar clocks entirely.
* **Tray icon:** hover to see all times. Click to show or hide the widget. Right-click opens the menu. You can hide it with **Taskbar → Show tray icon**; alarm notifications still appear.

## Layouts (right-click → Layout)
* **Table:** columns for ID, city, time, state (working / free / asleep), offset, and a dotted day-progress bar.
* **Analog faces:** periwinkle ring by day, dim ring by night, pink seconds hand.
* **Timeline planner:** a 24-hour strip per city. Periwinkle marks working hours and the dotted pattern marks night. The **pink band** shows when everyone is working. Hover the strips to read every city's time at that moment.
* **Compact:** one prompt-style line per city.

The status bar shows the mode on the left (`NORMAL`, `SHIFT` or `ALARM`), the next alarm countdown in the middle, and UTC time on the right.

## Typography
Words use the Windows system font, **Segoe UI Variable**, which switches between its Small, Text and Display optical cuts by size. Times and numbers use **Cascadia Mono**, whose tabular digits don't jitter as they tick. Hierarchy comes from weight and size together, secondary text is high-contrast, and ClearType is used wherever the panel is opaque. The taskbar pop-up's entrance animation respects Windows' *Animation effects* setting.

## Themes
![Themes](docs/themes.png)

Terminal (default), Paper (light), Synthwave, Periwinkle, Glass, Transparent, and Auto (follows Windows light/dark mode).

![Timeline planner](docs/timeline.png)

## Planning tools (right-click menu)
* **Alarms & countdowns:** set a time in any city's zone. Each one repeats once, daily, on weekdays or on weekends; add a date for a one-off countdown. Alarms fire a Windows notification with a sound and work even while the screen is off.
* **Jump to a time:** for example "3 pm in New York, tomorrow" shows every clock at that moment.
* **Find next meeting slot:** jumps to the next time when every city is inside working hours.
* **Mouse wheel** shifts all clocks ±15 min (Shift+wheel ±1 h). **Middle-click** resets.
* **Hourly chime**, a choice of working hours, 12/24 h, seconds, opacity, size (Ctrl+wheel), click-through and lock.
* Tooltips show the full date, ISO week, day of year, UTC offset, daylight saving and working status.
* Ctrl+Alt+W shows or hides the widget.

## Light on resources
* Pure Win32 + GDI+ with no frameworks and no browser engine.
* Redraws only when the displayed text changes. That's once a second with seconds on and once a minute otherwise.
* Drawing pauses while the screen is off or the PC is locked; alarms still fire.
* Trims its memory after start-up and after menus close.

## Build
Windows with [MSYS2](https://www.msys2.org/) or [w64devkit](https://github.com/skeeto/w64devkit) (MinGW-w64 g++ and windres on PATH):

```bat
cd src
build.bat
```

Cross-compiling from Linux works too:

```sh
cd src
x86_64-w64-mingw32-windres resource.rc -O coff -o resource.o
x86_64-w64-mingw32-g++ -std=c++17 -Os -municode -mwindows -static -s main.cpp resource.o -o WinClock.exe \
  -lgdiplus -lcomctl32 -ldwmapi -lwtsapi32 -lshell32 -ladvapi32 -lole32 -lpsapi -lwinmm
```

Add `-DPREVIEW_BUILD` to get a build that renders every layout and theme to PNG files and then exits. It's handy for checking design changes.

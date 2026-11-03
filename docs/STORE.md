# Putting WinClock in the Microsoft Store

Every release builds **WinClock-Store.msix**, a Store version of WinClock. It is the same app, except that the Store installs and updates it, so its own updater is switched off.

The Store signs MSIX packages itself after they pass review. You don't need to buy a code-signing certificate, and Store installs don't show the "Windows protected your PC" warning.

## One-time setup (about 20 minutes)

1. Go to **[storedeveloper.microsoft.com](https://storedeveloper.microsoft.com)** and choose **Get started for free → Individual developer (free)**. You'll verify your identity with a photo ID and a selfie.
2. In Partner Center, go to **Apps and games → New product → MSIX or PWA app** and reserve the name **WinClock**. If it's taken, reserve another name, such as "WinClock World Clocks".
3. Open the app → **Product management → Product identity**. Copy three values into [`store/identity.env`](../store/identity.env):
   - `Package/Identity/Name` → `IDENTITY_NAME`
   - `Package/Identity/Publisher` → `PUBLISHER` (starts with `CN=`)
   - `Package/Properties/PublisherDisplayName` → `PUBLISHER_DISPLAY`
4. Raise the number in `VERSION` and push. The release now includes a WinClock-Store.msix built with your identity.

## Each submission

1. In Partner Center, open the app → **Start your submission**.
2. **Packages:** upload `WinClock-Store.msix` from the latest release.
3. **Pricing and availability:** Free, all markets.
4. **Properties:** category *Utilities & tools*. WinClock collects no personal data. The update check and the optional weather and calendar features talk to GitHub, Open-Meteo and your calendar link; say so in the privacy section.
5. **Age ratings:** complete the questionnaire. There is no user-generated content.
6. **Store listing:** use the text below and screenshots from [`docs/`](./).
7. Submit. Certification usually takes 1–3 business days.

Partner Center will ask why the app needs `runFullTrust`. Answer: *"WinClock is a classic Win32 desktop app (layered desktop widget and taskbar clocks). It needs full trust to draw on the desktop and next to the taskbar."*

### Listing text

**Short description:** World clocks for your desktop and taskbar, with a time calculator, meeting planner and focus timer.

**Description:**

WinClock keeps every city and teammate you work with one glance away, on the desktop and right on the taskbar.

• Ask in plain words: "9pm in India, what time in Germany?" or "how long until 6pm London".
• See who is working, free or asleep, with their own hours, days off and public holidays.
• Find meeting times that are fair to everyone, and copy a ready-made invite.
• Focus timer, stopwatch, alarms and leave-by reminders in any time zone.
• Day/night world map, stock-market hours, weather, sunrise and sunset.
• Tiny and fast: a few MB of memory and almost no CPU.

## Where things differ in the Store version

- **Updates** come from the Store. *Check for updates* opens the Store's updates page.
- **Start with Windows** is managed in Windows Settings → Apps → Startup. It's on after the first launch.
- **Settings** live in `%APPDATA%\WinClock\WinClock.ini`.

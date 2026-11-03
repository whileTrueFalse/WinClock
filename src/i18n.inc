// ---------------------------------------------------------------- languages: English, Hindi, German, Spanish
// Menus, the widget's words and day/month names are translated. Dialog boxes stay in English for now.
struct Tr { const wchar_t* en; const wchar_t* hi; const wchar_t* de; const wchar_t* es; };
static const Tr kTr[] = {
    // main menu
    { L"Add clocks / countries…", L"घड़ियाँ / देश जोड़ें…", L"Uhren / Länder hinzufügen…", L"Añadir relojes / países…" },
    { L"Clocks", L"घड़ियाँ", L"Uhren", L"Relojes" },
    { L"Edit / change zone…", L"बदलें / समय क्षेत्र…", L"Bearbeiten / Zone ändern…", L"Editar / cambiar zona…" },
    { L"Person, hours & holidays…", L"व्यक्ति, समय और छुट्टियाँ…", L"Person, Arbeitszeit & Feiertage…", L"Persona, horario y festivos…" },
    { L"Copy this time", L"यह समय कॉपी करें", L"Diese Zeit kopieren", L"Copiar esta hora" },
    { L"Show on taskbar", L"टास्कबार पर दिखाएँ", L"In der Taskleiste zeigen", L"Mostrar en la barra de tareas" },
    { L"Use for the tray icon", L"ट्रे आइकन के लिए", L"Für das Tray-Symbol", L"Usar para el icono" },
    { L"Move up", L"ऊपर ले जाएँ", L"Nach oben", L"Subir" },
    { L"Move down", L"नीचे ले जाएँ", L"Nach unten", L"Bajar" },
    { L"Remove", L"हटाएँ", L"Entfernen", L"Quitar" },
    { L"New profile from these clocks…", L"इन घड़ियों से नई प्रोफ़ाइल…", L"Neues Profil aus diesen Uhren…", L"Nuevo perfil con estos relojes…" },
    { L"Switch automatically (Work in work hours, Personal otherwise)", L"अपने आप बदलें (काम के समय Work, बाकी Personal)", L"Automatisch wechseln (Work zur Arbeitszeit, sonst Personal)", L"Cambiar solo (Work en horario laboral, si no Personal)" },
    { L"Convert time from clipboard", L"कॉपी किया समय बदलें", L"Zeit aus Zwischenablage umrechnen", L"Convertir hora del portapapeles" },
    { L"Jump to a time…", L"किसी समय पर जाएँ…", L"Zu einer Zeit springen…", L"Ir a una hora…" },
    { L"Find next meeting slot", L"अगला मीटिंग समय खोजें", L"Nächsten Termin finden", L"Buscar hueco para reunión" },
    { L"Meeting card / invite…", L"मीटिंग कार्ड / निमंत्रण…", L"Terminkarte / Einladung…", L"Tarjeta / invitación…" },
    { L"Meeting card / invite for this time…", L"इस समय का मीटिंग कार्ड…", L"Terminkarte für diese Zeit…", L"Tarjeta para esta hora…" },
    { L"Pin this moment…", L"यह पल पिन करें…", L"Diesen Moment anheften…", L"Fijar este momento…" },
    { L"Pin this planned moment…", L"यह योजना पिन करें…", L"Geplanten Moment anheften…", L"Fijar el momento planeado…" },
    { L"Show it on the clocks", L"घड़ियों पर दिखाएँ", L"Auf den Uhren zeigen", L"Mostrar en los relojes" },
    { L"Copy meeting card", L"मीटिंग कार्ड कॉपी करें", L"Terminkarte kopieren", L"Copiar tarjeta" },
    { L"Pinned moments", L"पिन किए पल", L"Angeheftete Momente", L"Momentos fijados" },
    { L"Fairness report…", L"निष्पक्षता रिपोर्ट…", L"Fairness-Bericht…", L"Informe de equidad…" },
    { L"Daylight-saving radar…", L"डेलाइट-सेविंग रडार…", L"Sommerzeit-Radar…", L"Radar de horario de verano…" },
    { L"Time calculator…", L"समय कैलकुलेटर…", L"Zeitrechner…", L"Calculadora de horas…" },
    { L"Plan", L"योजना", L"Planen", L"Planificar" },
    { L"Add alarm or countdown…", L"अलार्म या उलटी गिनती जोड़ें…", L"Wecker oder Countdown…", L"Añadir alarma o cuenta atrás…" },
    { L"Edit…", L"बदलें…", L"Bearbeiten…", L"Editar…" },
    { L"Enabled", L"चालू", L"Aktiv", L"Activada" },
    { L"Delete", L"मिटाएँ", L"Löschen", L"Eliminar" },
    { L"Alarms & countdowns", L"अलार्म और उलटी गिनती", L"Wecker & Countdowns", L"Alarmas y cuentas atrás" },
    { L"Connect a calendar link (.ics)…", L"कैलेंडर लिंक (.ics) जोड़ें…", L"Kalenderlink (.ics) verbinden…", L"Conectar enlace de calendario (.ics)…" },
    { L"Change calendar link…", L"कैलेंडर लिंक बदलें…", L"Kalenderlink ändern…", L"Cambiar enlace de calendario…" },
    { L"Show next meeting", L"अगली मीटिंग दिखाएँ", L"Nächsten Termin zeigen", L"Mostrar próxima reunión" },
    { L"Refresh now", L"अभी ताज़ा करें", L"Jetzt aktualisieren", L"Actualizar ahora" },
    { L"Disconnect", L"हटाएँ", L"Trennen", L"Desconectar" },
    { L"Calendar", L"कैलेंडर", L"Kalender", L"Calendario" },
    { L"Tools", L"औज़ार", L"Werkzeuge", L"Herramientas" },
    { L"Show", L"दिखाएँ", L"Anzeigen", L"Mostrar" },
    { L"Analog columns", L"एनालॉग कॉलम", L"Analog-Spalten", L"Columnas analógicas" },
    { L"Taskbar pop-up layout", L"टास्कबार पॉप-अप लेआउट", L"Layout des Taskleisten-Pop-ups", L"Diseño de la ventana emergente" },
    { L"Layout", L"लेआउट", L"Layout", L"Diseño" },
    { L"Theme", L"थीम", L"Design", L"Tema" },
    { L"Size  (Ctrl + wheel)", L"आकार  (Ctrl + व्हील)", L"Größe  (Strg + Mausrad)", L"Tamaño  (Ctrl + rueda)" },
    { L"Opacity", L"अपारदर्शिता", L"Deckkraft", L"Opacidad" },
    { L"24-hour time", L"24-घंटे का समय", L"24-Stunden-Format", L"Formato 24 horas" },
    { L"Show seconds", L"सेकंड दिखाएँ", L"Sekunden zeigen", L"Mostrar segundos" },
    { L"Show dates", L"तारीख दिखाएँ", L"Datum zeigen", L"Mostrar fechas" },
    { L"Hourly chime", L"हर घंटे की घंटी", L"Stündlicher Ton", L"Aviso cada hora" },
    { L"Default working hours", L"सामान्य काम के घंटे", L"Standard-Arbeitszeit", L"Horario laboral" },
    { L"Appearance", L"रूप", L"Aussehen", L"Apariencia" },
    { L"Pin to desktop", L"डेस्कटॉप पर रखें", L"Auf dem Desktop", L"Fijar al escritorio" },
    { L"Always on top (hides during full-screen apps)", L"हमेशा ऊपर (फ़ुल-स्क्रीन में छिपे)", L"Immer im Vordergrund (bei Vollbild aus)", L"Siempre encima (se oculta a pantalla completa)" },
    { L"Lock position", L"जगह लॉक करें", L"Position sperren", L"Bloquear posición" },
    { L"Click-through (use tray icon to undo)", L"क्लिक-थ्रू (ट्रे आइकन से वापस)", L"Durchklickbar (über Tray-Symbol zurück)", L"Clic a través (deshacer con el icono)" },
    { L"Move to monitor", L"मॉनिटर पर ले जाएँ", L"Auf Monitor verschieben", L"Mover al monitor" },
    { L"Desktop widget", L"डेस्कटॉप विजेट", L"Desktop-Widget", L"Widget de escritorio" },
    { L"Show clocks on taskbar", L"टास्कबार पर घड़ियाँ", L"Uhren in der Taskleiste", L"Relojes en la barra de tareas" },
    { L"Show all my clocks", L"मेरी सभी घड़ियाँ", L"Alle meine Uhren", L"Todos mis relojes" },
    { L"Show only the ticked clocks:", L"सिर्फ़ चुनी हुई घड़ियाँ:", L"Nur die markierten Uhren:", L"Solo los relojes marcados:" },
    { L"Side by side (rotates if they don't fit)", L"साथ-साथ (जगह कम हो तो बारी-बारी)", L"Nebeneinander (wechselnd, wenn zu breit)", L"Uno al lado del otro (rotan si no caben)" },
    { L"One at a time, rotating", L"एक-एक करके", L"Einzeln, wechselnd", L"De uno en uno, rotando" },
    { L"Show city labels", L"शहर के नाम दिखाएँ", L"Städtenamen zeigen", L"Mostrar nombres de ciudad" },
    { L"Position: left edge", L"जगह: बाएँ किनारे", L"Position: linker Rand", L"Posición: borde izquierdo" },
    { L"Position: right, next to tray", L"जगह: दाएँ, ट्रे के पास", L"Position: rechts, neben dem Tray", L"Posición: derecha, junto a la bandeja" },
    { L"Position: custom (drag the clocks)", L"जगह: अपनी (खींचें)", L"Position: frei (Uhren ziehen)", L"Posición: libre (arrastrar)" },
    { L"Also on other monitors' taskbars", L"दूसरे मॉनिटरों पर भी", L"Auch auf anderen Monitoren", L"También en otros monitores" },
    { L"Show tray icon", L"ट्रे आइकन दिखाएँ", L"Tray-Symbol zeigen", L"Mostrar icono en la bandeja" },
    { L"Tray icon shows a city's hour", L"ट्रे आइकन में शहर का घंटा", L"Tray-Symbol zeigt Stunde einer Stadt", L"El icono muestra la hora de una ciudad" },
    { L"Taskbar", L"टास्कबार", L"Taskleiste", L"Barra de tareas" },
    { L"Command palette…", L"कमांड पैलेट…", L"Befehlspalette…", L"Paleta de comandos…" },
    { L"Settings…", L"सेटिंग्स…", L"Einstellungen…", L"Configuración…" },
    { L"Copy share code for these clocks", L"इन घड़ियों का शेयर कोड कॉपी करें", L"Freigabecode kopieren", L"Copiar código para compartir" },
    { L"Import a share code…", L"शेयर कोड डालें…", L"Freigabecode importieren…", L"Importar código…" },
    { L"Share", L"शेयर", L"Teilen", L"Compartir" },
    { L"Copy all times", L"सभी समय कॉपी करें", L"Alle Zeiten kopieren", L"Copiar todas las horas" },
    { L"Hide widget", L"विजेट छिपाएँ", L"Widget ausblenden", L"Ocultar widget" },
    { L"Show widget", L"विजेट दिखाएँ", L"Widget zeigen", L"Mostrar widget" },
    { L"Start with Windows", L"Windows के साथ शुरू करें", L"Mit Windows starten", L"Iniciar con Windows" },
    { L"Check for updates when WinClock starts", L"शुरू होने पर अपडेट जाँचें", L"Beim Start nach Updates suchen", L"Buscar actualizaciones al iniciar" },
    { L"Check for updates now", L"अभी अपडेट जाँचें", L"Jetzt nach Updates suchen", L"Buscar actualizaciones ahora" },
    { L"Open settings file", L"सेटिंग फ़ाइल खोलें", L"Einstellungsdatei öffnen", L"Abrir archivo de configuración" },
    { L"Reload settings", L"सेटिंग्स दोबारा लोड करें", L"Einstellungen neu laden", L"Recargar configuración" },
    { L"About WinClock", L"WinClock के बारे में", L"Über WinClock", L"Acerca de WinClock" },
    { L"Help & updates", L"मदद और अपडेट", L"Hilfe & Updates", L"Ayuda y actualizaciones" },
    { L"Exit", L"बंद करें", L"Beenden", L"Salir" },
    // layouts and themes
    { L"Table", L"तालिका", L"Tabelle", L"Tabla" },
    { L"Analog faces", L"एनालॉग घड़ियाँ", L"Analoge Zifferblätter", L"Esferas analógicas" },
    { L"Timeline planner", L"टाइमलाइन योजनाकार", L"Zeitleisten-Planer", L"Planificador en línea de tiempo" },
    { L"Compact", L"छोटा", L"Kompakt", L"Compacto" },
    { L"Mini strip (one line)", L"मिनी पट्टी (एक लाइन)", L"Mini-Leiste (eine Zeile)", L"Mini barra (una línea)" },
    { L"Terminal (dark)", L"टर्मिनल (गहरा)", L"Terminal (dunkel)", L"Terminal (oscuro)" },
    { L"Paper (light)", L"पेपर (हल्का)", L"Papier (hell)", L"Papel (claro)" },
    { L"Auto (follow Windows light/dark)", L"अपने आप (Windows जैसा)", L"Automatisch (wie Windows)", L"Automático (como Windows)" },
    { L"Custom (your colours)", L"अपने रंग", L"Eigene Farben", L"Personalizado (tus colores)" },
    // new in 2.5
    { L"Resume", L"फिर शुरू करें", L"Fortsetzen", L"Reanudar" },
    { L"Pause", L"रोकें", L"Pause", L"Pausar" },
    { L"Skip to the break", L"ब्रेक पर जाएँ", L"Zur Pause springen", L"Saltar al descanso" },
    { L"Skip to focus", L"फ़ोकस पर जाएँ", L"Zum Fokus springen", L"Saltar al enfoque" },
    { L"Stop", L"रोकें", L"Stopp", L"Detener" },
    { L"Focus length", L"फ़ोकस की लंबाई", L"Fokusdauer", L"Duración del enfoque" },
    { L"Focus timer", L"फ़ोकस टाइमर", L"Fokus-Timer", L"Temporizador de enfoque" },
    { L"Start", L"शुरू", L"Start", L"Iniciar" },
    { L"Lap", L"लैप", L"Runde", L"Vuelta" },
    { L"Copy laps", L"लैप कॉपी करें", L"Runden kopieren", L"Copiar vueltas" },
    { L"Reset", L"रीसेट", L"Zurücksetzen", L"Reiniciar" },
    { L"Stopwatch", L"स्टॉपवॉच", L"Stoppuhr", L"Cronómetro" },
    { L"Leave-by reminder…", L"निकलने का रिमाइंडर…", L"Losgeh-Erinnerung…", L"Recordatorio de salida…" },
    { L"Day/night world map", L"दिन/रात विश्व मानचित्र", L"Tag/Nacht-Weltkarte", L"Mapa del día y la noche" },
    { L"Market hours", L"बाज़ार के घंटे", L"Börsenzeiten", L"Horario de bolsas" },
    { L"Show weather next to each city", L"हर शहर के पास मौसम", L"Wetter neben jeder Stadt", L"Tiempo junto a cada ciudad" },
    { L"Weather", L"मौसम", L"Wetter", L"Tiempo" },
    { L"Show prayer times in tooltips", L"टूलटिप में नमाज़ का समय", L"Gebetszeiten im Tooltip", L"Horarios de oración en la info" },
    { L"Prayer times", L"नमाज़ का समय", L"Gebetszeiten", L"Horarios de oración" },
    { L"Moon phase is always in the tooltips", L"चाँद की कला हमेशा टूलटिप में", L"Mondphase steht immer im Tooltip", L"La fase lunar siempre está en la info" },
    { L"Clear answer cards", L"उत्तर कार्ड हटाएँ", L"Antwortkarten entfernen", L"Quitar tarjetas de respuesta" },
    { L"Mini: one-line strip", L"मिनी: एक लाइन", L"Mini: eine Zeile", L"Mini: una línea" },
    { L"Small: compact list", L"छोटा: सूची", L"Klein: kompakte Liste", L"Pequeño: lista compacta" },
    { L"Medium: table", L"मध्यम: तालिका", L"Mittel: Tabelle", L"Mediano: tabla" },
    { L"Large: big table", L"बड़ा: बड़ी तालिका", L"Groß: große Tabelle", L"Grande: tabla grande" },
    { L"Size preset", L"आकार", L"Größe", L"Tamaño predefinido" },
    { L"Background colour…", L"पृष्ठभूमि रंग…", L"Hintergrundfarbe…", L"Color de fondo…" },
    { L"Text colour…", L"टेक्स्ट रंग…", L"Textfarbe…", L"Color del texto…" },
    { L"Accent colour…", L"मुख्य रंग…", L"Akzentfarbe…", L"Color de acento…" },
    { L"Second colour…", L"दूसरा रंग…", L"Zweitfarbe…", L"Segundo color…" },
    { L"Use my Windows accent colour", L"Windows का रंग लें", L"Windows-Akzentfarbe verwenden", L"Usar el color de Windows" },
    { L"Reset custom colours", L"अपने रंग रीसेट करें", L"Eigene Farben zurücksetzen", L"Restablecer colores" },
    { L"Custom theme colours", L"थीम के अपने रंग", L"Eigene Designfarben", L"Colores del tema personalizado" },
    { L"Language", L"भाषा", L"Sprache", L"Idioma" },
    { L"Copy this planned time as a message", L"यह समय संदेश के रूप में कॉपी करें", L"Geplante Zeit als Nachricht kopieren", L"Copiar esta hora como mensaje" },
    { L"Copy the current time as a message", L"अभी का समय संदेश के रूप में कॉपी करें", L"Aktuelle Zeit als Nachricht kopieren", L"Copiar la hora actual como mensaje" },
    { L"Copy my working hours (Slack / Teams status)", L"मेरे काम के घंटे कॉपी करें (Slack / Teams)", L"Meine Arbeitszeit kopieren (Slack / Teams)", L"Copiar mi horario (Slack / Teams)" },
    // widget words
    { L"world clocks", L"विश्व घड़ियाँ", L"weltuhren", L"relojes del mundo" },
    { L"analog", L"एनालॉग", L"analog", L"analógico" },
    { L"timeline", L"टाइमलाइन", L"zeitleiste", L"línea de tiempo" },
    { L"CITY / PERSON", L"शहर / व्यक्ति", L"STADT / PERSON", L"CIUDAD / PERSONA" },
    { L"TIME", L"समय", L"ZEIT", L"HORA" },
    { L"STATE", L"स्थिति", L"STATUS", L"ESTADO" },
    { L"OFFSET", L"अंतर", L"VERSATZ", L"DIFERENCIA" },
    { L"DAY", L"दिन", L"TAG", L"DÍA" },
    { L"Working", L"काम पर", L"Arbeitet", L"Trabajando" },
    { L"Free", L"खाली", L"Frei", L"Libre" },
    { L"Asleep", L"सो रहे", L"Schläft", L"Durmiendo" },
    { L"Holiday", L"छुट्टी", L"Feiertag", L"Festivo" },
    { L"Away", L"बाहर", L"Abwesend", L"Fuera" },
};
struct TrPrefix { const wchar_t* en; const wchar_t* hi; const wchar_t* de; const wchar_t* es; };
static const TrPrefix kTrPrefix[] = {
    { L"Profile: ", L"प्रोफ़ाइल: ", L"Profil: ", L"Perfil: " },
    { L"Focus timer (", L"फ़ोकस टाइमर (", L"Fokus-Timer (", L"Enfoque (" },
    { L"Stopwatch (", L"स्टॉपवॉच (", L"Stoppuhr (", L"Cronómetro (" },
    { L"Market hours (", L"बाज़ार के घंटे (", L"Börsenzeiten (", L"Bolsas (" },
    { L"Start focus (", L"फ़ोकस शुरू करें (", L"Fokus starten (", L"Empezar enfoque (" },
    { L"Start a break (", L"ब्रेक लें (", L"Pause starten (", L"Tomar un descanso (" },
    { L"Back to now (", L"अभी पर लौटें (", L"Zurück zu jetzt (", L"Volver a ahora (" },
    { L"⬆  Update to WinClock ", L"⬆  WinClock अपडेट करें ", L"⬆  Auf WinClock aktualisieren ", L"⬆  Actualizar a WinClock " },
};
static std::wstring NoAmpStr(const std::wstring& s) { std::wstring o; for (size_t i = 0; i < s.size(); i++) { if (s[i] == L'&') { if (i + 1 < s.size() && s[i + 1] == L'&') { o += L'&'; i++; } continue; } o += s[i]; } return o; }
static const wchar_t* Pick(const Tr& t) { return S.lang == 1 ? t.hi : S.lang == 2 ? t.de : t.es; }
// translate a UI string (exact match, then known prefixes); English or unknown text comes back as is
static std::wstring Tx(const std::wstring& en) {
    if (!S.lang) return en;
    std::wstring key = NoAmpStr(en);
    for (auto& t : kTr) if (key == t.en) return Pick(t);
    for (auto& p : kTrPrefix) { size_t n = wcslen(p.en); if (key.compare(0, n, p.en) == 0) return std::wstring(S.lang == 1 ? p.hi : S.lang == 2 ? p.de : p.es) + key.substr(n); }
    return en;
}
static const wchar_t* T(const wchar_t* en) {   // for fixed strings drawn on the widget
    if (!S.lang) return en;
    for (auto& t : kTr) if (!wcscmp(en, t.en)) return Pick(t);
    return en;
}
static void TranslateMenu(HMENU m) {
    int n = GetMenuItemCount(m);
    for (int i = 0; i < n; i++) {
        wchar_t buf[512]; MENUITEMINFOW mi{ sizeof(mi) }; mi.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_FTYPE; mi.dwTypeData = buf; mi.cch = 511;
        if (!GetMenuItemInfoW(m, i, TRUE, &mi)) continue;
        if (mi.hSubMenu) TranslateMenu(mi.hSubMenu);
        if (mi.fType & MFT_SEPARATOR || !mi.cch) continue;
        std::wstring s = buf, tab; size_t p = s.find(L'\t'); if (p != std::wstring::npos) { tab = s.substr(p); s = s.substr(0, p); }
        std::wstring lead; while (!s.empty() && s[0] == L' ') { lead += L' '; s.erase(0, 1); }
        std::wstring t = Tx(s);
        if (t == s) continue;
        std::wstring esc; for (wchar_t c : t) { if (c == L'&') esc += L"&&"; else esc += c; }
        esc = lead + esc + tab;
        MENUITEMINFOW si{ sizeof(si) }; si.fMask = MIIM_STRING; si.dwTypeData = (LPWSTR)esc.c_str();
        SetMenuItemInfoW(m, i, TRUE, &si);
    }
}
// day and month names on the widget follow the language
static void ApplyDayNames() {
    static const wchar_t* d[4][7] = {
        { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" },
        { L"रवि", L"सोम", L"मंगल", L"बुध", L"गुरु", L"शुक्र", L"शनि" },
        { L"So", L"Mo", L"Di", L"Mi", L"Do", L"Fr", L"Sa" },
        { L"dom", L"lun", L"mar", L"mié", L"jue", L"vie", L"sáb" } };
    static const wchar_t* mo[4][12] = {
        { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" },
        { L"जन", L"फ़र", L"मार्च", L"अप्रै", L"मई", L"जून", L"जुला", L"अग", L"सित", L"अक्टू", L"नव", L"दिस" },
        { L"Jan", L"Feb", L"Mär", L"Apr", L"Mai", L"Jun", L"Jul", L"Aug", L"Sep", L"Okt", L"Nov", L"Dez" },
        { L"ene", L"feb", L"mar", L"abr", L"may", L"jun", L"jul", L"ago", L"sep", L"oct", L"nov", L"dic" } };
    int l = std::clamp(S.lang, 0, 3);
    for (int i = 0; i < 7; i++) kDays[i] = d[l][i];
    for (int i = 0; i < 12; i++) kMonths[i] = mo[l][i];
}
static void ApplyLanguageFonts() { FreeFonts(); InitFonts(); ApplyDayNames(); }
static void SetLanguage(int lang) { S.lang = std::clamp(lang, 0, kLangCount - 1); SaveSettings(); ApplyLanguageFonts(); }

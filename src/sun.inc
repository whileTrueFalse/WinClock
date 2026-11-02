// ---------------------------------------------------------------- sunrise / sunset (NOAA solar calculator)
// Returns minutes after UTC midnight for the given civil date at (lat, lon). Accurate to about a minute.
// result: 0 = normal, 1 = sun up all day (polar day), 2 = sun down all day (polar night)
static int SunTimesUtc(int y, int m, int d, double lat, double lon, double& riseMin, double& setMin) {
    const double PI = 3.14159265358979323846, R = PI / 180;
    long long days = DaysOf(y, m, d);
    double jd = (double)days + 2305813.5 + 0.5;                 // Julian day at 12:00 UTC (1601-01-01 = JD 2305813.5)
    double t = (jd - 2451545.0) / 36525.0;
    double L0 = fmod(280.46646 + t * (36000.76983 + t * 0.0003032), 360.0);
    double M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    double C = sin(M * R) * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin(2 * M * R) * (0.019993 - 0.000101 * t) + sin(3 * M * R) * 0.000289;
    double trueLong = L0 + C;
    double omega = 125.04 - 1934.136 * t;
    double lambda = trueLong - 0.00569 - 0.00478 * sin(omega * R);
    double eps0 = 23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
    double eps = eps0 + 0.00256 * cos(omega * R);
    double decl = asin(sin(eps * R) * sin(lambda * R));
    double yv = tan(eps * R / 2); yv *= yv;
    double eqTime = 4 / R * (yv * sin(2 * L0 * R) - 2 * e * sin(M * R) + 4 * e * yv * sin(M * R) * cos(2 * L0 * R)
                             - 0.5 * yv * yv * sin(4 * L0 * R) - 1.25 * e * e * sin(2 * M * R));
    double cosH = (cos(90.833 * R) / (cos(lat * R) * cos(decl))) - tan(lat * R) * tan(decl);
    if (cosH < -1) return 1;
    if (cosH > 1) return 2;
    double ha = acos(cosH) / R;
    double noon = 720 - 4 * lon - eqTime;
    riseMin = noon - 4 * ha; setMin = noon + 4 * ha;
    return 0;
}
// sunrise/sunset as absolute UTC minutes for the zone-local date that contains localDay (days since 1601)
static int SunForLocalDay(const Clock& c, long long localDay, long long& rise, long long& set) {
    if (std::isnan(c.lat) || std::isnan(c.lon)) return -1;
    SYSTEMTIME st = DateOf(localDay);
    double r, s; int k = SunTimesUtc(st.wYear, st.wMonth, st.wDay, c.lat, c.lon, r, s);
    if (k) return k;
    long long base = localDay * 1440;   // UTC midnight of that civil date, in minutes since 1601
    rise = base + (long long)llround(r); set = base + (long long)llround(s);
    // the civil date is local, the NOAA times are UTC for that date: nudge into the right local day
    int off = ClockAt(c, (ULONGLONG)(base + 720) * TPM).offset;
    while (rise + off < base) { rise += 1440; set += 1440; }
    while (rise + off >= base + 1440) { rise -= 1440; set -= 1440; }
    return 0;
}
// is the sun up at absolute UTC minute u for this clock? returns -1 when unknown (no coordinates)
static int SunUpAt(const Clock& c, long long u) {
    if (std::isnan(c.lat)) return -1;
    int off = ClockAt(c, (ULONGLONG)u * TPM).offset;
    long long localDay = (u + off) / 1440;
    long long r, s; int k = SunForLocalDay(c, localDay, r, s);
    if (k == 1) return 1;
    if (k == 2) return 0;
    if (k < 0) return -1;
    return u >= r && u < s;
}

package com.kimjio.umamusumelocalify.settings;

import java.util.List;

public final class Constants {
    public static final String PREF_NAME = "pref_settings";

    public static final String PKG_JPN = "jp.co.cygames.umamusume";
    public static final String PKG_JPN_PCR_GM = "jp.co.cygames.priconnegrandmasters";
    public static final String PKG_KOR = "com.kakaogames.umamusume";
    public static final String PKG_KOR_PCR_GM = "com.kakaogames.pcrgm";
    public static final String PKG_ENG = "com.cygames.umamusume";
    public static final String PKG_TWN_GOOGLE = "com.komoe.kmumamusumegp";
    public static final String PKG_TWN_MY_CARD = "com.komoe.umamusumeofficial";

    public static final List<String> targetPackageNames = List.of(
            PKG_JPN,
            PKG_JPN_PCR_GM,
            PKG_KOR,
            PKG_KOR_PCR_GM,
            PKG_ENG,
            PKG_TWN_GOOGLE,
            PKG_TWN_MY_CARD
            // TODO Package by region
    );
}

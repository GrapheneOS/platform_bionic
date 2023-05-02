#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>

// keep in sync with android.ext.settings.{ExtSettings,GnssConstants}
#define PSDS_SERVER_GRAPHENEOS 0
#define PSDS_SERVER_STANDARD 1
#define PSDS_DISABLED 2
#define PSDS_SETTING_DEFAULT PSDS_SERVER_GRAPHENEOS

static int get_gnss_psds_setting() {
    char value[PROP_VALUE_MAX];
    if (__system_property_get("persist.sys.gnss_psds", value) == 0) {
        return PSDS_SETTING_DEFAULT;
    }
    // to avoid corner cases, don't convert to int directly
    if (strlen(value) == 1) {
        int v = (int) value[0] - '0';
        if (v >= PSDS_SERVER_GRAPHENEOS && v <= PSDS_DISABLED) {
            return v;
        }
    }

    return PSDS_SETTING_DEFAULT;
}

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "gnss_psds_setting.c"

#define XTRA_DAEMON_HOSTNAME_OVERRIDE "qualcomm.psds.grapheneos.org"
#define XTRA_DAEMON_DEFAULT_NTP_SERVER "time.grapheneos.org"
#define XTRA_DAEMON_QUALCOMM_NTP_SERVER "time.xtracloud.net"

static const char* xtra_hook_translate_hostname(const char* hostname) {
    const int psds_setting = get_gnss_psds_setting();

    if (psds_setting == PSDS_DISABLED) {
        // intentionally break DNS resolution
        return NULL;
    }

    if (strcmp(hostname, XTRA_DAEMON_DEFAULT_NTP_SERVER) == 0) {
        if (psds_setting == PSDS_SERVER_STANDARD) {
            return XTRA_DAEMON_QUALCOMM_NTP_SERVER;
        }
        return hostname;
    }

    if (psds_setting == PSDS_SERVER_GRAPHENEOS) {
        return XTRA_DAEMON_HOSTNAME_OVERRIDE;
    } else if (psds_setting == PSDS_SERVER_STANDARD) {
        return hostname;
    } else {
        // should be unreachable
        abort();
    }
}

// There are several xtra3*.bin file versions, xtra-daemon gets told by the modem which one to
// download. This likely reveals the (very rough) region the device is in to the server.
// To avoid this, hardcode the most complete file version.
#define XTRA_REQUEST_GET_HEADER "GET /xtra3Mgrbeji.bin HTTP/1.1\r\n"

static void* xtra_hook_override_ssl_write(const void* orig_buf, int orig_len, int* out_len) {
    const uint8_t* obuf = (const uint8_t*) orig_buf;
    const size_t obuf_len = (size_t) orig_len;

    const char user_agent[] = "\r\nUser-Agent:";
    if (memmem(obuf, obuf_len, user_agent, strlen(user_agent)) == NULL) {
        return NULL;
    }

    const int psds_setting = get_gnss_psds_setting();

    // specify the maximum possible size
    uint8_t* buf = calloc(obuf_len + strlen(XTRA_DAEMON_HOSTNAME_OVERRIDE) + strlen(XTRA_REQUEST_GET_HEADER), 1);
    size_t buf_off = 0;

    const char separator[] = "\r\n";
    const size_t separator_len = strlen(separator);

    size_t obuf_off = 0;
    while (obuf_off < obuf_len) {
        const uint8_t* line_start = obuf + obuf_off;
        const uint8_t* separator_start = memmem(line_start, obuf_len - obuf_off, separator, separator_len);
        if (separator_start == NULL) {
            break;
        }

        const uint8_t* line_end = separator_start + separator_len;

        const size_t line_len = line_end - line_start;
        obuf_off += line_len;

        const char get_start[] = "GET /xtra3";
        if (line_len > strlen(get_start)) {
            if (memcmp(line_start, get_start, strlen(get_start)) == 0) {
                const size_t l = strlen(XTRA_REQUEST_GET_HEADER);
                memcpy(buf + buf_off, XTRA_REQUEST_GET_HEADER, l);
                buf_off += l;
                continue;
            }
        }

        if (psds_setting == PSDS_SERVER_GRAPHENEOS) {
            const char host_start[] = "Host:";
            if (line_len > strlen(host_start)) {
                if (memcmp(line_start, host_start, strlen(host_start)) == 0) {
                    const char host_line[] = "Host: " XTRA_DAEMON_HOSTNAME_OVERRIDE "\r\n";
                    const size_t l = strlen(host_line);
                    memcpy(buf + buf_off, host_line, l);
                    buf_off += l;
                    continue;
                }
            }
        }

        const char user_agent_start[] = "User-Agent:";
        if (line_len > strlen(user_agent_start)) {
            if (memcmp(line_start, user_agent_start, strlen(user_agent_start)) == 0) {
                // omit the user agent, it contains persistent randomly generated identifier and
                // device model name, cell network operator name, etc
                continue;
            }
        }

        memcpy(buf + buf_off, line_start, line_len);
        buf_off += line_len;
    }

    if (buf_off > INT_MAX) {
        abort();
    }

    *out_len = (int) buf_off;
    return buf;
}

// base64-encoded SHA-256 sums of public keys of trusted root certificates for
// qualcomm.psds.grapheneos.org
static const char* XTRA_EXTRA_TRUSTED_CERTS[] = {
        // ISRG Root X1
        "C5+lpZ7tcVwmwQIMcRtPbsQtWLABXhQzejna0wHFr8M=",
        // ISRG Root X2
        "diGVwiVYbubAI3RW4hB9xU8e/CH2GnkuvVFZE8zmgzI=",
        NULL
};

static const char* const* xtra_hook_get_trusted_ssl_certificates() {
    const int psds_setting = get_gnss_psds_setting();
    if (psds_setting != PSDS_SERVER_GRAPHENEOS) {
        return NULL;
    }

    return XTRA_EXTRA_TRUSTED_CERTS;
}

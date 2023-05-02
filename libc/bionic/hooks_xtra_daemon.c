#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "gnss_psds_setting.c"

#define XTRA_DAEMON_HOSTNAME_OVERRIDE "qualcomm.psds.grapheneos.org"
#define XTRA_DAEMON_ALLOWED_HOSTNAME "time.xtracloud.net"

static const char* xtra_hook_translate_hostname(const char* hostname) {
    const int psds_setting = get_gnss_psds_setting();

    if (psds_setting == PSDS_DISABLED) {
        // intentionally break DNS resolution
        return NULL;
    }

    if (strcmp(hostname, XTRA_DAEMON_ALLOWED_HOSTNAME) == 0) {
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
                    if (memmem(line_start, line_len, XTRA_DAEMON_ALLOWED_HOSTNAME,
                               strlen(XTRA_DAEMON_ALLOWED_HOSTNAME)) == NULL)
                    {
                        const char host_line[] = "Host: " XTRA_DAEMON_HOSTNAME_OVERRIDE "\r\n";
                        const size_t l = strlen(host_line);
                        memcpy(buf + buf_off, host_line, l);
                        buf_off += l;
                        continue;
                    }
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

static const char* xtra_hook_get_trusted_ssl_certificate() {
    const int psds_setting = get_gnss_psds_setting();
    if (psds_setting != PSDS_SERVER_GRAPHENEOS) {
        return NULL;
    }

    // Root certificate of qualcomm.psds.grapheneos.org
    // (from Let's Encrypt, cross-signed by DST Root CA X3)
    //  s:C = US, O = Internet Security Research Group, CN = ISRG Root X1
    //  i:O = Digital Signature Trust Co., CN = DST Root CA X3
    return
"-----BEGIN CERTIFICATE-----\n"
"MIIFYDCCBEigAwIBAgIQQAF3ITfU6UK47naqPGQKtzANBgkqhkiG9w0BAQsFADA/\n"
"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"
"DkRTVCBSb290IENBIFgzMB4XDTIxMDEyMDE5MTQwM1oXDTI0MDkzMDE4MTQwM1ow\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwggIiMA0GCSqGSIb3DQEB\n"
"AQUAA4ICDwAwggIKAoICAQCt6CRz9BQ385ueK1coHIe+3LffOJCMbjzmV6B493XC\n"
"ov71am72AE8o295ohmxEk7axY/0UEmu/H9LqMZshftEzPLpI9d1537O4/xLxIZpL\n"
"wYqGcWlKZmZsj348cL+tKSIG8+TA5oCu4kuPt5l+lAOf00eXfJlII1PoOK5PCm+D\n"
"LtFJV4yAdLbaL9A4jXsDcCEbdfIwPPqPrt3aY6vrFk/CjhFLfs8L6P+1dy70sntK\n"
"4EwSJQxwjQMpoOFTJOwT2e4ZvxCzSow/iaNhUd6shweU9GNx7C7ib1uYgeGJXDR5\n"
"bHbvO5BieebbpJovJsXQEOEO3tkQjhb7t/eo98flAgeYjzYIlefiN5YNNnWe+w5y\n"
"sR2bvAP5SQXYgd0FtCrWQemsAXaVCg/Y39W9Eh81LygXbNKYwagJZHduRze6zqxZ\n"
"Xmidf3LWicUGQSk+WT7dJvUkyRGnWqNMQB9GoZm1pzpRboY7nn1ypxIFeFntPlF4\n"
"FQsDj43QLwWyPntKHEtzBRL8xurgUBN8Q5N0s8p0544fAQjQMNRbcTa0B7rBMDBc\n"
"SLeCO5imfWCKoqMpgsy6vYMEG6KDA0Gh1gXxG8K28Kh8hjtGqEgqiNx2mna/H2ql\n"
"PRmP6zjzZN7IKw0KKP/32+IVQtQi0Cdd4Xn+GOdwiK1O5tmLOsbdJ1Fu/7xk9TND\n"
"TwIDAQABo4IBRjCCAUIwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYw\n"
"SwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5pZGVudHJ1\n"
"c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTEp7Gkeyxx\n"
"+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEEAYLfEwEB\n"
"ATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2VuY3J5cHQu\n"
"b3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0LmNvbS9E\n"
"U1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFHm0WeZ7tuXkAXOACIjIGlj26Ztu\n"
"MA0GCSqGSIb3DQEBCwUAA4IBAQAKcwBslm7/DlLQrt2M51oGrS+o44+/yQoDFVDC\n"
"5WxCu2+b9LRPwkSICHXM6webFGJueN7sJ7o5XPWioW5WlHAQU7G75K/QosMrAdSW\n"
"9MUgNTP52GE24HGNtLi1qoJFlcDyqSMo59ahy2cI2qBDLKobkx/J3vWraV0T9VuG\n"
"WCLKTVXkcGdtwlfFRjlBz4pYg1htmf5X6DYO8A4jqv2Il9DjXA6USbW1FzXSLr9O\n"
"he8Y4IWS6wY7bCkjCWDcRQJMEhg76fsO3txE+FiYruq9RUWhiF1myv4Q6W+CyBFC\n"
"Dfvp7OOGAN6dEOM4+qR9sdjoSYKEBpsr6GtPAQw4dy753ec5\n"
"-----END CERTIFICATE-----\n";
}

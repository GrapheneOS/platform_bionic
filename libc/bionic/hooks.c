#include <stdlib.h>

#include "hooks_xtra_daemon.c"

// android_getaddrinfofornetcontext()
// SSL_set_tlsext_host_name()
const char* hook_translate_hostname(const char* hostname) {
    if (hostname == NULL) {
        return NULL;
    }

    switch (get_prog_id()) {
        case PROG_XTRA_DAEMON:
            return xtra_hook_translate_hostname(hostname);
        default:
            return hostname;
    }
}

// X509_verify_cert()
const char* hook_get_trusted_ssl_certificate() {
    switch (get_prog_id()) {
        case PROG_XTRA_DAEMON:
            return xtra_hook_get_trusted_ssl_certificate();
        default:
            return NULL;
    }
}

// SSL_write()
void* hook_override_ssl_write(const void* orig_buf, int orig_len, int* out_len) {
    if (orig_len <= 0) {
        return NULL;
    }

    switch (get_prog_id()) {
        case PROG_XTRA_DAEMON:
            return xtra_hook_override_ssl_write(orig_buf, orig_len, out_len);
        default:
            return NULL;
    }
}

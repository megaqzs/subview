/* subspa.h */
#ifndef SUBSPA_INCLUDED
#define SUBSPA_INCLUDED

#define SUBSPA_VERSION "0.1"
#define SUBSPA_VERSION_MAJOR 0
#define SUBSPA_VERSION_MINOR 1

#ifdef __cplusplus
extern "C" {
#endif

// set SUBSPA_NETWORK in order to transfer messages across different computers
#ifdef SUBTRANSFER_NETWORK
#   include <sys/socket.h>
#   include <arpa/inet.h>
#endif
#include <stdint.h>


#ifndef SUBSPA_STRUCT_ATTRIBUTES
#   ifdef SUBSPA_NETWORK
        // GCC packing
#       define SUBSPA_STRUCT_ATTRIBUTES __attribute__((packed))
#   else 
        // no packing
#       define SUBSPA_STRUCT_ATTRIBUTES
#   endif
#endif

enum subspa_type_t {
    SUBSPA_GET_IMPL = -5, // get the details of the server's implementation of the protocol, all fields are ignored
    SUBSPA_CLEAR_CACHE = -4, // clears token cache from timestamp
    SUBSPA_PAUSE = -3, // pauses token lookup
    SUBSPA_PLAY = -2, // continues token lookup
    SUBSPA_SYNC_TIMESTAMPS = -1, // adds value to all timestamps that are currently cached

    // speaker specific tokens, the number of speakers that are allowed is implementation dependent,
    // non implemented speakers will fall back to default which is always zero
    SUBSPA_DEFAULT_SPKR_TOKEN = 0,
    SUBSPA_SPKR0_TOKEN = 0,
    SUBSPA_SPKR1_TOKEN,
    SUBSPA_SPKR2_TOKEN,
    SUBSPA_SPKR3_TOKEN,
    SUBSPA_SPKR4_TOKEN,
    SUBSPA_SPKR5_TOKEN,
    SUBSPA_SPKR6_TOKEN,
    SUBSPA_SPKR7_TOKEN,
    SUBSPA_SPKR8_TOKEN,
    SUBSPA_SPKR9_TOKEN,

};

enum subspa_impl_mthds_t {
    SUBSPA_TOKEN_NEGATIVE_DELAY_IGNORED = 1,
    SUBSPA_TOKEN_NEGATIVE_DELAY_RENDERED = 2,
    SUBSPA_SYNC_NEGATIVE_DELAY_IGNORED = 4,
    SUBSPA_SYNC_NEGATIVE_DELAY_PERFORMED = 8,
    SUBSPA_CLEAR_CACHE_RENDERED = 16
}

typedef struct SUBSPA_STRUCT_ATTRIBUTES {
    uint32_t pid; // unique program id
    uint32_t vid; // unique version id
    char name[16]; // program name
    uint32_t max_spkr; // maximum speaker number
    uint32_t impl_mthds; // supported operation information
} subspa_impl_t;

// the header used by all messages to the subtitle rendering server
typedef struct SUBSPA_STRUCT_ATTRIBUTES {
    int32_t type; // the type of this message
    uint8_t likelyhood; // the probability of this token being correct, from 0 to UINT32_MAX
    int32_t timestamp; // the number of milliseconds from this point, negative delay is implementation specific
    uint32_t buf_len; // the length of this token buffer
    char buf[]; // the token buffer
} subspa_msg_t;

#ifdef SUBTRANSFER_NETWORK
    inline void subspa_pack_header(subspa_msg_t *header) {
        header->type = htonl(header->type);
        header->likelyhood = htonl(header->likelyhood);
        header->timestamp = htonl(header->timestamp);
        header->buf_len = htonl(header->buf_len);
    }
    inline void subspa_pack_header(subspa_msg_t *header) {
        header->type = ntohl(header->type);
        header->likelyhood = ntohl(header->likelyhood);
        header->timestamp = ntohl(header->timestamp);
        header->buf_len = ntohl(header->buf_len);
    }
#else
#   define subspa_pack_header(X) (X)
#   define subspa_unpack_header(X) (X)
#endif
#ifdef __cplusplus
}
#endif

#endif /* SUBSPA_INCLUDED */

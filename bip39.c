#include "bip39.h"
#include "mnemonic.h"
#include "wordlist.h"
#include <ccan/crypto/sha256/sha256.h>
#include <string.h>

#include "data/wordlists/chinese_simplified.c"
#include "data/wordlists/chinese_traditional.c"
#include "data/wordlists/english.c"
#include "data/wordlists/french.c"
#include "data/wordlists/italian.c"
#include "data/wordlists/spanish.c"
#include "data/wordlists/japanese.c"


static const struct {
    const char name[4];
    const struct words* words;
} lookup[] = {
    { "en", &en_words}, { "es", &es_words}, { "fr", &fr_words},
    { "it", &it_words}, { "jp", &jp_words}, { "zhs", &zhs_words},
    { "zht", &zht_words},
    /* FIXME: Should 'zh' map to traditional or simplified? */
};

const char *bip39_get_languages()
{
    return "en es fr it jp zhs zht";
}

const struct words *bip39_get_wordlist(const char* lang)
{
    if (lang) {
        size_t i;
        for (i = 0; i < sizeof(lookup) / sizeof(lookup[0]); ++i)
            if (!strcmp(lang, lookup[i].name))
                return lookup[i].words;
    }
    return &en_words; /* Fallback to English if not found */
}

/* Convert an input entropy length to a mask for checksum bits. As it
 * returns 0 for bad lengths, it serves as a validation function too.
 */
static size_t entropy_len_to_mask(size_t len)
{
    switch (len) {
    case BIP39_ENTROPY_LEN_128: return 0xf0;
    case BIP39_ENTROPY_LEN_160: return 0xf8;
    case BIP39_ENTROPY_LEN_192: return 0xfc;
    case BIP39_ENTROPY_LEN_224: return 0xfe;
    case BIP39_ENTROPY_LEN_256: return 0xff;
    }
    return 0;
}

char* bip39_mnemonic_from_bytes(const struct words *w, const uint8_t *bytes, size_t len)
{
    /* 128 to 256 bits of entropy require 4-8 bits of checksum */
    uint8_t checksummed_bytes[BIP39_ENTROPY_LEN_256 + sizeof(uint8_t)];
    uint8_t checksum;

    w = w ? w : &en_words;

    if (w->bits != 11u || !entropy_len_to_mask(len))
        return NULL;
    else {
        struct sha256 tmp;
        sha256(&tmp, bytes, len); /* FIXME: Allow user to provide a SHA256 impl */
        checksum = tmp.u.u8[0];
    }

    memcpy(checksummed_bytes, bytes, len);
    checksummed_bytes[len] = checksum;
    return mnemonic_from_bytes(w, checksummed_bytes, len + 1);
}

size_t bip39_mnemonic_to_bytes(const struct words *w, const char *mnemonic,
                               uint8_t *bytes, size_t len)
{
    uint8_t tmp_bytes[BIP39_ENTROPY_LEN_256 + sizeof(uint8_t)];
    size_t mask, tmp_len;
    uint8_t checksum;

    /* Ideally we would infer the wordlist here. Unfortunately this cannot
     * work reliably because the default word lists overlap. In combination
     * with being sorted lexographically, this means the default lists
     * were poorly chosen. But we are stuck with them now.
     *
     * If the caller doesn't know which word list to use, they should iterate
     * over the available ones and try any resulting list that the mnemonic
     * validates against.
     */
    w = w ? w : &en_words;

    if (w->bits != 11u)
        return false;

    tmp_len = mnemonic_to_bytes(w, mnemonic, tmp_bytes, sizeof(tmp_bytes));

    if (!tmp_len || !(mask = entropy_len_to_mask(tmp_len - 1)))
        return false;
    else {
        struct sha256 tmp;

        if (len < tmp_len - 1)
            return 0; /* Callers buffer is too small */

        sha256(&tmp, tmp_bytes, tmp_len - 1); /* FIXME: Allow user to provide a SHA256 impl */
        checksum = tmp.u.u8[0];
    }

    if ((tmp_bytes[tmp_len - 1] & mask) != (checksum & mask))
        return 0; /* Mismatched checksum */

    memcpy(bytes, tmp_bytes, tmp_len - 1);
    return tmp_len - 1;
}

bool bip39_mnemonic_is_valid(const struct words *w, const char *mnemonic)
{
    uint8_t bytes[BIP39_ENTROPY_LEN_256 + sizeof(uint8_t)];
    return bip39_mnemonic_to_bytes(w, mnemonic, bytes, sizeof(bytes)) != 0;
}

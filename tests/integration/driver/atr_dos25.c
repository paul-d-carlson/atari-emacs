/*
 * atr_dos25.c -- see atr_dos25.h for the format reference this implements.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "atr_dos25.h"

#define ATR_HEADER_SIZE   16
#define ATR_MAGIC         0x0296u
#define SECTOR_SIZE       128
#define SECTOR_LINK_BYTES 3
#define SECTOR_DATA_BYTES (SECTOR_SIZE - SECTOR_LINK_BYTES) /* 125 */

#define VTOC_SECTOR        360
#define DIR_SECTOR_FIRST   361
#define DIR_SECTOR_COUNT   8
#define DIR_ENTRIES_PER_SECTOR 8
#define DIR_ENTRY_SIZE     16

#define DIRFLAG_DELETED  0x80u
#define DIRFLAG_INUSE    0x40u

/* Whole-disk image, loaded once into memory for easy random-access reads. */
typedef struct {
    unsigned char *data;
    size_t         size;
    int            sector_size;
} atr_image_t;

static int atr_load(const char *path, atr_image_t *img)
{
    FILE *fp;
    long fsize;
    unsigned magic, secsize;

    memset(img, 0, sizeof(*img));

    fp = fopen(path, "rb");
    if (fp == NULL)
        return -1;

    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
    fsize = ftell(fp);
    if (fsize < ATR_HEADER_SIZE) { fclose(fp); return -1; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return -1; }

    img->data = malloc((size_t)fsize);
    if (img->data == NULL) { fclose(fp); return -1; }

    if (fread(img->data, 1, (size_t)fsize, fp) != (size_t)fsize) {
        fclose(fp);
        free(img->data);
        img->data = NULL;
        return -1;
    }
    fclose(fp);
    img->size = (size_t)fsize;

    magic   = (unsigned)img->data[0] | ((unsigned)img->data[1] << 8);
    secsize = (unsigned)img->data[4] | ((unsigned)img->data[5] << 8);
    if (magic != ATR_MAGIC || secsize != SECTOR_SIZE) {
        free(img->data);
        img->data = NULL;
        return -1;
    }
    img->sector_size = SECTOR_SIZE;
    return 0;
}

static void atr_free(atr_image_t *img)
{
    free(img->data);
    img->data = NULL;
}

/* Sector N (1-indexed) -> pointer to its first byte, or NULL if out of
 * range of the loaded image. */
static const unsigned char *atr_sector(const atr_image_t *img, unsigned sector)
{
    size_t off = (size_t)ATR_HEADER_SIZE + (size_t)(sector - 1) * SECTOR_SIZE;
    if (sector < 1 || off + SECTOR_SIZE > img->size)
        return NULL;
    return img->data + off;
}

/* Splits "POEM.TXT" (optionally "D:POEM.TXT") into an 11-byte, space
 * padded, upper-cased 8+3 name as stored in a directory entry. */
static void make_dos_name(const char *dos_name, char out[11])
{
    const char *p = dos_name;
    const char *colon = strchr(p, ':');
    const char *dot;
    size_t namelen, extlen;
    int i;

    if (colon != NULL)
        p = colon + 1;

    dot = strchr(p, '.');
    namelen = (dot != NULL) ? (size_t)(dot - p) : strlen(p);
    if (namelen > 8)
        namelen = 8;
    extlen = (dot != NULL) ? strlen(dot + 1) : 0;
    if (extlen > 3)
        extlen = 3;

    memset(out, ' ', 11);
    for (i = 0; i < (int)namelen; i++)
        out[i] = (char)toupper((unsigned char)p[i]);
    if (dot != NULL)
        for (i = 0; i < (int)extlen; i++)
            out[8 + i] = (char)toupper((unsigned char)dot[1 + i]);
}

int atr_dos25_read_file(const char *atr_path, const char *dos_name,
                         unsigned char **out_data, size_t *out_len)
{
    atr_image_t img;
    char want[11];
    unsigned dsec;
    int start_sector = -1;
    unsigned char *buf = NULL;
    size_t buf_len = 0, buf_cap = 0;
    unsigned sector;
    int rc = -1;

    if (atr_load(atr_path, &img) != 0)
        return -1;

    make_dos_name(dos_name, want);

    /* --- find the directory entry --- */
    for (dsec = DIR_SECTOR_FIRST; dsec < DIR_SECTOR_FIRST + DIR_SECTOR_COUNT && start_sector < 0; dsec++) {
        const unsigned char *sec = atr_sector(&img, dsec);
        int e;
        if (sec == NULL)
            goto done;
        for (e = 0; e < DIR_ENTRIES_PER_SECTOR; e++) {
            const unsigned char *ent = sec + e * DIR_ENTRY_SIZE;
            unsigned char flag = ent[0];
            if (flag == 0x00) /* never-used slot */
                continue;
            if ((flag & DIRFLAG_DELETED) != 0 || (flag & DIRFLAG_INUSE) == 0)
                continue;
            if (memcmp(ent + 5, want, 11) == 0) {
                start_sector = (int)ent[3] | ((int)ent[4] << 8);
                break;
            }
        }
    }
    if (start_sector < 0)
        goto done; /* file not found */

    /* --- follow the sector chain --- */
    sector = (unsigned)start_sector;
    while (sector != 0) {
        const unsigned char *sec = atr_sector(&img, sector);
        unsigned next;
        int count;

        if (sec == NULL)
            goto done; /* corrupt chain: points off the end of the image */

        next = ((unsigned)(sec[125] & 0x03) << 8) | sec[126];
        count = (next == 0) ? (sec[127] & 0x7F) : SECTOR_DATA_BYTES;
        if (count < 0 || count > SECTOR_DATA_BYTES)
            goto done;

        if (buf_len + (size_t)count > buf_cap) {
            size_t newcap = buf_cap ? buf_cap * 2 : 512;
            unsigned char *nb;
            while (newcap < buf_len + (size_t)count)
                newcap *= 2;
            nb = realloc(buf, newcap);
            if (nb == NULL)
                goto done;
            buf = nb;
            buf_cap = newcap;
        }
        memcpy(buf + buf_len, sec, (size_t)count);
        buf_len += (size_t)count;

        sector = next;
    }

    *out_data = buf;
    *out_len = buf_len;
    buf = NULL; /* ownership transferred to caller */
    rc = 0;

done:
    free(buf);
    atr_free(&img);
    return rc;
}

int atr_dos25_free_sectors(const char *atr_path, unsigned *out_free)
{
    atr_image_t img;
    const unsigned char *vtoc;

    if (atr_load(atr_path, &img) != 0)
        return -1;

    vtoc = atr_sector(&img, VTOC_SECTOR);
    if (vtoc == NULL) {
        atr_free(&img);
        return -1;
    }
    *out_free = (unsigned)vtoc[3] | ((unsigned)vtoc[4] << 8);
    atr_free(&img);
    return 0;
}

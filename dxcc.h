#ifndef DXCC_H
#define DXCC_H

#include "qsolog.h"

/**
 * @file dxcc.h
 * @brief DXCC-databashantering, prefixuppslag och landstillhörighet.
 */

/* Läs in DXCC-prefixdatabas från fil (t.ex. dxcc.txt) */
void load_dxcc_file(const char *filename);

/* Frigör allokerat minne för DXCC-databasen */
void free_dxcc_db(void);

/* Slå upp DXCC-land för en given anropssignal */
void lookup_dxcc(const char *call_in, char *out, size_t out_sz);

#endif /* DXCC_H */

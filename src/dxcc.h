/**
 * @file dxcc.h
 * @brief DXCC-databashantering, prefixuppslag och landstillhörighet.
 */

#ifndef DXCC_H
#define DXCC_H

#include "qsolog.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Läser in DXCC-prefixdatabas från textfil (t.ex. dxcc.txt).
 * @param filename Sökväg till DXCC-filen.
 */
void load_dxcc_file(const char *filename);

/**
 * @brief Frigör allokerat minne för DXCC-databasen.
 */
void free_dxcc_db(void);

/**
 * @brief Slår upp DXCC-land för en given anropssignal.
 * @param call_in Inkommande signal (t.ex. "SM5UEI" eller "LA/SM5UEI/P").
 * @param out Buffer för det identifierade DXCC-landets namn.
 * @param out_sz Storlek på målbuffer.
 */
void lookup_dxcc(const char *call_in, char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* DXCC_H */

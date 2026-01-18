/*
 *   Copyright (c) 2026 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#ifdef DEBUG_DEFAULT_DESTRUCTOR
#define DEFAULT_DESTRUCTOR                                                                                             \
    {                                                                                                                  \
    }
#else
#define DEFAULT_DESTRUCTOR = default
#endif

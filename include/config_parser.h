#ifndef DOTMGR_CONFIG_PARSER_H
#define DOTMGR_CONFIG_PARSER_H

#include "dotmgr.h"

bool load_config(const AppOptions *opts, DotfileConfig *config);
void free_config(DotfileConfig *config);

#endif

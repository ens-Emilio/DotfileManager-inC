#ifndef DOTMGR_SYMLINK_ENGINE_H
#define DOTMGR_SYMLINK_ENGINE_H

#include "dotmgr.h"

bool install_entry(const AppOptions *opts, const DotfileEntry *entry);
bool uninstall_entry(const AppOptions *opts, const DotfileEntry *entry);
bool status_entry(const AppOptions *opts, const DotfileEntry *entry);

#endif

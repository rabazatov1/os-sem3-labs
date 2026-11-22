#pragma once

#include "contract.h"

typedef int (*switch_fn)(math_api *api, void *context);

int run_cli(math_api *api, switch_fn switch_implementation, void *context);

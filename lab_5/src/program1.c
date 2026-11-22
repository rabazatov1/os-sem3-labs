#include <stddef.h>

#include "cli.h"

int main(void)
{
    math_api api = {SinIntegral, Derivative, 1};
    return run_cli(&api, NULL, NULL);
}

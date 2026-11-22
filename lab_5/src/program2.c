#include <dlfcn.h>
#include <stdio.h>

#include "cli.h"

typedef struct {
    void *handle;
    int implementation;
} library_state;

static int load_library(math_api *api, library_state *state, int implementation)
{
    const char *path = implementation == 1
        ? "./build/libimpl1.so" : "./build/libimpl2.so";
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return -1;
    }

    dlerror();
    sin_integral_fn integral = (sin_integral_fn)dlsym(handle, "SinIntegral");
    const char *error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "dlsym: %s\n", error);
        dlclose(handle);
        return -1;
    }

    derivative_fn derivative = (derivative_fn)dlsym(handle, "Derivative");
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "dlsym: %s\n", error);
        dlclose(handle);
        return -1;
    }

    if (state->handle != NULL)
        dlclose(state->handle);
    state->handle = handle;
    state->implementation = implementation;
    *api = (math_api){integral, derivative, implementation};
    return 0;
}

static int switch_library(math_api *api, void *context)
{
    library_state *state = context;
    int next = state->implementation == 1 ? 2 : 1;
    return load_library(api, state, next);
}

int main(void)
{
    math_api api;
    library_state state = {NULL, 0};
    if (load_library(&api, &state, 1) != 0)
        return 1;

    int result = run_cli(&api, switch_library, &state);
    if (dlclose(state.handle) != 0) {
        fprintf(stderr, "dlclose: %s\n", dlerror());
        return 1;
    }
    return result;
}

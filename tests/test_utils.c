#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <stdlib.h>
#else
#include <stdlib.h>
#endif

#include "utils.h"

static void test_join_paths(void) {
    char buffer[PATH_MAX];
    assert(join_paths("/home/user", "file", buffer, sizeof(buffer)));
#ifdef _WIN32
    assert(strstr(buffer, "home") != NULL);
#else
    assert(strcmp(buffer, "/home/user/file") == 0);
#endif

    assert(join_paths("/home/user", "sub/dir", buffer, sizeof(buffer)));
    assert(strstr(buffer, "sub") != NULL);
}

static void test_is_absolute(void) {
#ifdef _WIN32
    assert(is_absolute_path("C:/temp"));
    assert(!is_absolute_path("temp\\file"));
#else
    assert(is_absolute_path("/tmp"));
    assert(!is_absolute_path("tmp/file"));
#endif
}

static void test_expand_home(void) {
    char buffer[PATH_MAX];
#ifdef _WIN32
    _putenv("HOME=C:/dotmgr-test");
    assert(expand_home("~\\AppData", buffer, sizeof(buffer)));
    assert(strstr(buffer, "AppData") != NULL);
#else
    setenv("HOME", "/dotmgr-test", 1);
    assert(expand_home("~/.config", buffer, sizeof(buffer)));
    assert(strstr(buffer, ".config") != NULL);
#endif
}

int main(void) {
    test_join_paths();
    test_is_absolute();
    test_expand_home();
    printf("All utils tests passed.\n");
    return 0;
}

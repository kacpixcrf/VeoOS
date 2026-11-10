#include "kernel/core/user.h"

const char* user_get_current_name(void) {
    return VEOOS_USER_NAME;
}

const char* user_get_home_directory(void) {
    return VEOOS_HOME_DIR;
}

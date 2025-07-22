#include "managers/content_lock.h"
#include "managers/nvs_manager.h"

bool content_lock_is_unlocked(void) {
    return nvs_manager_get_unlocked();
}

esp_err_t content_lock_toggle(void) {
    bool current_state = nvs_manager_get_unlocked();
    return nvs_manager_set_unlocked(!current_state);
}

esp_err_t content_lock_set_state(bool unlocked) {
    return nvs_manager_set_unlocked(unlocked);
}
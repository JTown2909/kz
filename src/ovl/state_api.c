/* the state overlay's api table, see ovl.h */
#include "ovl.h"

const struct ovl_state_api ovl_api OVL_API = {
    save_state_alloc,   load_state,     state_is_valid,
};

#include "drivers/input/uefi_input.h"
#include "drivers/input/input.h"
#include "drivers/video/framebuffer.h"

typedef struct {
    uint16_t ScanCode;
    uint16_t UnicodeChar;
} efi_input_key_t;

typedef struct {
    uint32_t ShiftState;
    uint8_t ToggleState;
} efi_key_state_t;

typedef struct {
    efi_input_key_t Key;
    efi_key_state_t KeyState;
} efi_key_data_t;

struct efi_simple_text_input_protocol;

typedef uint64_t (__attribute__((ms_abi)) *efi_input_read_key_t)(
    struct efi_simple_text_input_protocol *This,
    efi_input_key_t *Key
);

typedef struct efi_simple_text_input_protocol {
    void *Reset;
    efi_input_read_key_t ReadKeyStroke;
    void *WaitForKey;
} efi_simple_text_input_protocol_t;

struct efi_simple_text_input_ex_protocol;

typedef uint64_t (__attribute__((ms_abi)) *efi_input_read_key_ex_t)(
    struct efi_simple_text_input_ex_protocol *This,
    efi_key_data_t *KeyData
);

typedef struct efi_simple_text_input_ex_protocol {
    void *Reset;
    efi_input_read_key_ex_t ReadKeyStrokeEx;
    void *WaitForKeyEx;
    void *SetState;
    void *RegisterKeyNotify;
    void *UnregisterKeyNotify;
} efi_simple_text_input_ex_protocol_t;

static efi_simple_text_input_protocol_t *con_in = NULL;
static efi_simple_text_input_ex_protocol_t *con_in_ex = NULL;
static char pending_char = 0;
static bool uefi_ctrl_down = false;
static bool uefi_shift_down = false;

void uefi_input_initialize(void) {
    con_in_ex = (efi_simple_text_input_ex_protocol_t*)(*(uint64_t*)0x100018);
    con_in = (efi_simple_text_input_protocol_t*)(*(uint64_t*)0x100020);
    pending_char = 0;
    uefi_ctrl_down = false;
    uefi_shift_down = false;
}

bool uefi_input_is_available(void) {
    return (con_in_ex != NULL || con_in != NULL);
}

static char uefi_input_poll(void) {
    efi_input_key_t key;
    key.ScanCode = 0;
    key.UnicodeChar = 0;
    bool has_shift = uefi_shift_down;
    bool has_ctrl = uefi_ctrl_down;

    if (con_in_ex != NULL && con_in_ex->ReadKeyStrokeEx != NULL) {
        efi_key_data_t data;
        data.Key.ScanCode = 0;
        data.Key.UnicodeChar = 0;
        data.KeyState.ShiftState = 0;
        data.KeyState.ToggleState = 0;

        uint64_t st = con_in_ex->ReadKeyStrokeEx(con_in_ex, &data);
        if (st == 0) {
            key = data.Key;
            uint32_t ss = data.KeyState.ShiftState;
            if ((ss & 0x80000000) || (ss & 0x000003FF)) {
                uefi_shift_down = (ss & 0x03) != 0;
                uefi_ctrl_down = (ss & 0x0C) != 0;
                has_shift = uefi_shift_down;
                has_ctrl = uefi_ctrl_down;
            } else if (ss == 0) {
                uefi_shift_down = false;
                uefi_ctrl_down = false;
                has_shift = false;
                has_ctrl = false;
            }
        } else {
            return 0;
        }
    } else if (con_in != NULL && con_in->ReadKeyStroke != NULL) {
        uint64_t st = con_in->ReadKeyStroke(con_in, &key);
        if (st != 0) {
            return 0;
        }
    } else {
        return 0;
    }

    if (key.ScanCode != 0) {
        if (has_ctrl || has_shift) {
            if (key.ScanCode == 0x01) {
                framebuffer_scroll_view_lines(-2);
                return 0;
            }
            if (key.ScanCode == 0x02) {
                framebuffer_scroll_view_lines(2);
                return 0;
            }
            if (key.ScanCode == 0x09) {
                framebuffer_scroll_view_page(-1);
                return 0;
            }
            if (key.ScanCode == 0x0A) {
                framebuffer_scroll_view_page(1);
                return 0;
            }
            if (key.ScanCode == 0x05) {
                framebuffer_scroll_view_top();
                return 0;
            }
            if (key.ScanCode == 0x06) {
                framebuffer_scroll_view_bottom();
                return 0;
            }
        }

        if (key.ScanCode == 0x01) {
            return (char)INPUT_KEY_UP;
        }
        if (key.ScanCode == 0x02) {
            return (char)INPUT_KEY_DOWN;
        }
        if (key.ScanCode == 0x03) {
            return (char)INPUT_KEY_RIGHT;
        }
        if (key.ScanCode == 0x04) {
            return (char)INPUT_KEY_LEFT;
        }
        if (key.ScanCode == 0x08) {
            return '\b';
        }
        if (key.ScanCode == 0x09) {
            return (char)INPUT_KEY_PAGE_UP;
        }
        if (key.ScanCode == 0x0A) {
            return (char)INPUT_KEY_PAGE_DOWN;
        }
        if (key.ScanCode == 0x17) {
            return 27;
        }
        return 0;
    }

    if (key.UnicodeChar != 0) {
        if (key.UnicodeChar == '\r') {
            return '\n';
        }
        if (key.UnicodeChar == 127 || key.UnicodeChar == '\b') {
            return '\b';
        }

        if (has_ctrl) {
            if (key.UnicodeChar >= 'a' && key.UnicodeChar <= 'z') {
                return (char)(key.UnicodeChar - 'a' + 1);
            }
            if (key.UnicodeChar >= 'A' && key.UnicodeChar <= 'Z') {
                return (char)(key.UnicodeChar - 'A' + 1);
            }
        }

        if (key.UnicodeChar < 128) {
            return (char)key.UnicodeChar;
        }
        return 0;
    }

    return 0;
}

bool uefi_input_has_character(void) {
    if (pending_char != 0) {
        return true;
    }
    pending_char = uefi_input_poll();
    return pending_char != 0;
}

char uefi_input_try_read_character(void) {
    if (pending_char != 0) {
        char ch = pending_char;
        pending_char = 0;
        return ch;
    }
    return uefi_input_poll();
}

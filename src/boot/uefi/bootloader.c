#include "uefi.h"
#include "kernel_blob.h"

static const EFI_GUID gop_guid = {
    0x9042a9de, 0x23dc, 0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}
};

static const EFI_GUID loaded_image_guid = {
    0x5b1b31a1, 0x9b62, 0x11d0,
    {0x8e, 0xc1, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b}
};

static const EFI_GUID sfs_guid = {
    0x964e5b22, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

static const EFI_GUID text_input_ex_guid = {
    0xdd9e7534, 0x7762, 0x4698,
    {0x8c, 0x14, 0xf5, 0x85, 0x17, 0xa6, 0x25, 0xaa}
};

struct __attribute__((packed)) mb_info_struct {
    UINT32 flags;
    UINT32 mem_lower;
    UINT32 mem_upper;
    UINT32 boot_device;
    UINT32 cmdline;
    UINT32 mods_count;
    UINT32 mods_addr;
    UINT32 syms[4];
    UINT32 mmap_length;
    UINT32 mmap_addr;
    UINT32 drives_length;
    UINT32 drives_addr;
    UINT32 config_table;
    UINT32 boot_loader_name;
    UINT32 apm_table;
    UINT32 vbe_control_info;
    UINT32 vbe_mode_info;
    UINT16 vbe_mode;
    UINT16 vbe_interface_seg;
    UINT16 vbe_interface_off;
    UINT16 vbe_interface_len;
    UINT64 framebuffer_addr;
    UINT32 framebuffer_pitch;
    UINT32 framebuffer_width;
    UINT32 framebuffer_height;
    UINT8 framebuffer_bpp;
    UINT8 framebuffer_type;
    UINT8 framebuffer_color_info[6];
};

__attribute__((aligned(64))) static UINT8 mmap_buf[65536];

static void print_str(EFI_SYSTEM_TABLE *sys, const CHAR16 *str) {
    if (sys && sys->ConOut && sys->ConOut->OutputString) {
        sys->ConOut->OutputString(sys->ConOut, str);
    }
}

static void print_hex(EFI_SYSTEM_TABLE *sys, UINT64 val) {
    CHAR16 buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        UINT8 nibble = (val >> (i * 4)) & 0xF;
        buf[2 + (15 - i)] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    buf[18] = 0;
    print_str(sys, buf);
}

void* memset(void *dst, int val, UINTN n) {
    UINT8 *p = (UINT8*)dst;
    while (n--) {
        *p++ = (UINT8)val;
    }
    return dst;
}

void* memcpy(void *dst, const void *src, UINTN n) {
    UINT8 *d = (UINT8*)dst;
    const UINT8 *s = (const UINT8*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

static UINT64 load_kernel_elf(EFI_BOOT_SERVICES *bs, EFI_SYSTEM_TABLE *sys, const UINT8 *blob, UINTN blob_sz, EFI_FILE_PROTOCOL *file) {
    Elf64_Ehdr ehdr;
    if (file) {
        UINTN sz = sizeof(ehdr);
        file->SetPosition(file, 0);
        file->Read(file, &sz, &ehdr);
    } else if (blob && blob_sz >= sizeof(ehdr)) {
        memcpy(&ehdr, blob, sizeof(ehdr));
    } else {
        return 0;
    }

    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' || ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        return 0;
    }

    UINT64 kernel_addr = 0x100000;
    bs->FreePages(kernel_addr, 16384);
    EFI_STATUS st = bs->AllocatePages(2, 1, 16384, &kernel_addr);
    if (st != EFI_SUCCESS) {
        bs->FreePages(kernel_addr, 256);
        st = bs->AllocatePages(2, 1, 256, &kernel_addr);
    }
    print_str(sys, u"[VeoOS] AllocatePages(1MB, LoaderCode): ");
    print_hex(sys, (UINT64)st);
    print_str(sys, u"\r\n");

    for (UINT16 i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr ph;
        if (file) {
            UINTN phsz = sizeof(ph);
            file->SetPosition(file, ehdr.e_phoff + (UINT64)i * ehdr.e_phentsize);
            file->Read(file, &phsz, &ph);
        } else {
            memcpy(&ph, blob + ehdr.e_phoff + (UINT64)i * ehdr.e_phentsize, sizeof(ph));
        }

        if (ph.p_type == 1) {
            if (file) {
                file->SetPosition(file, ph.p_offset);
                UINTN rsz = (UINTN)ph.p_filesz;
                file->Read(file, &rsz, (void*)ph.p_paddr);
            } else {
                memcpy((void*)ph.p_paddr, blob + ph.p_offset, (UINTN)ph.p_filesz);
            }
            if (ph.p_memsz > ph.p_filesz) {
                memset((void*)(ph.p_paddr + ph.p_filesz), 0, (UINTN)(ph.p_memsz - ph.p_filesz));
            }
        }
    }

    return ehdr.e_entry;
}

typedef void (*kernel_func_t)(const void* mb_info);

__attribute__((ms_abi))
EFI_STATUS EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_BOOT_SERVICES *bs = SystemTable->BootServices;
    bs->SetWatchdogTimer(0, 0, 0, 0);

    struct mb_info_struct local_mb_info;
    memset(&local_mb_info, 0, sizeof(local_mb_info));

    print_str(SystemTable, u"[VeoOS] UEFI Bootloader Starting...\r\n");

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    EFI_STATUS st = bs->LocateProtocol((EFI_GUID*)&gop_guid, 0, (void**)&gop);
    if (st == EFI_SUCCESS && gop && gop->Mode && gop->Mode->Info) {
        UINT32 best_mode = gop->Mode->Mode;
        UINT32 best_w = gop->Mode->Info->HorizontalResolution;
        UINT32 best_h = gop->Mode->Info->VerticalResolution;
        UINT64 best_score = (UINT64)best_w * best_h;
        if (best_w * 10 >= best_h * 16) {
            best_score += (best_score / 4);
        }

        if (gop->QueryMode && gop->SetMode) {
            for (UINT32 m = 0; m < gop->Mode->MaxMode; m++) {
                UINTN sz = 0;
                EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *minfo = 0;
                if (gop->QueryMode(gop, m, &sz, &minfo) == EFI_SUCCESS && minfo) {
                    if (minfo->PixelFormat != 3) {
                        UINT32 mw = minfo->HorizontalResolution;
                        UINT32 mh = minfo->VerticalResolution;
                        UINT64 score = (UINT64)mw * mh;
                        if (mw * 10 >= mh * 16) {
                            score += (score / 4);
                        }
                        if (score > best_score) {
                            best_mode = m;
                            best_w = mw;
                            best_h = mh;
                            best_score = score;
                        }
                    }
                    bs->FreePool(minfo);
                }
            }
        }

        if (best_mode != gop->Mode->Mode && gop->SetMode) {
            gop->SetMode(gop, best_mode);
        }

        local_mb_info.flags |= (1 << 12);
        local_mb_info.framebuffer_addr = gop->Mode->FrameBufferBase;
        UINT32 ppsl = gop->Mode->Info->PixelsPerScanLine;
        if (ppsl < gop->Mode->Info->HorizontalResolution) {
            ppsl = gop->Mode->Info->HorizontalResolution;
        }
        local_mb_info.framebuffer_pitch = ppsl * 4;
        local_mb_info.framebuffer_width = gop->Mode->Info->HorizontalResolution;
        local_mb_info.framebuffer_height = gop->Mode->Info->VerticalResolution;
        local_mb_info.framebuffer_bpp = 32;
        local_mb_info.framebuffer_type = 1;
        print_str(SystemTable, u"[VeoOS] GOP Framebuffer: ");
        print_hex(SystemTable, local_mb_info.framebuffer_addr);
        print_str(SystemTable, u"\r\n");
    }

    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = 0;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs = 0;
    EFI_FILE_PROTOCOL *root_dir = 0;
    EFI_FILE_PROTOCOL *kernel_file = 0;

    st = bs->HandleProtocol(ImageHandle, (EFI_GUID*)&loaded_image_guid, (void**)&loaded_image);
    if (st == EFI_SUCCESS && loaded_image && loaded_image->DeviceHandle) {
        st = bs->HandleProtocol(loaded_image->DeviceHandle, (EFI_GUID*)&sfs_guid, (void**)&sfs);
        if (st == EFI_SUCCESS && sfs) {
            if (sfs->OpenVolume(sfs, &root_dir) == EFI_SUCCESS && root_dir) {
                if (root_dir->Open(root_dir, &kernel_file, u"veoos.elf", EFI_FILE_MODE_READ, 0) == EFI_SUCCESS) {
                    print_str(SystemTable, u"[VeoOS] Kernel found on boot drive.\r\n");
                } else if (root_dir->Open(root_dir, &kernel_file, u"VEOOS.ELF", EFI_FILE_MODE_READ, 0) == EFI_SUCCESS) {
                    print_str(SystemTable, u"[VeoOS] Kernel found on boot drive.\r\n");
                }
            }
        }
    }

    if (!kernel_file) {
        EFI_HANDLE *handles = 0;
        UINTN num_handles = 0;
        st = bs->LocateHandleBuffer(2, (EFI_GUID*)&sfs_guid, 0, &num_handles, &handles);
        if (st == EFI_SUCCESS && handles) {
            for (UINTN i = 0; i < num_handles && !kernel_file; i++) {
                EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *cur_sfs = 0;
                if (bs->HandleProtocol(handles[i], (EFI_GUID*)&sfs_guid, (void**)&cur_sfs) == EFI_SUCCESS && cur_sfs) {
                    EFI_FILE_PROTOCOL *cur_root = 0;
                    if (cur_sfs->OpenVolume(cur_sfs, &cur_root) == EFI_SUCCESS && cur_root) {
                        if (cur_root->Open(cur_root, &kernel_file, u"veoos.elf", EFI_FILE_MODE_READ, 0) == EFI_SUCCESS) {
                            root_dir = cur_root;
                            print_str(SystemTable, u"[VeoOS] Kernel found via search.\r\n");
                            break;
                        }
                        if (cur_root->Open(cur_root, &kernel_file, u"VEOOS.ELF", EFI_FILE_MODE_READ, 0) == EFI_SUCCESS) {
                            root_dir = cur_root;
                            print_str(SystemTable, u"[VeoOS] Kernel found via search.\r\n");
                            break;
                        }
                        cur_root->Close(cur_root);
                    }
                }
            }
            bs->FreePool(handles);
        }
    }

    UINT64 entry_point = 0;
    if (kernel_file) {
        print_str(SystemTable, u"[VeoOS] Loading kernel from filesystem...\r\n");
        entry_point = load_kernel_elf(bs, SystemTable, 0, 0, kernel_file);
        kernel_file->Close(kernel_file);
        if (root_dir) {
            root_dir->Close(root_dir);
        }
    }

    if (!entry_point) {
        print_str(SystemTable, u"[VeoOS] Loading embedded kernel payload...\r\n");
        entry_point = load_kernel_elf(bs, SystemTable, kernel_elf_blob, kernel_elf_blob_size, 0);
    }

    if (!entry_point) {
        print_str(SystemTable, u"[VeoOS] FATAL: Failed to load kernel!\r\n");
        while (1) { }
    }

    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *con_in_ex = 0;
    st = bs->HandleProtocol(SystemTable->ConsoleInHandle, (EFI_GUID*)&text_input_ex_guid, (void**)&con_in_ex);
    if (st != EFI_SUCCESS || !con_in_ex) {
        bs->LocateProtocol((EFI_GUID*)&text_input_ex_guid, 0, (void**)&con_in_ex);
    }
    if (con_in_ex) {
        *(UINT64*)0x100018 = (UINT64)con_in_ex;
    } else {
        *(UINT64*)0x100018 = 0;
    }

    *(UINT64*)0x100020 = (UINT64)SystemTable->ConIn;
    *(UINT64*)0x100028 = (UINT64)SystemTable;

    if (SystemTable->RuntimeServices) {
        *(UINT64*)0x100030 = (UINT64)SystemTable->RuntimeServices->ResetSystem;
    } else {
        *(UINT64*)0x100030 = 0;
    }

    UINTN mmap_sz = sizeof(mmap_buf);
    UINTN mkey = 0, dsz = 0;
    UINT32 dver = 0;
    st = bs->GetMemoryMap(&mmap_sz, (EFI_MEMORY_DESCRIPTOR*)mmap_buf, &mkey, &dsz, &dver);
    if (st == EFI_SUCCESS && dsz > 0) {
        UINT64 total_ram = 0;
        for (UINTN off = 0; off + dsz <= mmap_sz; off += dsz) {
            EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)(mmap_buf + off);
            total_ram += d->NumberOfPages * 4096;
        }
        local_mb_info.flags |= (1 << 0);
        local_mb_info.mem_upper = (UINT32)(total_ram / 1024);
    }

    memcpy((void*)0x100040, &local_mb_info, sizeof(local_mb_info));

    print_str(SystemTable, u"[VeoOS] Starting VeoOS kernel...\r\n");

    kernel_func_t kernel_entry = (kernel_func_t)entry_point;
    kernel_entry((const void*)0x100040);

    while (1) {
        __asm__ volatile("hlt");
    }

    return EFI_SUCCESS;
}

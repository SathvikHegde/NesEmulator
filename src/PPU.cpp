#include "PPU.h"
#include <cstring>
#include <cstdlib>

PPU::PPU() {
    memset(screen, 0, sizeof(screen));
    memset(tblName, 0, sizeof(tblName));
    // Authentic Nestopia YUV Hex Array (Little Endian AABBGGRR for Vulkan R8G8B8A8 unorm)
    palScreen[0x00] = 0xFF7C7C7C; palScreen[0x01] = 0xFFFC0000; palScreen[0x02] = 0xFFBC0000; palScreen[0x03] = 0xFFBC2844;
    palScreen[0x04] = 0xFF840094; palScreen[0x05] = 0xFF2000A8; palScreen[0x06] = 0xFF0010A8; palScreen[0x07] = 0xFF001488;
    palScreen[0x08] = 0xFF003050; palScreen[0x09] = 0xFF007800; palScreen[0x0A] = 0xFF006800; palScreen[0x0B] = 0xFF005800;
    palScreen[0x0C] = 0xFF584000; palScreen[0x0D] = 0xFF000000; palScreen[0x0E] = 0xFF000000; palScreen[0x0F] = 0xFF000000;
    
    palScreen[0x10] = 0xFFBCBCBC; palScreen[0x11] = 0xFFF87800; palScreen[0x12] = 0xFFF85800; palScreen[0x13] = 0xFFFC4468;
    palScreen[0x14] = 0xFFCC00D8; palScreen[0x15] = 0xFF5800E4; palScreen[0x16] = 0xFF0038F8; palScreen[0x17] = 0xFF105CE4;
    palScreen[0x18] = 0xFF007CAC; palScreen[0x19] = 0xFF00B800; palScreen[0x1A] = 0xFF00A800; palScreen[0x1B] = 0xFF44A800;
    palScreen[0x1C] = 0xFF888800; palScreen[0x1D] = 0xFF000000; palScreen[0x1E] = 0xFF000000; palScreen[0x1F] = 0xFF000000;
    
    palScreen[0x20] = 0xFFF8F8F8; palScreen[0x21] = 0xFFFCBC3C; palScreen[0x22] = 0xFFFC8868; palScreen[0x23] = 0xFFF87898;
    palScreen[0x24] = 0xFFF878F8; palScreen[0x25] = 0xFF9858F8; palScreen[0x26] = 0xFF5878F8; palScreen[0x27] = 0xFF44A0FC;
    palScreen[0x28] = 0xFF00B8F8; palScreen[0x29] = 0xFF18F8B8; palScreen[0x2A] = 0xFF54D858; palScreen[0x2B] = 0xFF98F858;
    palScreen[0x2C] = 0xFFD8E800; palScreen[0x2D] = 0xFF787878; palScreen[0x2E] = 0xFF000000; palScreen[0x2F] = 0xFF000000;
    
    palScreen[0x30] = 0xFFF8F8F8; palScreen[0x31] = 0xFFFCE4A4; palScreen[0x32] = 0xFFF8B8B8; palScreen[0x33] = 0xFFF8B8D8;
    palScreen[0x34] = 0xFFF8B8F8; palScreen[0x35] = 0xFFC0A4F8; palScreen[0x36] = 0xFFB0D0F0; palScreen[0x37] = 0xFFA8E0FC;
    palScreen[0x38] = 0xFF78D8F8; palScreen[0x39] = 0xFF78F8D8; palScreen[0x3A] = 0xFFB8F8B8; palScreen[0x3B] = 0xFFD8F8B8;
    palScreen[0x3C] = 0xFFFCFC00; palScreen[0x3D] = 0xFFF8D8F8; palScreen[0x3E] = 0xFF000000; palScreen[0x3F] = 0xFF000000;
}

PPU::~PPU() { }

uint8_t PPU::cpuRead(uint16_t addr, bool rdonly) {
    uint8_t data = 0x00;
    switch (addr) {
        case 0x0000: // Control
            break;
        case 0x0001: // Mask
            break;
        case 0x0002: // Status
            data = (status.reg & 0xE0) | (ppu_data_buffer & 0x1F);
            status.vertical_blank = 0;
            address_latch = 0;
            break;
        case 0x0003: // OAM Address
            break;
        case 0x0004: // OAM Data
            data = pOAM[OAMADDR];
            break;
        case 0x0005: // Scroll
            break;
        case 0x0006: // PPU Address
            break;
        case 0x0007: // PPU Data
            data = ppu_data_buffer;
            ppu_data_buffer = ppuRead(vram_addr.reg);
            if (vram_addr.reg >= 0x3F00) data = ppu_data_buffer;
            vram_addr.reg += (control.increment_mode ? 32 : 1);
            break;
    }
    return data;
}

void PPU::cpuWrite(uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x0000: // Control
            control.reg = data;
            trampoline_addr.nametable_x = control.nametable_x;
            trampoline_addr.nametable_y = control.nametable_y;
            break;
        case 0x0001: // Mask
            mask.reg = data;
            break;
        case 0x0002: // Status
            break;
        case 0x0003: // OAM Address
            OAMADDR = data;
            break;
        case 0x0004: // OAM Data
            pOAM[OAMADDR] = data;
            // writing to OAM via $2004 normally increments the address 
            // (technically only on correct scanlines/cycles but basic NES uses DMA anyway)
            break;
        case 0x0005: // Scroll
            if (address_latch == 0) {
                fine_x = data & 0x07;
                trampoline_addr.coarse_x = data >> 3;
                address_latch = 1;
            } else {
                trampoline_addr.fine_y = data & 0x07;
                trampoline_addr.coarse_y = data >> 3;
                address_latch = 0;
            }
            break;
        case 0x0006: // PPU Address
            if (address_latch == 0) {
                trampoline_addr.reg = (trampoline_addr.reg & 0x00FF) | ((data & 0x3F) << 8);
                address_latch = 1;
            } else {
                trampoline_addr.reg = (trampoline_addr.reg & 0xFF00) | data;
                vram_addr = trampoline_addr;
                address_latch = 0;
            }
            break;
        case 0x0007: // PPU Data
            ppuWrite(vram_addr.reg, data);
            vram_addr.reg += (control.increment_mode ? 32 : 1);
            break;
    }
}

uint8_t PPU::ppuRead(uint16_t addr, bool rdonly) {
    uint8_t data = 0x00;
    addr &= 0x3FFF;

    if (cart->ppuRead(addr, data)) {
        // Cartridge Handles CHR ROM/RAM
    } else if (addr >= 0x0000 && addr <= 0x1FFF) {
        data = tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF];
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Hardcode vertical mirroring for now
        addr &= 0x0FFF;
        if (addr >= 0x0000 && addr <= 0x03FF) data = tblName[0][addr & 0x03FF];
        else if (addr >= 0x0400 && addr <= 0x07FF) data = tblName[1][addr & 0x03FF];
        else if (addr >= 0x0800 && addr <= 0x0BFF) data = tblName[0][addr & 0x03FF];
        else if (addr >= 0x0C00 && addr <= 0x0FFF) data = tblName[1][addr & 0x03FF];
    } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        else if (addr == 0x0014) addr = 0x0004;
        else if (addr == 0x0018) addr = 0x0008;
        else if (addr == 0x001C) addr = 0x000C;
        data = tblPalette[addr] & (mask.grayscale ? 0x30 : 0x3F);
    }

    return data;
}

void PPU::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;

    if (cart->ppuWrite(addr, data)) {
        // Cartridge Handles CHR ROM/RAM
    } else if (addr >= 0x0000 && addr <= 0x1FFF) {
        tblPattern[(addr & 0x1000) >> 12][addr & 0x0FFF] = data;
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        addr &= 0x0FFF;
        if (addr >= 0x0000 && addr <= 0x03FF) tblName[0][addr & 0x03FF] = data;
        else if (addr >= 0x0400 && addr <= 0x07FF) tblName[1][addr & 0x03FF] = data;
        else if (addr >= 0x0800 && addr <= 0x0BFF) tblName[0][addr & 0x03FF] = data;
        else if (addr >= 0x0C00 && addr <= 0x0FFF) tblName[1][addr & 0x03FF] = data;
    } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        else if (addr == 0x0014) addr = 0x0004;
        else if (addr == 0x0018) addr = 0x0008;
        else if (addr == 0x001C) addr = 0x000C;
        tblPalette[addr] = data;
    }
}

void PPU::ConnectCartridge(const std::shared_ptr<Cartridge>& cartridge) {
    this->cart = cartridge;
}

void PPU::clock() {
    auto IncrementScrollX = [&]() {
        if (mask.render_background || mask.render_sprites) {
            if (vram_addr.coarse_x == 31) {
                vram_addr.coarse_x = 0;
                vram_addr.nametable_x = ~vram_addr.nametable_x;
            } else {
                vram_addr.coarse_x++;
            }
        }
    };

    auto IncrementScrollY = [&]() {
        if (mask.render_background || mask.render_sprites) {
            if (vram_addr.fine_y < 7) {
                vram_addr.fine_y++;
            } else {
                vram_addr.fine_y = 0;
                if (vram_addr.coarse_y == 29) {
                    vram_addr.coarse_y = 0;
                    vram_addr.nametable_y = ~vram_addr.nametable_y;
                } else if (vram_addr.coarse_y == 31) {
                    vram_addr.coarse_y = 0;
                } else {
                    vram_addr.coarse_y++;
                }
            }
        }
    };

    auto TransferAddressX = [&]() {
        if (mask.render_background || mask.render_sprites) {
            vram_addr.nametable_x = trampoline_addr.nametable_x;
            vram_addr.coarse_x    = trampoline_addr.coarse_x;
        }
    };

    auto TransferAddressY = [&]() {
        if (mask.render_background || mask.render_sprites) {
            vram_addr.fine_y      = trampoline_addr.fine_y;
            vram_addr.nametable_y = trampoline_addr.nametable_y;
            vram_addr.coarse_y    = trampoline_addr.coarse_y;
        }
    };

    auto LoadBackgroundShifters = [&]() {
        bg_shifter_pattern_lo = (bg_shifter_pattern_lo & 0xFF00) | bg_next_tile_lsb;
        bg_shifter_pattern_hi = (bg_shifter_pattern_hi & 0xFF00) | bg_next_tile_msb;
        bg_shifter_attrib_lo  = (bg_shifter_attrib_lo & 0xFF00) | ((bg_next_tile_attrib & 0b01) ? 0xFF : 0x00);
        bg_shifter_attrib_hi  = (bg_shifter_attrib_hi & 0xFF00) | ((bg_next_tile_attrib & 0b10) ? 0xFF : 0x00);
    };

    auto UpdateShifters = [&]() {
        if (mask.render_background) {
            bg_shifter_pattern_lo <<= 1;
            bg_shifter_pattern_hi <<= 1;
            bg_shifter_attrib_lo <<= 1;
            bg_shifter_attrib_hi <<= 1;
        }
    };

    if (scanline >= -1 && scanline < 240) {
        if (scanline == 0 && cycle == 0) cycle = 1;
        if (scanline == -1 && cycle == 1) {
            status.vertical_blank = 0;
            status.sprite_zero_hit = 0;
            status.sprite_overflow = 0;
        }

        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338)) {
            UpdateShifters();

            switch ((cycle - 1) % 8) {
                case 0:
                    LoadBackgroundShifters();
                    bg_next_tile_id = ppuRead(0x2000 | (vram_addr.reg & 0x0FFF));
                    break;
                case 2:
                    bg_next_tile_attrib = ppuRead(0x23C0 | (vram_addr.nametable_y << 11) 
                        | (vram_addr.nametable_x << 10) 
                        | ((vram_addr.coarse_y >> 2) << 3) 
                        | (vram_addr.coarse_x >> 2));
                    if (vram_addr.coarse_y & 0x02) bg_next_tile_attrib >>= 4;
                    if (vram_addr.coarse_x & 0x02) bg_next_tile_attrib >>= 2;
                    bg_next_tile_attrib &= 0x03;
                    break;
                case 4:
                    bg_next_tile_lsb = ppuRead((control.pattern_background << 12) 
                        + ((uint16_t)bg_next_tile_id << 4) 
                        + (vram_addr.fine_y) + 0);
                    break;
                case 6:
                    bg_next_tile_msb = ppuRead((control.pattern_background << 12) 
                        + ((uint16_t)bg_next_tile_id << 4) 
                        + (vram_addr.fine_y) + 8);
                    break;
                case 7:
                    IncrementScrollX();
                    break;
            }
        }

        if (cycle == 256) IncrementScrollY();
        if (cycle == 257) { 
            LoadBackgroundShifters(); 
            TransferAddressX(); 
            
            // --- Sprite Evaluation for Next Scanline ---
            if (scanline >= 0) {
                std::memset(spriteScanline, 0xFF, sizeof(spriteScanline));
                sprite_count = 0;
                bSpriteZeroHitPossible = false;
                
                uint8_t nOAMEntry = 0;
                while (nOAMEntry < 64 && sprite_count <= 8) {
                    int16_t diff = ((int16_t)scanline) - ((int16_t)OAM[nOAMEntry].y);
                    if (diff >= 0 && diff < (control.sprite_size ? 16 : 8)) {
                        if (sprite_count < 8) {
                            if (nOAMEntry == 0) bSpriteZeroHitPossible = true;
                            spriteScanline[sprite_count] = OAM[nOAMEntry];
                            sprite_count++;
                        } else {
                            status.sprite_overflow = 1;
                            break; // Accurately stop evaluating to crash/overflow correctly
                        }
                    }
                    nOAMEntry++;
                }
            }
        }
        
        if (cycle == 340) {
            // Fetch Sprite Patterns
            for (uint8_t i = 0; i < sprite_count; i++) {
                uint8_t sprite_pattern_bits_lo, sprite_pattern_bits_hi;
                uint16_t sprite_pattern_addr_lo, sprite_pattern_addr_hi;
                
                if (!control.sprite_size) {
                    // 8x8 Sprites
                    if (!(spriteScanline[i].attribute & 0x80)) { // Normal
                        sprite_pattern_addr_lo = (control.pattern_sprite << 12) 
                            | (spriteScanline[i].id << 4) 
                            | (scanline - spriteScanline[i].y);
                    } else { // Flipped Vertically
                        sprite_pattern_addr_lo = (control.pattern_sprite << 12) 
                            | (spriteScanline[i].id << 4) 
                            | (7 - (scanline - spriteScanline[i].y));
                    }
                } else {
                    // 8x16 Sprites
                    if (!(spriteScanline[i].attribute & 0x80)) { // Normal
                        if (scanline - spriteScanline[i].y < 8) {
                            sprite_pattern_addr_lo = ((spriteScanline[i].id & 0x01) << 12)  
                                | ((spriteScanline[i].id & 0xFE) << 4) 
                                | ((scanline - spriteScanline[i].y) & 0x07);
                        } else {
                            sprite_pattern_addr_lo = ((spriteScanline[i].id & 0x01) << 12)  
                                | (((spriteScanline[i].id & 0xFE) + 1) << 4) 
                                | ((scanline - spriteScanline[i].y) & 0x07);
                        }
                    } else { // Flipped vertically
                        if (scanline - spriteScanline[i].y < 8) {
                            sprite_pattern_addr_lo = ((spriteScanline[i].id & 0x01) << 12)  
                                | (((spriteScanline[i].id & 0xFE) + 1) << 4) 
                                | ((7 - (scanline - spriteScanline[i].y)) & 0x07);
                        } else {
                            sprite_pattern_addr_lo = ((spriteScanline[i].id & 0x01) << 12)  
                                | ((spriteScanline[i].id & 0xFE) << 4) 
                                | ((7 - (scanline - spriteScanline[i].y)) & 0x07);
                        }
                    }
                }
                
                sprite_pattern_addr_hi = sprite_pattern_addr_lo + 8;
                sprite_pattern_bits_lo = ppuRead(sprite_pattern_addr_lo);
                sprite_pattern_bits_hi = ppuRead(sprite_pattern_addr_hi);
                
                // Flip horizontally if requested (0x40)
                if (spriteScanline[i].attribute & 0x40) {
                    auto flipbyte = [](uint8_t b) {
                        b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
                        b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
                        b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
                        return b;
                    };
                    sprite_pattern_bits_lo = flipbyte(sprite_pattern_bits_lo);
                    sprite_pattern_bits_hi = flipbyte(sprite_pattern_bits_hi);
                }
                
                sprite_shifter_pattern_lo[i] = sprite_pattern_bits_lo;
                sprite_shifter_pattern_hi[i] = sprite_pattern_bits_hi;
            }
        }
        
        if (cycle == 338 || cycle == 340) bg_next_tile_id = ppuRead(0x2000 | (vram_addr.reg & 0x0FFF));
        if (scanline == -1 && cycle >= 280 && cycle < 305) TransferAddressY();
    }

    if (scanline == 241 && cycle == 1) {
        status.vertical_blank = 1;
        if (control.enable_nmi) nmi = true;
    }

    // Rendering Pixel
    uint8_t bg_pixel = 0x00;
    uint8_t bg_palette = 0x00;

    if (mask.render_background) {
        uint16_t bit_mux = 0x8000 >> fine_x;
        uint8_t p0_pixel = (bg_shifter_pattern_lo & bit_mux) > 0;
        uint8_t p1_pixel = (bg_shifter_pattern_hi & bit_mux) > 0;
        bg_pixel = (p1_pixel << 1) | p0_pixel;

        uint8_t bg_pal0 = (bg_shifter_attrib_lo & bit_mux) > 0;
        uint8_t bg_pal1 = (bg_shifter_attrib_hi & bit_mux) > 0;
        bg_palette = (bg_pal1 << 1) | bg_pal0;
    }

    uint8_t fg_pixel = 0x00;
    uint8_t fg_palette = 0x00;
    uint8_t fg_priority = 0x00;

    if (mask.render_sprites) {
        bSpriteZeroBeingRendered = false;
        
        for (uint8_t i = 0; i < sprite_count; i++) {
            if (spriteScanline[i].x == 0) {
                uint8_t fg_pixel_lo = (sprite_shifter_pattern_lo[i] & 0x80) > 0;
                uint8_t fg_pixel_hi = (sprite_shifter_pattern_hi[i] & 0x80) > 0;
                fg_pixel = (fg_pixel_hi << 1) | fg_pixel_lo;
                
                fg_palette = (spriteScanline[i].attribute & 0x03) + 0x04;
                fg_priority = (spriteScanline[i].attribute & 0x20) == 0;
                
                if (fg_pixel != 0) {
                    if (i == 0) bSpriteZeroBeingRendered = true;
                    break; // Hit front-most sprite
                }
            }
        }
    }
    
    // Left-Edge Hardware Masking
    if (!mask.render_background_left && cycle >= 1 && cycle <= 8) bg_pixel = 0;
    if (!mask.render_sprites_left && cycle >= 1 && cycle <= 8) fg_pixel = 0;

    uint8_t final_pixel = 0x00;
    uint8_t final_palette = 0x00;
    
    if (bg_pixel == 0 && fg_pixel == 0) {
        final_pixel = 0x00;
        final_palette = 0x00;
    } else if (bg_pixel == 0 && fg_pixel > 0) {
        final_pixel = fg_pixel;
        final_palette = fg_palette;
    } else if (bg_pixel > 0 && fg_pixel == 0) {
        final_pixel = bg_pixel;
        final_palette = bg_palette;
    } else if (bg_pixel > 0 && fg_pixel > 0) {
        if (fg_priority) {
            final_pixel = fg_pixel;
            final_palette = fg_palette;
        } else {
            final_pixel = bg_pixel;
            final_palette = bg_palette;
        }
        
        // Sprite Zero Hit detection
        if (bSpriteZeroHitPossible && bSpriteZeroBeingRendered) {
            if (mask.render_background && mask.render_sprites) {
                if (!(mask.render_background_left | mask.render_sprites_left)) {
                    if (cycle >= 9 && cycle < 258) status.sprite_zero_hit = 1;
                } else {
                    if (cycle >= 1 && cycle < 258) status.sprite_zero_hit = 1;
                }
            }
        }
    }

    if (scanline >= 0 && scanline < 240 && cycle >= 1 && cycle < 257) {
        uint8_t color_index = ppuRead(0x3F00 + (final_palette << 2) + final_pixel) & 0x3F;
        screen[scanline * 256 + (cycle - 1)] = palScreen[color_index];
    }
    
    // Shift sprites
    if (mask.render_sprites && cycle >= 1 && cycle < 258) {
        for (uint8_t i = 0; i < sprite_count; i++) {
            if (spriteScanline[i].x > 0) {
                spriteScanline[i].x--;
            } else {
                sprite_shifter_pattern_lo[i] <<= 1;
                sprite_shifter_pattern_hi[i] <<= 1;
            }
        }
    }

    cycle++;
    if (cycle >= 341) {
        cycle = 0;
        scanline++;
        if (scanline >= 261) {
            scanline = -1;
            frame_complete = true;
        }
    }
}

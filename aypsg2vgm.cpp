#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VGM_CMD_END    0x66
#define AY8910_CLOCK   1773400  // Default clock rate for AY-3-8910 (ZX Spectrum)

// Function to write a 32-bit value in little-endian format
void write_le32(uint8_t* buffer, size_t offset, uint32_t value) {
    buffer[offset + 0] = (value & 0x000000FF);
    buffer[offset + 1] = (value & 0x0000FF00) >> 8;
    buffer[offset + 2] = (value & 0x00FF0000) >> 16;
    buffer[offset + 3] = (value & 0xFF000000) >> 24;
}

// Function to write the VGM header for AY-3-8910
void write_vgm_header(FILE* f, int rate, int ay_type) {
    uint8_t header[0x100];
    memset(header, 0, sizeof(header));

    // 'Vgm '
    header[0x00] = 'V';
    header[0x01] = 'g';
    header[0x02] = 'm';
    header[0x03] = ' ';

    // EOF offset placeholder (set later in finalize)
    write_le32(header, 0x04, 0x00000000);

    // Version = 0x00000151
    write_le32(header, 0x08, 0x00000151);

    // SN76489 clock = 0
    write_le32(header, 0x0C, 0);

    // YM2413 clock = 0
    write_le32(header, 0x10, 0);

    // GD3 offset = 0
    write_le32(header, 0x14, 0);

    // Total # samples (set later)
    write_le32(header, 0x18, 0);

    // Loop offset and loop # samples = 0
    write_le32(header, 0x1C, 0);
    write_le32(header, 0x20, 0);

    // Rate (0 = default)
    write_le32(header, 0x24, 0);

    // SN76489 feedback and shift register
    write_le32(header, 0x28, 0);
    header[0x2C] = 0; // shift register width
    header[0x2D] = 0; // SN76489 flags

    // YM2612 clock = 0
    header[0x2E] = 0; header[0x2F] = 0;

    // YM2151 clock = 0
    write_le32(header, 0x30, 0);

    // VGM data offset = 0x0014 (relative to 0x34)
    // If our data starts at 0x80, then offset is (0x80 - 0x34) = 0x4C
    write_le32(header, 0x34, 0x0000004C);

    // All remaining chip clocks zeroed
    // (from 0x38 to 0x73 is already zeroed via memset)

    // AY8910 clock rate
    write_le32(header, 0x74, (uint32_t)rate);

    // AY chip type
    header[0x78] = (uint8_t)ay_type;

    // AY flags — single chip, no legacy mapping
    header[0x79] = 0x00;

    // Reserved (0x7A-0x7B already zero)

    // Write full 0x80 (128) byte header
    fwrite(header, 1, 0x80, f);
}

// Finalize the VGM file by patching the EOF and sample data
void finalize_vgm(FILE* f, uint32_t total_samples, int rate, double delay) {
    // Write the VGM EOF command (0x66) at the end of the file.
    fputc(VGM_CMD_END, f);

    // Get the final file position (which is the total file size)
    long final_pos = ftell(f);

    // The EOF offset in the header (at offset 0x04) is defined as (file size - 4)
    uint32_t eof_offset = (uint32_t)(final_pos - 4);

    // Prepare a 4-byte buffer and write the value in little-endian order
    uint8_t offset_bytes[4];
    write_le32(offset_bytes, 0, eof_offset);

    // Seek to offset 0x04 (i.e. after the "Vgm " magic) in the file
    fseek(f, 0x04, SEEK_SET);

    // Write the patched EOF offset in little-endian order
    fwrite(offset_bytes, 1, 4, f);

    // Calculate the total sample count and write it to the file
    uint8_t total_samples_bytes[4];
    write_le32(total_samples_bytes, 0, total_samples);
    fseek(f, 0x18, SEEK_SET);  // Total samples pointer
    fwrite(total_samples_bytes, 1, 4, f);

    // Calculate the clock rate for AY-3-8910 (e.g., 1773400 Hz / delay)
    uint8_t rate_bytes[4];
    write_le32(rate_bytes, 0, (uint32_t)(round(rate / delay)));
    fseek(f, 0x24, SEEK_SET);  // AY-3-8910 clock rate pointer
    fwrite(rate_bytes, 1, 4, f);

    // Flush to make sure everything is written out
    fflush(f);
}

void write_psg_data(FILE* f, FILE* p, int rate, double delay) {
    uint32_t sample_num = 0;

    // Read PSG data
    uint8_t header_check[4];
    fread(header_check, 1, 4, p);
    assert(header_check[0] == 'P' && header_check[1] == 'S' && header_check[2] == 'G' && header_check[3] == 0x1A);

    // Read metadata/padding bytes
    uint8_t metadata[12];
    fread(metadata, 1, sizeof(metadata), p);
    
    // Read PSG commands and write to VGM file
    while (1) {
        uint8_t data;
        if (fread(&data, 1, 1, p) != 1) break;

        if (data == 0xFE) {
            uint8_t wait_data;
            fread(&wait_data, 1, 1, p);
            int wait = (wait_data * 4) * 882;

            for (int i = 0; i < wait / 0xFFFF; i++) {
                uint8_t cmd[] = { 0x61 };
                uint16_t value = 0xFFFF;
                fwrite(cmd, 1, sizeof(cmd), f);
                fwrite(&value, sizeof(value), 1, f);
            }

            uint16_t remainder = wait % 0xFFFF;
            uint8_t cmd[] = { 0x61 };
            fwrite(cmd, 1, sizeof(cmd), f);
            fwrite(&remainder, sizeof(remainder), 1, f);

            sample_num += wait;
        }
        else if (data == 0xFF) {
            uint8_t cmd[] = { 0x63 };
            fwrite(cmd, 1, sizeof(cmd), f);
            sample_num += 882;
        }
        else {
            uint8_t cmd[] = { 0xA0 };
            fwrite(cmd, 1, sizeof(cmd), f);
            fwrite(&data, sizeof(data), 1, f);

            uint8_t next_data;
            fread(&next_data, 1, 1, p);
            fwrite(&next_data, sizeof(next_data), 1, f);
        }
    }

    printf("Sample count: 0x%lx\n", sample_num);

    // Finalize VGM with the correct sample count and rate calculation
    finalize_vgm(f, sample_num, rate, 44100.0 / delay);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_psg> [output_vgm] [-r rate] [-t ay_type]\n", argv[0]);
        return 1;
    }

    FILE* p = fopen(argv[1], "rb");
    if (!p) {
        perror("Error opening input PSG file");
        return 1;
    }

    char output_filename[1024];
    if (argc >= 3 && argv[2][0] != '-') {
        strncpy(output_filename, argv[2], sizeof(output_filename));
    }
    else {
        // Copy input filename and replace extension with .vgm
        strncpy(output_filename, argv[1], sizeof(output_filename) - 1);
        output_filename[sizeof(output_filename) - 1] = '\0';

        char* dot = strrchr(output_filename, '.');
        if (dot) {
            strcpy(dot, ".vgm");
        }
        else {
            strcat(output_filename, ".vgm");
        }
    }

    FILE* f = fopen(output_filename, "wb");
    if (!f) {
        perror("Error opening output VGM file");
        fclose(p);
        return 1;
    }

    int rate = 1773400;  // Default rate (ZX Spectrum clock rate)
    int ay_type = 1;     // Default AY chip type (AY-3-8910)

    // Process optional arguments
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] != '-') continue;  // Skip non-option arguments (already handled output file)
        if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            rate = atoi(argv[++i]);
            printf("Setting rate to %d\n", rate);
        }
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            ay_type = atoi(argv[++i]);
            printf("Setting type to %d\n", ay_type);
        }
    }

    // Write the VGM header with the specified rate
    write_vgm_header(f, rate, ay_type);

    // Set the delay based on the frequency (60 Hz = 882, 50 Hz = 735)
    double delay = (ay_type == 1) ? 882 : 735;  // Default 60 Hz or AY type-specific delay

    // Write PSG data and finalize the VGM file
    write_psg_data(f, p, rate, delay);

    fclose(p);
    fclose(f);

    printf("\nConversion complete.\n");

    return 0;
}
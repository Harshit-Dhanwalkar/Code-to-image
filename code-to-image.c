#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <dirent.h>

// Define STB_IMAGE_WRITE_IMPLEMENTATION and STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#define CHANNELS 3

typedef struct {
    char *name;
    char *path;
} FontInfo;

FontInfo *discovered_fonts = NULL;
int discovered_fonts_count = 0;
int discovered_fonts_capacity = 0;

void hex_to_rgb(const char* hex_color, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!hex_color || strlen(hex_color) != 7 || hex_color[0] != '#') {
        *r = *g = *b = 0; // Default to black on error
        return;
    }
    sscanf(hex_color + 1, "%2hhx%2hhx%2hhx", r, g, b);
}

void draw_char_bitmap(uint8_t* img_pixels, int img_width, int img_height,
                      uint8_t* char_pixels, int char_width, int char_height,
                      int draw_x, int draw_y,
                      uint8_t r, uint8_t g, uint8_t b) {
    for (int cy = 0; cy < char_height; ++cy) {
        for (int cx = 0; cx < char_width; ++cx) {
            int img_px = draw_x + cx;
            int img_py = draw_y + cy;

            // Check boundaries
            if (img_px >= 0 && img_px < img_width && img_py >= 0 && img_py < img_height) {
                uint8_t alpha = char_pixels[cy * char_width + cx];
                int img_idx = (img_py * img_width + img_px) * 3; // 3 channels (RGB)

                float alpha_norm = alpha / 255.0f;

                img_pixels[img_idx + 0] = (uint8_t)(alpha_norm * r + (1.0f - alpha_norm) * img_pixels[img_idx + 0]);
                img_pixels[img_idx + 1] = (uint8_t)(alpha_norm * g + (1.0f - alpha_norm) * img_pixels[img_idx + 1]);
                img_pixels[img_idx + 2] = (uint8_t)(alpha_norm * b + (1.0f - alpha_norm) * img_pixels[img_idx + 2]);
            }
        }
    }
}

// Function to draw text string onto the image buffer and return the end x-position
int draw_text(uint8_t* img_pixels, int img_width, int img_height,
               int start_x, int start_y, const char* text,
               stbtt_fontinfo* font, float scale, uint8_t r, uint8_t g, uint8_t b) {

    int x_cursor = start_x;

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
    int baseline = (int)(ascent * scale);

    for (int i = 0; text[i]; ++i) {
        int codepoint = (unsigned char)text[i]; // Simple ASCII assumed, handle UTF-8 for real

        int char_width, char_height, x_offset, y_offset;
        uint8_t* char_bitmap = stbtt_GetCodepointBitmap(font, 0, scale, codepoint, &char_width, &char_height, &x_offset, &y_offset);

        if (char_bitmap) {
            int draw_x = x_cursor + x_offset;
            int draw_y = start_y + baseline + y_offset;

            draw_char_bitmap(img_pixels, img_width, img_height,
                             char_bitmap, char_width, char_height,
                             draw_x, draw_y, r, g, b);

            free(char_bitmap);
        }

        int advance_width;
        stbtt_GetCodepointHMetrics(font, codepoint, &advance_width, NULL);
        x_cursor += (int)(advance_width * scale);
    }
    return x_cursor;
}

// Function to add a font to the discovered_fonts list
void add_font(const char* name, const char* path) {
    if (discovered_fonts_count >= discovered_fonts_capacity) {
        discovered_fonts_capacity = (discovered_fonts_capacity == 0) ? 4 : discovered_fonts_capacity * 2;
        discovered_fonts = realloc(discovered_fonts, sizeof(FontInfo) * discovered_fonts_capacity);
        if (!discovered_fonts) {
            fprintf(stderr, "Memory allocation failed for fonts list!\n");
            exit(EXIT_FAILURE);
        }
    }
    discovered_fonts[discovered_fonts_count].name = strdup(name);
    discovered_fonts[discovered_fonts_count].path = strdup(path);
    if (!discovered_fonts[discovered_fonts_count].name || !discovered_fonts[discovered_fonts_count].path) {
        fprintf(stderr, "Memory allocation failed for font name/path!\n");
        exit(EXIT_FAILURE);
    }
    discovered_fonts_count++;
}

// Recursive function to collect .ttf font files
void collect_fonts_recursive(const char* base_path) {
    DIR *dir = opendir(base_path);
    if (!dir) {
        fprintf(stderr, "Warning: Could not open directory '%s'\n", base_path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            collect_fonts_recursive(path);
        } else if (entry->d_type == DT_REG) {
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".ttf") == 0) {
                char font_name[256];
                strncpy(font_name, entry->d_name, ext - entry->d_name);
                font_name[ext - entry->d_name] = '\0';
                add_font(font_name, path);
            }
        }
    }
    closedir(dir);
}

// Function to free all discovered font info
void free_discovered_fonts() {
    for (int i = 0; i < discovered_fonts_count; ++i) {
        free(discovered_fonts[i].name);
        free(discovered_fonts[i].path);
    }
    free(discovered_fonts);
    discovered_fonts = NULL;
    discovered_fonts_count = 0;
    discovered_fonts_capacity = 0;
}


// Print usage help
void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s [options] <output_image_path>\n\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -f FONT    Select font (e.g., 'JetBrainsMono-Regular'). See available fonts below.\n");
    fprintf(stderr, "  -fs SIZE   Set font size in pixels (default: 18.0)\n");
    fprintf(stderr, "  -w WIDTH   Set image width in pixels (default: 800)\n");
    fprintf(stderr, "  -h HEIGHT  Set image height in pixels (default: 600)\n");
    fprintf(stderr, "\nAvailable Fonts (from ./Fonts/ directory):\n");
    if (discovered_fonts_count == 0) {
        fprintf(stderr, "  No fonts found. Ensure .ttf files are in 'Fonts/' or its subdirectories.\n");
    } else {
        for (int i = 0; i < discovered_fonts_count; ++i) {
            fprintf(stderr, "  - %s\n", discovered_fonts[i].name);
        }
    }
    fprintf(stderr, "\n");
}


int main(int argc, char **argv) {
    const char *output_image_path = "highlighted_code.png";
    const char *selected_font_name = NULL;
    float font_pixel_height = 18.0f;
    int img_width = 800;
    int img_height = 600;

    collect_fonts_recursive("Fonts");

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-u") == 0) {
            print_usage(argv[0]);
            free_discovered_fonts();
            return 0; // EXIT IMMEDIATELY AFTER PRINTING HELP
        }
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            selected_font_name = argv[++i];
        } else if (strcmp(argv[i], "-fs") == 0 && i + 1 < argc) {
            font_pixel_height = atof(argv[++i]);
            if (font_pixel_height <= 0) {
                fprintf(stderr, "Error: Font size must be positive.\n");
                free_discovered_fonts();
                return 1;
            }
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            img_width = atoi(argv[++i]);
            if (img_width <= 0) {
                fprintf(stderr, "Error: Image width must be positive.\n");
                free_discovered_fonts();
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            img_height = atoi(argv[++i]);
            if (img_height <= 0) {
                fprintf(stderr, "Error: Image height must be positive.\n");
                free_discovered_fonts();
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            free_discovered_fonts();
            return 1;
        } else {
            output_image_path = argv[i]; // Positional argument for output file
        }
    }

    const char* font_to_load_path = NULL;
    if (selected_font_name == NULL && discovered_fonts_count > 0) {
        font_to_load_path = discovered_fonts[0].path;
        fprintf(stderr, "No font specified. Defaulting to '%s'.\n", discovered_fonts[0].name);
    } else if (selected_font_name == NULL && discovered_fonts_count == 0) {
         fprintf(stderr, "Error: No fonts found in 'Fonts/' directory. Cannot proceed without a font.\n");
         free_discovered_fonts();
         return 1;
    } else {
        for (int i = 0; i < discovered_fonts_count; ++i) {
            if (strcmp(selected_font_name, discovered_fonts[i].name) == 0) {
                font_to_load_path = discovered_fonts[i].path;
                break;
            }
        }
        if (!font_to_load_path) {
            fprintf(stderr, "Error: Specified font '%s' not found.\n", selected_font_name);
            print_usage(argv[0]);
            free_discovered_fonts();
            return 1;
        }
    }


    // Allocate memory for image pixels
    uint8_t *pixels = (uint8_t *)malloc(img_width * img_height * CHANNELS);
    if (!pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer memory!\n");
        free_discovered_fonts();
        return 1;
    }

    // --- 1. Load Font ---
    long font_buffer_size;
    unsigned char* font_buffer = NULL;

    FILE* font_file = fopen(font_to_load_path, "rb");
    if (!font_file) {
        fprintf(stderr, "Error: Could not open font file '%s'. This should not happen if discovered correctly.\n", font_to_load_path);
        free(pixels);
        free_discovered_fonts();
        return 1;
    }

    fseek(font_file, 0, SEEK_END);
    font_buffer_size = ftell(font_file);
    fseek(font_file, 0, SEEK_SET);

    font_buffer = (unsigned char*)malloc(font_buffer_size);
    if (!font_buffer) {
        fprintf(stderr, "Failed to allocate font buffer memory!\n");
        fclose(font_file);
        free(pixels);
        free_discovered_fonts();
        return 1;
    }
    fread(font_buffer, 1, font_buffer_size, font_file);
    fclose(font_file);

    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, font_buffer, 0)) {
        fprintf(stderr, "Failed to initialize font from '%s'!\n", font_to_load_path);
        free(font_buffer);
        free(pixels);
        free_discovered_fonts();
        return 1;
    }

    float scale = stbtt_ScaleForPixelHeight(&font_info, font_pixel_height);


    // --- 2. Define Colors ---
    uint8_t bg_r, bg_g, bg_b;
    uint8_t code_bg_r, code_bg_g, code_bg_b;
    uint8_t default_text_r, default_text_g, default_text_b;
    uint8_t comment_r, comment_g, comment_b;
    uint8_t keyword_r, keyword_g, keyword_b;
    uint8_t function_r, function_g, function_b;
    uint8_t string_r, string_g, string_b;
    uint8_t literal_r, literal_g, literal_b;

    hex_to_rgb("#1a1a1a", &bg_r, &bg_g, &bg_b);
    hex_to_rgb("#0d0d0d", &code_bg_r, &code_bg_g, &code_bg_b);
    hex_to_rgb("#f8f8f2", &default_text_r, &default_text_g, &default_text_b);
    hex_to_rgb("#6272a4", &comment_r, &comment_g, &comment_b);
    hex_to_rgb("#ff79c6", &keyword_r, &keyword_g, &keyword_b);
    hex_to_rgb("#50fa7b", &function_r, &function_g, &function_b);
    hex_to_rgb("#f1fa8c", &string_r, &string_g, &string_b);
    hex_to_rgb("#ffb86c", &literal_r, &literal_g, &literal_b);


    // --- 3. Fill Background ---
    for (int y = 0; y < img_height; ++y) {
        for (int x = 0; x < img_width; ++x) {
            int index = (y * img_width + x) * CHANNELS;
            pixels[index + 0] = bg_r;
            pixels[index + 1] = bg_g;
            pixels[index + 2] = bg_b;
        }
    }

    // --- 4. Draw Code Block Background ---
    int padding = 20;
    int code_block_x = padding;
    int code_block_y = padding;
    int code_block_width = img_width - 2 * padding;
    int code_block_height = img_height - 2 * padding;

    for (int y = code_block_y; y < code_block_y + code_block_height; ++y) {
        for (int x = code_block_x; x < code_block_x + code_block_width; ++x) {
            if (x >=0 && x < img_width && y >= 0 && y < img_height) { // Safety check
                int index = (y * img_width + x) * CHANNELS;
                pixels[index + 0] = code_bg_r;
                pixels[index + 1] = code_bg_g;
                pixels[index + 2] = code_bg_b;
            }
        }
    }

    // --- 5. Draw Sample Code Lines ---
    int current_line_y = code_block_y + 10;
    int line_height = (int)(font_pixel_height * 1.5);

    // Line 1: // C Code Example:
    draw_text(pixels, img_width, img_height, code_block_x + 10, current_line_y, "// C Code Example:", &font_info, scale, comment_r, comment_g, comment_b);
    current_line_y += line_height;

    // Line 2: #include <stdio.h>
    int x_pos = code_block_x + 10;
    x_pos = draw_text(pixels, img_width, img_height, x_pos, current_line_y, "#include ", &font_info, scale, keyword_r, keyword_g, keyword_b);
    x_pos = draw_text(pixels, img_width, img_height, x_pos, current_line_y, "<stdio.h>", &font_info, scale, string_r, string_g, string_b);
    draw_text(pixels, img_width, img_height, x_pos, current_line_y, " // Include standard I/O", &font_info, scale, comment_r, comment_g, comment_b);
    current_line_y += line_height;

    // Line 3: int main() {
    x_pos = code_block_x + 10;
    x_pos = draw_text(pixels, img_width, img_height, x_pos, current_line_y, "int ", &font_info, scale, keyword_r, keyword_g, keyword_b);
    x_pos = draw_text(pixels, img_width, img_height, x_pos, current_line_y, "main", &font_info, scale, function_r, function_g, function_b); // function color
    draw_text(pixels, img_width, img_height, x_pos, current_line_y, "() {", &font_info, scale, default_text_r, default_text_g, default_text_b);
    current_line_y += line_height;

    // Line 4: printf("Hello, World!\n");
    x_pos = code_block_x + 10 + 20; // Indent 20 pixels
    x_pos = draw_text(pixels, img_width, img_height, x_pos, current_line_y, "  printf(", &font_info, scale, function_r, function_g, function_b); // function color
    x_pos = draw_text(pixels, img_width, img_height, x_pos, current_line_y, "\"Hello, World!\\n\"", &font_info, scale, string_r, string_g, string_b);
    draw_text(pixels, img_width, img_height, x_pos, current_line_y, ");", &font_info, scale, default_text_r, default_text_g, default_text_b);
    current_line_y += line_height;
    
    // Line 5: return 0;
    x_pos = code_block_x + 10 + 20; // Indent 20 pixels
    x_pos = draw_text(pixels, img_width, img_height, x_pos, current_line_y, "  return ", &font_info, scale, keyword_r, keyword_g, keyword_b);
    draw_text(pixels, img_width, img_height, x_pos, current_line_y, "0;", &font_info, scale, literal_r, literal_g, literal_b); // literal color
    current_line_y += line_height;
    
    // Line 6: }
    draw_text(pixels, img_width, img_height, code_block_x + 10, current_line_y, "}", &font_info, scale, default_text_r, default_text_g, default_text_b);


    // --- 6. Save the Image ---
    if (stbi_write_png(output_image_path, img_width, img_height, CHANNELS, pixels, img_width * CHANNELS)) {
        printf("Successfully wrote '%s'\n", output_image_path);
    } else {
        fprintf(stderr, "Failed to write PNG file '%s'!\n", output_image_path);
    }

    // --- Cleanup ---
    free(font_buffer);
    free(pixels);
    free_discovered_fonts(); // Free all dynamically allocated font info
    return 0;
}

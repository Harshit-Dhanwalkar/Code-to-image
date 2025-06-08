#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // For uint8_t
#include <stdbool.h> // For bool
#include <math.h>   // Required for sqrt, floor, ceil, acos, cos, pow, fmod
#include <dirent.h> // Required for directory traversal

// Define STB_IMAGE_WRITE_IMPLEMENTATION and STB_TRUETYPE_IMPLEMENTATION
// in ONE .c file (this one) before including their headers to get the implementations.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h" // Path to stb_image_write.h

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"    // Path to stb_truetype.h

// --- GLOBAL CONSTANT ---
#define CHANNELS 3 // Define CHANNELS as a global constant for RGB images

// --- Data Structures ---

// Structure to hold information about a discovered font
typedef struct {
    char *name; // User-friendly name (e.g., "JetBrainsMono-Regular")
    char *path; // Full path to the .ttf file
} FontInfo;

// Global list of discovered fonts
FontInfo *discovered_fonts = NULL;
int discovered_fonts_count = 0;
int discovered_fonts_capacity = 0;

// --- Helper Functions ---

// Function to convert hex color string to RGB values
// Assumes hex_color is "#RRGGBB"
void hex_to_rgb(const char* hex_color, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!hex_color || strlen(hex_color) != 7 || hex_color[0] != '#') {
        *r = *g = *b = 0; // Default to black on error
        return;
    }
    sscanf(hex_color + 1, "%2hhx%2hhx%2hhx", r, g, b);
}

// Function to draw a single character bitmap onto the main image buffer
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
                int img_idx = (img_py * img_width + img_px) * CHANNELS; // Use CHANNELS constant

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
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

        if (entry->d_type == DT_DIR) { // If it's a directory, recurse
            collect_fonts_recursive(path);
        } else if (entry->d_type == DT_REG) { // If it's a regular file
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".ttf") == 0) {
                // Extract a friendly name from the filename (e.g., "FiraCode-Regular")
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

// Function to load the content of a file into a malloc'ed buffer
char *load_file(const char *filename, size_t *out_size) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    
    long sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        return NULL;
    }
    
    rewind(fp);
    char *buf = malloc(sz + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    
    size_t read_size = fread(buf, 1, sz, fp); 
    buf[read_size] = '\0'; // Null-terminate the buffer
    fclose(fp);
    
    if (out_size) *out_size = read_size;
    return buf;
}

// Function to calculate the required image dimensions based on code content and font
// Added font_pixel_height as a parameter here
void get_code_dimensions(const char* code_buffer, stbtt_fontinfo* font, float scale, float font_pixel_height, float line_spacing_multiplier, int padding, int* out_max_width, int* out_total_height) {
    *out_max_width = 0;
    *out_total_height = 0;

    if (!code_buffer || !font) {
        fprintf(stderr, "DEBUG: get_code_dimensions received null code_buffer or font.\n");
        return;
    }

    int line_count = 0;
    int current_line_char_count = 0; // Number of characters on the current line
    int max_line_char_count = 0; // Maximum number of characters on any line

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
    // Use the vertical metrics to get an accurate line height
    float font_line_height_base = (ascent - descent + lineGap) * scale;

    // Get the advance width of a space character. This is typically the character width for monospace fonts.
    int space_advance_width_units;
    stbtt_GetCodepointHMetrics(font, ' ', &space_advance_width_units, NULL);
    float assumed_char_width = space_advance_width_units * scale;
    
    // Fallback if space has zero width or invalid (e.g., non-displayable characters)
    if (assumed_char_width <= 0) { 
        stbtt_GetCodepointHMetrics(font, 'M', &space_advance_width_units, NULL);
        assumed_char_width = space_advance_width_units * scale;
        if (assumed_char_width <= 0) {
            assumed_char_width = font_pixel_height * 0.6f; // A heuristic fallback using the passed parameter
        }
    }
    fprintf(stderr, "DEBUG: Assumed Char Width for calculation: %.2f pixels\n", assumed_char_width);


    for (size_t i = 0; code_buffer[i] != '\0'; ++i) {
        int codepoint = (unsigned char)code_buffer[i]; // Simple ASCII assumed, handle UTF-8 for real
        if (codepoint == '\n') {
            if (current_line_char_count > max_line_char_count) {
                max_line_char_count = current_line_char_count;
            }
            current_line_char_count = 0;
            line_count++;
        } else if (codepoint == '\t') { // Handle tabs as 4 spaces for width calculation
            current_line_char_count += 4; // Assuming 4 space tab width
        } else {
            current_line_char_count++;
        }
    }
    // Handle the last line if it doesn't end with a newline
    if (current_line_char_count > max_line_char_count) {
        max_line_char_count = current_line_char_count;
    }
    line_count++; // Account for the last line (even empty files have 1 line)

    // Calculate width based on max character count and assumed monospace width
    *out_max_width = (int)(max_line_char_count * assumed_char_width + 0.5f); // Round up to nearest integer
    *out_max_width += 2 * padding; // Add horizontal padding for code block

    *out_total_height = (int)(line_count * font_line_height_base * line_spacing_multiplier + 0.5f); // Round up
    *out_total_height += 2 * padding; // Add vertical padding for code block

    // Ensure minimum dimensions (these should be generous enough for a small snippet)
    if (*out_max_width < (int)(font_pixel_height * 10)) *out_max_width = (int)(font_pixel_height * 10); // Minimum 10 chars wide
    if (*out_total_height < (int)(font_pixel_height * 3)) *out_total_height = (int)(font_pixel_height * 3); // Minimum 3 lines tall

    fprintf(stderr, "DEBUG: Lines: %d, Max Chars per line: %d\n", line_count, max_line_char_count);
    fprintf(stderr, "DEBUG: Calculated Image Dimensions (before user override): %dx%d\n", *out_max_width, *out_total_height);
}


// Print usage help
void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s [options] <output_image_path>\n\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -i FILE    Input code file to convert (e.g., my_script.c)\n"); // New input option
    fprintf(stderr, "  -f FONT    Select font (e.g., 'JetBrainsMono-Regular'). See available fonts below.\n");
    fprintf(stderr, "  -fs SIZE   Set font size in pixels (default: 18.0)\n");
    fprintf(stderr, "  -w WIDTH   Set image width in pixels (default: calculated based on content, or 200 if no content)\n"); // Updated help
    fprintf(stderr, "  -h HEIGHT  Set image height in pixels (default: calculated based on content, or 100 if no content)\n"); // Updated help
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
    const char *input_file_path = NULL; // New variable for input file
    const char *selected_font_name = NULL;
    float font_pixel_height = 18.0f; // This is the definition needed
    int img_width_arg = 0; // Use 0 to indicate not set by user
    int img_height_arg = 0; // Use 0 to indicate not set by user

    // Discover fonts BEFORE parsing arguments so we can list them in usage
    collect_fonts_recursive("Fonts");

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-u") == 0) {
            print_usage(argv[0]);
            free_discovered_fonts();
            return 0; // EXIT IMMEDIATELY AFTER PRINTING HELP
        }
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            input_file_path = argv[++i];
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
            img_width_arg = atoi(argv[++i]);
            if (img_width_arg <= 0) {
                fprintf(stderr, "Error: Image width must be positive.\n");
                free_discovered_fonts();
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            img_height_arg = atoi(argv[++i]);
            if (img_height_arg <= 0) {
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

    // --- Validate Input File and Load Content ---
    char *code_content = NULL;
    size_t code_content_size = 0;
    if (input_file_path) {
        code_content = load_file(input_file_path, &code_content_size);
        if (!code_content) {
            fprintf(stderr, "Error: Could not read input file '%s'.\n", input_file_path);
            free_discovered_fonts();
            return 1;
        }
    } else {
        fprintf(stderr, "Error: No input code file specified. Use -i <filepath>.\n");
        print_usage(argv[0]);
        free_discovered_fonts();
        return 1;
    }


    // --- Font Loading Setup (needed for dimension calculation and drawing) ---
    const char* font_to_load_path = NULL;
    if (selected_font_name == NULL && discovered_fonts_count > 0) {
        font_to_load_path = discovered_fonts[0].path; // Default to the first discovered font
        fprintf(stderr, "No font specified. Defaulting to '%s'.\n", discovered_fonts[0].name);
    } else if (selected_font_name == NULL && discovered_fonts_count == 0) {
         fprintf(stderr, "Error: No fonts found in 'Fonts/' directory. Cannot proceed without a font.\n");
         free(code_content);
         free_discovered_fonts();
         return 1;
    } else {
        // Find the specified font
        for (int i = 0; i < discovered_fonts_count; ++i) {
            if (strcmp(selected_font_name, discovered_fonts[i].name) == 0) {
                font_to_load_path = discovered_fonts[i].path;
                break;
            }
        }
        if (!font_to_load_path) {
            fprintf(stderr, "Error: Specified font '%s' not found.\n", selected_font_name);
            print_usage(argv[0]); // Print usage again if font not found
            free(code_content);
            free_discovered_fonts();
            return 1;
        }
    }

    long font_buffer_size;
    unsigned char* font_buffer = NULL;

    FILE* font_file = fopen(font_to_load_path, "rb");
    if (!font_file) {
        fprintf(stderr, "Error: Could not open font file '%s'. This should not happen if discovered correctly.\n", font_to_load_path);
        free(code_content);
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
        free(code_content);
        free_discovered_fonts();
        return 1;
    }
    fread(font_buffer, 1, font_buffer_size, font_file);
    fclose(font_file);

    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, font_buffer, 0)) {
        fprintf(stderr, "Failed to initialize font from '%s'!\n", font_to_load_path);
        free(font_buffer);
        free(code_content);
        free_discovered_fonts();
        return 1;
    }

    float scale = stbtt_ScaleForPixelHeight(&font_info, font_pixel_height);

    // --- Determine Image Dimensions ---
    int calculated_img_width, calculated_img_height;
    float line_spacing_multiplier = 1.5f; // Matches the constant used in draw loop
    int inner_padding = 20; // Padding inside the code block for text

    get_code_dimensions(code_content, &font_info, scale, font_pixel_height, line_spacing_multiplier, inner_padding, &calculated_img_width, &calculated_img_height);

    // Use user-provided dimensions if available, otherwise use calculated ones
    int img_width = (img_width_arg > 0) ? img_width_arg : calculated_img_width;
    int img_height = (img_height_arg > 0) ? img_height_arg : calculated_img_height;
    
    // Ensure minimums if calculated dimensions are too small or user provides tiny ones
    if (img_width < 200) img_width = 200;
    if (img_height < 100) img_height = 100;


    // Allocate memory for image pixels
    uint8_t *pixels = (uint8_t *)malloc(img_width * img_height * CHANNELS); 
    if (!pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer memory!\n");
        free(font_buffer);
        free(code_content);
        free_discovered_fonts();
        return 1;
    }


    // --- 2. Define Colors (Dracula-like theme for this example) ---
    uint8_t bg_r, bg_g, bg_b;
    uint8_t code_bg_r, code_bg_g, code_bg_b;
    uint8_t default_text_r, default_text_g, default_text_b;
    uint8_t comment_r, comment_g, comment_b;
    uint8_t keyword_r, keyword_g, keyword_b;
    uint8_t function_r, function_g, function_b;
    uint8_t string_r, string_g, string_b;
    uint8_t literal_r, literal_g, literal_b;

    hex_to_rgb("#1a1a1a", &bg_r, &bg_g, &bg_b);         // Dark background
    hex_to_rgb("#0d0d0d", &code_bg_r, &code_bg_g, &code_bg_b); // Even darker for code block
    hex_to_rgb("#f8f8f2", &default_text_r, &default_text_g, &default_text_b); // Foreground (Dracula: #F8F8F2)
    hex_to_rgb("#6272a4", &comment_r, &comment_g, &comment_b); // Comment (Dracula: #6272A4)
    hex_to_rgb("#ff79c6", &keyword_r, &keyword_g, &keyword_b); // Keyword (Dracula: #FF79C6)
    hex_to_rgb("#50fa7b", &function_r, &function_g, &function_b); // Function (Dracula: #50FA7B)
    hex_to_rgb("#f1fa8c", &string_r, &string_g, &string_b);   // String (Dracula: #F1FA8C)
    hex_to_rgb("#ffb86c", &literal_r, &literal_g, &literal_b); // Literal/Number (Dracula: #FFB86C)


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
    int code_block_x = inner_padding;
    int code_block_y = inner_padding;
    int code_block_width = img_width - 2 * inner_padding;
    int code_block_height = img_height - 2 * inner_padding;

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

    // --- 5. Draw Loaded Code Content ---
    int current_line_y = code_block_y + (int)(font_pixel_height * 0.25); // Small offset for first line from top padding
    //float line_spacing_factor = 1.5f; // Already used in get_code_dimensions via parameter
    
    // Recalculate true line height for drawing, using ascent/descent directly
    int ascent_draw, descent_draw, lineGap_draw;
    stbtt_GetFontVMetrics(&font_info, &ascent_draw, &descent_draw, &lineGap_draw);
    float actual_font_line_height = (ascent_draw - descent_draw + lineGap_draw) * scale * line_spacing_multiplier;

    char *line_start = code_content;
    char *line_end;

    // Use a temp buffer for each line to avoid issues with non-null-terminated strncpy
    char temp_line_buffer[2048]; // Increased buffer size for longer lines

    while (true) {
        line_end = strchr(line_start, '\n');
        size_t line_len;

        if (line_end != NULL) {
            line_len = line_end - line_start;
        } else {
            line_len = strlen(line_start);
        }
        
        // Copy line to temporary buffer, ensuring null-termination and bounds check
        if (line_len >= sizeof(temp_line_buffer)) {
            line_len = sizeof(temp_line_buffer) - 1; // Truncate if line is too long
        }
        strncpy(temp_line_buffer, line_start, line_len);
        temp_line_buffer[line_len] = '\0';

        // Draw the line (for now, all in default text color)
        draw_text(pixels, img_width, img_height, code_block_x + 10, current_line_y, temp_line_buffer, &font_info, scale, default_text_r, default_text_g, default_text_b);
        current_line_y += (int)actual_font_line_height;

        if (line_end == NULL) { // Reached end of content
            break;
        }
        line_start = line_end + 1; // Move to the character after newline
    }


    // --- 6. Save the Image ---
    if (stbi_write_png(output_image_path, img_width, img_height, CHANNELS, pixels, img_width * CHANNELS)) {
        printf("Successfully wrote '%s'\n", output_image_path);
    } else {
        fprintf(stderr, "Failed to write PNG file '%s'!\n", output_image_path);
    }

    // --- Cleanup ---
    free(font_buffer);
    free(pixels);
    free(code_content); // Free the loaded code content
    free_discovered_fonts(); // Free all dynamically allocated font info
    return 0;
}

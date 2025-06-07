#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // For uint8_t
#include <stdbool.h> // For bool
#include <math.h>   // Required for sqrt, floor, ceil, acos, cos, pow, fmod

// Define STB_IMAGE_WRITE_IMPLEMENTATION and STB_TRUETYPE_IMPLEMENTATION
// in ONE .c file (this one) before including their headers to get the implementations.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h" // Path to stb_image_write.h

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"    // Path to stb_truetype.h

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
// img_pixels: Pointer to the main image pixel buffer
// img_width, img_height: Dimensions of the main image
// char_pixels: The 8-bit alpha bitmap of the character (from stbtt_MakeCodepointBitmap)
// char_width, char_height: Dimensions of the character bitmap
// draw_x, draw_y: Top-left position on the main image where to draw the char
// r, g, b: Color of the character
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
                // Get alpha value from character bitmap (0-255)
                uint8_t alpha = char_pixels[cy * char_width + cx];

                // Calculate current pixel index in the main image
                int img_idx = (img_py * img_width + img_px) * 3; // 3 channels (RGB)

                // Simple alpha blending: foreground (char) over background (existing pixel)
                // New = alpha * FG + (1 - alpha) * BG
                // Since char_pixels is 8-bit alpha, alpha is 0-255. Divide by 255.0 for blending.
                float alpha_norm = alpha / 255.0f;

                img_pixels[img_idx + 0] = (uint8_t)(alpha_norm * r + (1.0f - alpha_norm) * img_pixels[img_idx + 0]);
                img_pixels[img_idx + 1] = (uint8_t)(alpha_norm * g + (1.0f - alpha_norm) * img_pixels[img_idx + 1]);
                img_pixels[img_idx + 2] = (uint8_t)(alpha_norm * b + (1.0f - alpha_norm) * img_pixels[img_idx + 2]);
            }
        }
    }
}

// Function to draw text string onto the image buffer and return the end x-position
// This allows chaining drawing calls for segments on the same line.
int draw_text(uint8_t* img_pixels, int img_width, int img_height,
               int start_x, int start_y, const char* text,
               stbtt_fontinfo* font, float scale, uint8_t r, uint8_t g, uint8_t b) {

    int x_cursor = start_x;

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
    int baseline = (int)(ascent * scale);

    for (int i = 0; text[i]; ++i) {
        // Simple ASCII assumed, handle UTF-8 for real-world scenarios
        // For UTF-8, you'd use stbtt_FindNextCodepoint and stbtt_GetCodepointFromUTF8
        int codepoint = (unsigned char)text[i]; 

        // Get character bitmap
        int char_width, char_height, x_offset, y_offset;
        uint8_t* char_bitmap = stbtt_GetCodepointBitmap(font, 0, scale, codepoint, &char_width, &char_height, &x_offset, &y_offset);

        if (char_bitmap) {
            // Calculate drawing position (x_offset, y_offset are relative to baseline)
            int draw_x = x_cursor + x_offset;
            int draw_y = start_y + baseline + y_offset; // Adjust y based on baseline and bitmap offset

            draw_char_bitmap(img_pixels, img_width, img_height,
                             char_bitmap, char_width, char_height,
                             draw_x, draw_y, r, g, b);

            free(char_bitmap); // Use standard free, as stbtt_GetCodepointBitmap uses stbtt_malloc (which defaults to malloc)
        }

        // Advance cursor position for next character
        int advance_width;
        stbtt_GetCodepointHMetrics(font, codepoint, &advance_width, NULL);
        x_cursor += (int)(advance_width * scale);

        // Add kerning (if applicable, for a production system)
        // int kern = stbtt_GetCodepointKernAdvance(font, codepoint, (unsigned char)text[i+1]);
        // x_cursor += (int)(kern * scale);
    }
    return x_cursor; // Return the new x_cursor position
}

int main() {
    const int WIDTH = 800;  // Image width
    const int HEIGHT = 600; // Image height
    const int CHANNELS = 3; // RGB channels (no alpha for PNG output, but alpha used internally for blending)

    // Allocate memory for image pixels (RGB: R, G, B for each pixel)
    uint8_t *pixels = (uint8_t *)malloc(WIDTH * HEIGHT * CHANNELS);
    if (!pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer memory!\n");
        return 1;
    }

    // --- 1. Load Font ---
    // Ensure this path is correct for your system!
    // Example path: "Fonts/JetBrainsMono-2.304/ttf/JetBrainsMono-Regular.ttf"
    const char* font_path = "Fonts/JetBrainsMono-2.304/fonts/ttf/JetBrainsMonoNL-Regular.ttf"; 
    long font_size;
    unsigned char* font_buffer = NULL;

    FILE* font_file = fopen(font_path, "rb");
    if (!font_file) {
        fprintf(stderr, "Error: Could not open font file '%s'. Make sure it's in the correct relative path.\n", font_path);
        free(pixels);
        return 1;
    }

    fseek(font_file, 0, SEEK_END);
    font_size = ftell(font_file);
    fseek(font_file, 0, SEEK_SET);

    font_buffer = (unsigned char*)malloc(font_size);
    if (!font_buffer) {
        fprintf(stderr, "Failed to allocate font buffer memory!\n");
        fclose(font_file);
        free(pixels);
        return 1;
    }
    fread(font_buffer, 1, font_size, font_file);
    fclose(font_file);

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, font_buffer, 0)) {
        fprintf(stderr, "Failed to initialize font!\n");
        free(font_buffer);
        free(pixels);
        return 1;
    }

    float font_pixel_height = 18.0f; // Height of the font in pixels
    float scale = stbtt_ScaleForPixelHeight(&font, font_pixel_height);


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
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            int index = (y * WIDTH + x) * CHANNELS;
            pixels[index + 0] = bg_r;
            pixels[index + 1] = bg_g;
            pixels[index + 2] = bg_b;
        }
    }

    // --- 4. Draw Code Block Background ---
    int padding = 20;
    int code_block_x = padding;
    int code_block_y = padding;
    int code_block_width = WIDTH - 2 * padding;
    int code_block_height = HEIGHT - 2 * padding;

    for (int y = code_block_y; y < code_block_y + code_block_height; ++y) {
        for (int x = code_block_x; x < code_block_x + code_block_width; ++x) {
            if (x >=0 && x < WIDTH && y >= 0 && y < HEIGHT) { // Safety check
                int index = (y * WIDTH + x) * CHANNELS;
                pixels[index + 0] = code_bg_r;
                pixels[index + 1] = code_bg_g;
                pixels[index + 2] = code_bg_b;
            }
        }
    }

    // --- 5. Draw Sample Code Lines (using the corrected chaining logic) ---
    // In a real application, you'd feed the output of Tree-sitter here
    int current_line_y = code_block_y + 10;
    int line_height = (int)(font_pixel_height * 1.5); // Approx line spacing

    // Line 1: // C Code Example:
    draw_text(pixels, WIDTH, HEIGHT, code_block_x + 10, current_line_y, "// C Code Example:", &font, scale, comment_r, comment_g, comment_b);
    current_line_y += line_height;

    // Line 2: #include <stdio.h>
    int x_pos = code_block_x + 10;
    x_pos = draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, "#include ", &font, scale, keyword_r, keyword_g, keyword_b);
    x_pos = draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, "<stdio.h>", &font, scale, string_r, string_g, string_b);
    draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, " // Include standard I/O", &font, scale, comment_r, comment_g, comment_b);
    current_line_y += line_height;

    // Line 3: int main() {
    x_pos = code_block_x + 10;
    x_pos = draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, "int ", &font, scale, keyword_r, keyword_g, keyword_b);
    x_pos = draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, "main", &font, scale, function_r, function_g, function_b); // function color
    draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, "() {", &font, scale, default_text_r, default_text_g, default_text_b);
    current_line_y += line_height;

    // Line 4: printf("Hello, World!\n");
    x_pos = code_block_x + 10 + 20; // Indent 20 pixels
    x_pos = draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, "  printf(", &font, scale, function_r, function_g, function_b); // function color
    x_pos = draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, "\"Hello, World!\\n\"", &font, scale, string_r, string_g, string_b);
    draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, ");", &font, scale, default_text_r, default_text_g, default_text_b);
    current_line_y += line_height;
    
    // Line 5: return 0;
    x_pos = code_block_x + 10 + 20; // Indent 20 pixels
    x_pos = draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, "  return ", &font, scale, keyword_r, keyword_g, keyword_b);
    draw_text(pixels, WIDTH, HEIGHT, x_pos, current_line_y, "0;", &font, scale, literal_r, literal_g, literal_b); // literal color
    current_line_y += line_height;
    
    // Line 6: }
    draw_text(pixels, WIDTH, HEIGHT, code_block_x + 10, current_line_y, "}", &font, scale, default_text_r, default_text_g, default_text_b);


    // --- 6. Save the Image ---
    if (stbi_write_png("highlighted_code.png", WIDTH, HEIGHT, CHANNELS, pixels, WIDTH * CHANNELS)) {
        printf("Successfully wrote highlighted_code.png\n");
    } else {
        fprintf(stderr, "Failed to write PNG file!\n");
    }

    // --- Cleanup ---
    free(font_buffer);
    free(pixels);
    return 0;
}

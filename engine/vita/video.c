/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * Licensed under a BSD-style license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2017 OpenBOR Team
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <vita2d.h>
#include <source/gamelib/types.h>
#include <source/savedata.h>
#include "types.h"
#include "globals.h"
#include "video.h"

#include "shader.h"

//static vita2d_texture *vitaTexture[2] = {NULL, NULL};
static vita2d_shader *vita2d_shaders[4];
static unsigned char vitaPalette[4*256];
static int vitaBrightness = 0;
static unsigned char vitaBytesPerPixel = 1;

static void setPalette(void);

void video_init(void)
{
    vita2d_init();
    vita2d_set_vblank_wait(1);
	//vita2d_set_clear_color(RGBA8(0, 0, 0, 0xFF));
    vita2d_texture_set_alloc_memblock_type(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW);

	memset(vitaPalette, 0, sizeof(vitaPalette));

	// texture
    vita2d_shaders[0] = vita2d_create_shader((SceGxmProgram *) texture_v, (SceGxmProgram *) texture_f);
    // lcd3x
    vita2d_shaders[1] = vita2d_create_shader((SceGxmProgram *) lcd3x_v, (SceGxmProgram *) lcd3x_f);
    // sharp + scanlines
    vita2d_shaders[2] = vita2d_create_shader((SceGxmProgram *) sharp_bilinear_v, (SceGxmProgram *) sharp_bilinear_f);
    // sharp
    vita2d_shaders[3] = vita2d_create_shader((SceGxmProgram *) sharp_bilinear_simple_v, (SceGxmProgram *) sharp_bilinear_simple_f);
}

void video_exit(void)
{
    // wait for the GPU to finish rendering so we can free everything
	vita2d_fini();

	for(int i=0; i<4; i++) {
	    vita2d_free_shader(vita2d_shaders[i]);
	}

	//if (vitaTexture[0]) vita2d_free_texture(vitaTexture[0]);
	//if (vitaTexture[1]) vita2d_free_texture(vitaTexture[1]);
	//vitaTexture[0] = vitaTexture[1] = NULL;
}

int video_set_mode(s_videomodes videomodes) //(int width, int height, int bytes_per_pixel)
{
    printf("video_set_mode: %i x %i | scale: %f x %f | bpp: %i\n",
           videomodes.hRes, videomodes.vRes,
           videomodes.hScale, videomodes.vScale, videomodes.pixel);

    for(int i=0; i<10; i++) {
        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_end_drawing();
        vita2d_wait_rendering_done();
        vita2d_swap_buffers();
    }

    /*
    vitaBytesPerPixel = videomodes.pixel;

    // wait for rendering to finish before freeing textures; otherwise the GPU will hang
    vita2d_wait_rendering_done();

    // free existing textures
    if (vitaTexture[0]) vita2d_free_texture(vitaTexture[0]);
    if (vitaTexture[1]) vita2d_free_texture(vitaTexture[1]);

    // clear the screen
    vita2d_start_drawing();
	vita2d_clear_screen();
	vita2d_end_drawing();

    if (videomodes.hRes == 0 && videomodes.vRes == 0)
    {
        // deinitialize video
        vitaTexture[0] = vitaTexture[1] = NULL;
        return 1;
    }

    // determine texture format
    SceGxmTextureFormat format = SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR;
    switch (videomodes.pixel)
    {
        case 1: format = SCE_GXM_TEXTURE_FORMAT_P8_1BGR; break;
        case 2: format = SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR; break;
        case 4: format = SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR; break;
        default: printf("ERROR: unknown texture format!\n");
    }

    // init the textures
    vitaTexture[0] = vita2d_create_empty_texture_format(videomodes.hRes, videomodes.vRes, format);
    vitaTexture[1] = vita2d_create_empty_texture_format(videomodes.hRes, videomodes.vRes, format);

    // set texture palette for 8-bit color mode
    if (videomodes.pixel == 1)
    {
        setPalette();
    }
    */

    return 1;
}

int video_copy_screen(s_screen *screen)
{
    /*
    static int whichTexture = 0;
    whichTexture = !whichTexture;
    vita2d_texture *targetTexture = vitaTexture[whichTexture];
    unsigned int stride = vita2d_texture_get_stride(screen->texture);
    */

    unsigned int texWidth = vita2d_texture_get_width(screen->texture),
                 texHeight = vita2d_texture_get_height(screen->texture);

    /*
    if (stride == texWidth * vitaBytesPerPixel)
    {
        //memcpy(vita2d_texture_get_datap(targetTexture), screen->data, stride * texHeight);
    }
    else
    {
        uint8_t *srcLine = screen->data;
        uint8_t *dstLine = vita2d_texture_get_datap(targetTexture);
        int i;
        for (i = 0; i < texHeight; i++)
        {
            memcpy(dstLine, srcLine, texWidth * vitaBytesPerPixel);
            dstLine += stride;
            srcLine += texWidth * vitaBytesPerPixel;
        }
    }
    */

    // determine scale factor and on-screen dimensions
	float scaleFactor = 960.0f / texWidth;
	if (544.0 / texHeight < scaleFactor) scaleFactor = 544.0f / texHeight;

	// determine offsets
	float xOffset = (960.0f - texWidth * scaleFactor) / 2.0f;
	float yOffset = (544.0f - texHeight * scaleFactor) / 2.0f;

    // set filtering mode
    SceGxmTextureFilter filter = savedata.hwfilter ?
                 SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
    vita2d_texture_set_filters(screen->texture, filter, filter);

	vita2d_start_drawing();

	if (vitaBrightness < 0)
	{
	    vita2d_draw_texture_tint_scale(screen->texture, xOffset, yOffset, scaleFactor, scaleFactor,
                                       RGBA8(255, 255, 255, 255 + vitaBrightness));
	}
	else
	{
        vita2d_draw_texture_with_shader(vita2d_shaders[savedata.shader],
                                        screen->texture, xOffset, yOffset, scaleFactor, scaleFactor);
	}

	vita2d_end_drawing();
    vita2d_wait_rendering_done();
	vita2d_swap_buffers();

    return 1;
}

// TODO gamma
void video_set_color_correction(int gamma, int brightness)
{
    vitaBrightness = (brightness < -255) ? -255 : brightness;
}

static void setPalette(void)
{
    /*
    if (vitaBytesPerPixel != 1 || !vitaTexture[0] || !vitaTexture[1]) return;

    int i;
    uint32_t *texturePalette0 = vita2d_texture_get_palette(vitaTexture[0]),
             *texturePalette1 = vita2d_texture_get_palette(vitaTexture[1]);

    vita2d_wait_rendering_done();

    for (i = 0; i < 256; i++)
    {
	    uint32_t color = vitaPalette[i*3] | (vitaPalette[i*3+1] << 8) | (vitaPalette[i*3+2] << 16);
	    texturePalette0[i] = texturePalette1[i] = color;
    }
    */
}

void vga_setpalette(unsigned char* pal)
{
    printf("vga_setpalette\n");
    if (memcmp(pal, vitaPalette, PAL_BYTES) != 0)
    {
        memcpy(vitaPalette, pal, PAL_BYTES);
        setPalette();
	}
}

// no-op because this function is a useless DOS artifact
void vga_vwait(void)
{
}

// not sure if this is useful for anything
void video_clearscreen(void)
{
}

/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gl_vidxbox.c -- NXDK pbGL vid component

#include "quakedef.h"
#include "cfgfile.h"
#include "bgmusic.h"
#include "resource.h"

#include <stdint.h>
#include <hal/video.h>
#include <hal/debug.h>
#include <pbgl.h>
#include <pbkit/pbkit.h>
#include <windows.h>

#define MAX_MODE_LIST	16
#define MAX_BPPS_LIST	5
#define MAX_RATES_LIST	20
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		1920
#define MAXHEIGHT		1080

typedef struct {
	int			width;
	int			height;
	int			refreshrate;
	int			bpp;
} vmode_t;

static const char *gl_vendor;
static const char *gl_renderer;
static const char *gl_version;
static int gl_version_major;
static int gl_version_minor;
static const char *gl_extensions;

static VIDEO_MODE xbox_vidmode;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;

static qboolean	vid_initialized = false;

static qboolean	vid_locked = false; //johnfitz
static qboolean	vid_changed = false;

static void VID_Menu_Init (void); //johnfitz
static void VID_Menu_f (void); //johnfitz
static void VID_MenuDraw (void);
static void VID_MenuKey (int key);

static void ClearAllStates (void);
static void GL_Init (void);
static void GL_SetupState (void); //johnfitz

viddef_t	vid;				// global video state
modestate_t	modestate = MS_UNINIT;
qboolean	scr_skipupdate;

qboolean gl_mtexable = false;
qboolean gl_packed_pixels = false;
qboolean gl_texture_env_combine = false; //johnfitz
qboolean gl_texture_env_add = false; //johnfitz
qboolean gl_paletted_texture = false;
qboolean gl_swap_control = false; //johnfitz
qboolean gl_anisotropy_able = false; //johnfitz
float gl_max_anisotropy; //johnfitz
qboolean gl_texture_NPOT = false; //ericw
qboolean gl_vbo_able = false; //ericw
qboolean gl_glsl_able = false; //ericw
GLint gl_max_texture_units = 0; //ericw
qboolean gl_glsl_gamma_able = false; //ericw
qboolean gl_glsl_alias_able = false; //ericw
int gl_stencilbits;

PFNGLMULTITEXCOORD2FARBPROC GL_MTexCoord2fFunc = NULL; //johnfitz
PFNGLACTIVETEXTUREARBPROC GL_SelectTextureFunc = NULL; //johnfitz
PFNGLCLIENTACTIVETEXTUREARBPROC GL_ClientActiveTextureFunc = NULL; //ericw
PFNGLBINDBUFFERARBPROC GL_BindBufferFunc = NULL; //ericw
PFNGLBUFFERDATAARBPROC GL_BufferDataFunc = NULL; //ericw
PFNGLBUFFERSUBDATAARBPROC GL_BufferSubDataFunc = NULL; //ericw
PFNGLDELETEBUFFERSARBPROC GL_DeleteBuffersFunc = NULL; //ericw
PFNGLGENBUFFERSARBPROC GL_GenBuffersFunc = NULL; //ericw

QS_PFNGLCREATESHADERPROC GL_CreateShaderFunc = NULL; //ericw
QS_PFNGLDELETESHADERPROC GL_DeleteShaderFunc = NULL; //ericw
QS_PFNGLDELETEPROGRAMPROC GL_DeleteProgramFunc = NULL; //ericw
QS_PFNGLSHADERSOURCEPROC GL_ShaderSourceFunc = NULL; //ericw
QS_PFNGLCOMPILESHADERPROC GL_CompileShaderFunc = NULL; //ericw
QS_PFNGLGETSHADERIVPROC GL_GetShaderivFunc = NULL; //ericw
QS_PFNGLGETSHADERINFOLOGPROC GL_GetShaderInfoLogFunc = NULL; //ericw
QS_PFNGLGETPROGRAMIVPROC GL_GetProgramivFunc = NULL; //ericw
QS_PFNGLGETPROGRAMINFOLOGPROC GL_GetProgramInfoLogFunc = NULL; //ericw
QS_PFNGLCREATEPROGRAMPROC GL_CreateProgramFunc = NULL; //ericw
QS_PFNGLATTACHSHADERPROC GL_AttachShaderFunc = NULL; //ericw
QS_PFNGLLINKPROGRAMPROC GL_LinkProgramFunc = NULL; //ericw
QS_PFNGLBINDATTRIBLOCATIONFUNC GL_BindAttribLocationFunc = NULL; //ericw
QS_PFNGLUSEPROGRAMPROC GL_UseProgramFunc = NULL; //ericw
QS_PFNGLGETATTRIBLOCATIONPROC GL_GetAttribLocationFunc = NULL; //ericw
QS_PFNGLVERTEXATTRIBPOINTERPROC GL_VertexAttribPointerFunc = NULL; //ericw
QS_PFNGLENABLEVERTEXATTRIBARRAYPROC GL_EnableVertexAttribArrayFunc = NULL; //ericw
QS_PFNGLDISABLEVERTEXATTRIBARRAYPROC GL_DisableVertexAttribArrayFunc = NULL; //ericw
QS_PFNGLGETUNIFORMLOCATIONPROC GL_GetUniformLocationFunc = NULL; //ericw
QS_PFNGLUNIFORM1IPROC GL_Uniform1iFunc = NULL; //ericw
QS_PFNGLUNIFORM1FPROC GL_Uniform1fFunc = NULL; //ericw
QS_PFNGLUNIFORM3FPROC GL_Uniform3fFunc = NULL; //ericw
QS_PFNGLUNIFORM4FPROC GL_Uniform4fFunc = NULL; //ericw

QS_PFNGENERATEMIPMAP GL_GenerateMipmap = NULL;

QS_PFNGLCOLORTABLEEXTPROC GL_ColorTableFunc = NULL;

//====================================

//johnfitz -- new cvars
static cvar_t	vid_fullscreen = {"vid_fullscreen", "0", CVAR_ARCHIVE};	// QuakeSpasm, was "1"
static cvar_t	vid_width = {"vid_width", "640", CVAR_ARCHIVE};		// QuakeSpasm, was 640
static cvar_t	vid_height = {"vid_height", "480", CVAR_ARCHIVE};	// QuakeSpasm, was 480
static cvar_t	vid_bpp = {"vid_bpp", "32", CVAR_ARCHIVE};
static cvar_t	vid_refreshrate = {"vid_refreshrate", "60", CVAR_ARCHIVE};
static cvar_t	vid_vsync = {"vid_vsync", "1", CVAR_ARCHIVE};
//johnfitz

cvar_t		vid_gamma = {"gamma", "1", CVAR_ARCHIVE}; //johnfitz -- moved here from view.c
cvar_t		vid_contrast = {"contrast", "1", CVAR_ARCHIVE}; //QuakeSpasm, MarkV

//==========================================================================
//
//  HARDWARE GAMMA -- johnfitz
//
//==========================================================================

static qboolean	gammaworks = false;	// whether hw-gamma works

/*
================
VID_Gamma_SetGamma -- apply gamma correction
================
*/
static void VID_Gamma_SetGamma (void)
{

}

/*
================
VID_Gamma_Restore -- restore system gamma
================
*/
static void VID_Gamma_Restore (void)
{

}

/*
================
VID_Gamma_Shutdown -- called on exit
================
*/
static void VID_Gamma_Shutdown (void)
{
	VID_Gamma_Restore ();
}

/*
================
VID_Gamma_f -- callback when the cvar changes
================
*/
static void VID_Gamma_f (cvar_t *var)
{

}

/*
================
VID_Gamma_Init -- call on init
================
*/
static void VID_Gamma_Init (void)
{
	Cvar_RegisterVariable (&vid_gamma);
	Cvar_RegisterVariable (&vid_contrast);
	Cvar_SetCallback (&vid_gamma, VID_Gamma_f);
	Cvar_SetCallback (&vid_contrast, VID_Gamma_f);

	if (gl_glsl_gamma_able)
		return;

	gammaworks = false;

	if (!gammaworks)
		Con_SafePrintf("gamma adjustment not available\n");
}

/*
======================
VID_GetCurrentWidth
======================
*/
static int VID_GetCurrentWidth (void)
{
	return xbox_vidmode.width;
}

/*
=======================
VID_GetCurrentHeight
=======================
*/
static int VID_GetCurrentHeight (void)
{
	return xbox_vidmode.height;
}

/*
====================
VID_GetCurrentRefreshRate
====================
*/
static int VID_GetCurrentRefreshRate (void)
{
	return xbox_vidmode.refresh;
}


/*
====================
VID_GetCurrentBPP
====================
*/
static int VID_GetCurrentBPP (void)
{
	return xbox_vidmode.bpp;
}

/*
====================
VID_GetFullscreen
 
returns true if we are in regular fullscreen or "desktop fullscren"
====================
*/
static qboolean VID_GetFullscreen (void)
{
	return true;
}

/*
====================
VID_GetDesktopFullscreen
 
returns true if we are specifically in "desktop fullscreen" mode
====================
*/
static qboolean VID_GetDesktopFullscreen (void)
{
	return false;
}

/*
====================
VID_GetVSync
====================
*/
static qboolean VID_GetVSync (void)
{
	return pbgl_get_swap_interval() != 0;
}

/*
====================
VID_GetWindow

used by pl_win.c
====================
*/
void *VID_GetWindow (void)
{
	return NULL;
}

/*
====================
VID_HasMouseOrInputFocus
====================
*/
qboolean VID_HasMouseOrInputFocus (void)
{
	return true;
}

/*
====================
VID_IsMinimized
====================
*/
qboolean VID_IsMinimized (void)
{
	return false;
}

/*
================
VID_ValidMode
================
*/
static qboolean VID_ValidMode (int width, int height, int refreshrate, int bpp, qboolean fullscreen)
{
	VIDEO_MODE vm;
	void *ptr = NULL;

	/* TODO: 16bpp vidmode support */
	if (!fullscreen || (bpp != 32 && bpp != 0))
		return false;

	while (XVideoListModes(&vm, bpp, refreshrate, &ptr))
	{
		if (vm.width == width && vm.height == height &&
				(vm.refresh == refreshrate || refreshrate == 0) &&
				(vm.bpp == bpp || bpp == 0))
			return true;
	}

	return true;
}

/*
================
VID_SetMode
================
*/
static qboolean VID_SetMode (int width, int height, int refreshrate, int bpp, qboolean fullscreen)
{
	int		temp, err;
	Uint32	flags;
	int		depthbits, stencilbits;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();
	BGM_Pause ();

	/* this won't do anything if pbgl wasn't initialized */
	pbgl_shutdown();

	/* z-buffer depth */
	/* TODO: other zstencil formats */
	depthbits = 24;
	stencilbits = 8;

	if (!XVideoSetMode(width, height, bpp, refreshrate))
		Sys_Error ("Couldn't set video mode %dx%dx%d@%02d (%d)", width, height, bpp, refreshrate, fullscreen);

	xbox_vidmode = XVideoGetMode();

	if ((err = pbgl_init(GL_TRUE)) < 0)
		Sys_Error ("Couldn't init pbGL: error %d", err);

	pbgl_set_swap_interval((vid_vsync.value) ? 1 : 0);

	vid.width = VID_GetCurrentWidth();
	vid.height = VID_GetCurrentHeight();
	vid.conwidth = vid.width & 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
	vid.numpages = 2;

	gl_stencilbits = stencilbits; /* TODO: other zstencil formats */

	modestate = VID_GetFullscreen() ? MS_FULLSCREEN : MS_WINDOWED;

	CDAudio_Resume ();
	BGM_Resume ();
	scr_disabled_for_loading = temp;

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	Con_SafePrintf ("Video mode %dx%dx%d %dHz (%d-bit z-buffer) initialized\n",
				VID_GetCurrentWidth(),
				VID_GetCurrentHeight(),
				VID_GetCurrentBPP(),
				VID_GetCurrentRefreshRate(),
				depthbits);

	vid.recalc_refdef = 1;

// no pending changes
	vid_changed = false;

	return true;
}

/*
===================
VID_Changed_f -- kristian -- notify us that a value has changed that requires a vid_restart
===================
*/
static void VID_Changed_f (cvar_t *var)
{
	vid_changed = true;
}

/*
===================
VID_Restart -- johnfitz -- change video modes on the fly
===================
*/
static void VID_Restart (void)
{
	int width, height, refreshrate, bpp;
	qboolean fullscreen;

	if (vid_locked || !vid_changed)
		return;

	width = (int)vid_width.value;
	height = (int)vid_height.value;
	refreshrate = (int)vid_refreshrate.value;
	bpp = (int)vid_bpp.value;
	fullscreen = vid_fullscreen.value ? true : false;

//
// validate new mode
//
	if (!VID_ValidMode (width, height, refreshrate, bpp, fullscreen))
	{
		Con_Printf ("%dx%dx%d %dHz %s is not a valid mode\n",
				width, height, bpp, refreshrate, fullscreen? "fullscreen" : "windowed");
		return;
	}

// ericw -- OS X, SDL1: textures, VBO's invalid after mode change
//          OS X, SDL2: still valid after mode change
// To handle both cases, delete all GL objects (textures, VBO, GLSL) now.
// We must not interleave deleting the old objects with creating new ones, because
// one of the new objects could be given the same ID as an invalid handle
// which is later deleted.

	TexMgr_DeleteTextureObjects ();
	GLSLGamma_DeleteTexture ();
	R_ScaleView_DeleteTexture ();
	R_DeleteShaders ();
	GL_DeleteBModelVertexBuffer ();
	GLMesh_DeleteVertexBuffers ();

//
// set new mode
//
	VID_SetMode (width, height, refreshrate, bpp, fullscreen);

	GL_Init ();
	TexMgr_ReloadImages ();
	GL_BuildBModelVertexBuffer ();
	GLMesh_LoadVertexBuffers ();
	GL_SetupState ();
	Fog_SetupState ();

	//warpimages needs to be recalculated
	TexMgr_RecalcWarpImageSize ();

	//conwidth and conheight need to be recalculated
	vid.conwidth = (scr_conwidth.value > 0) ? (int)scr_conwidth.value : (scr_conscale.value > 0) ? (int)(vid.width/scr_conscale.value) : vid.width;
	vid.conwidth = CLAMP (320, vid.conwidth, vid.width);
	vid.conwidth &= 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
//
// keep cvars in line with actual mode
//
	VID_SyncCvars();
//
// update mouse grab
//
	if (key_dest == key_console || key_dest == key_menu)
	{
		if (modestate == MS_WINDOWED)
			IN_Deactivate(true);
		else if (modestate == MS_FULLSCREEN)
			IN_Activate();
	}
}

/*
================
VID_Test -- johnfitz -- like vid_restart, but asks for confirmation after switching modes
================
*/
static void VID_Test (void)
{
	int old_width, old_height, old_refreshrate, old_bpp, old_fullscreen;

	if (vid_locked || !vid_changed)
		return;
//
// now try the switch
//
	old_width = VID_GetCurrentWidth();
	old_height = VID_GetCurrentHeight();
	old_refreshrate = VID_GetCurrentRefreshRate();
	old_bpp = VID_GetCurrentBPP();
	old_fullscreen = VID_GetFullscreen() ? true : false;

	VID_Restart ();

	//pop up confirmation dialoge
	if (!SCR_ModalMessage("Would you like to keep this\nvideo mode? (y/n)\n", 5.0f))
	{
		//revert cvars and mode
		Cvar_SetValueQuick (&vid_width, old_width);
		Cvar_SetValueQuick (&vid_height, old_height);
		Cvar_SetValueQuick (&vid_refreshrate, old_refreshrate);
		Cvar_SetValueQuick (&vid_bpp, old_bpp);
		Cvar_SetQuick (&vid_fullscreen, old_fullscreen ? "1" : "0");
		VID_Restart ();
	}
}

/*
================
VID_Unlock -- johnfitz
================
*/
static void VID_Unlock (void)
{
	vid_locked = false;
	VID_SyncCvars();
}

/*
================
VID_Lock -- ericw

Subsequent changes to vid_* mode settings, and vid_restart commands, will
be ignored until the "vid_unlock" command is run.

Used when changing gamedirs so the current settings override what was saved
in the config.cfg.
================
*/
void VID_Lock (void)
{
	vid_locked = true;
}

//==============================================================================
//
//	OPENGL STUFF
//
//==============================================================================

/*
===============
GL_Info_f -- johnfitz
===============
*/
static void GL_Info_f (void)
{
	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);
}

/*
===============
GL_CheckExtensions
===============
*/
static qboolean GL_ParseExtensionList (const char *list, const char *name)
{
	const char	*start;
	const char	*where, *terminator;

	if (!list || !name || !*name)
		return false;
	if (strchr(name, ' ') != NULL)
		return false;	// extension names must not have spaces

	start = list;
	while (1) {
		where = strstr (start, name);
		if (!where)
			break;
		terminator = where + strlen (name);
		if (where == start || where[-1] == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return true;
		start = terminator;
	}
	return false;
}

static void GL_CheckExtensions (void)
{
	int swap_control;

	// ARB_vertex_buffer_object
	//
	Con_Warning ("OpenGL version < 1.5, skipping ARB_vertex_buffer_object check\n");
	gl_vbo_able = false;

	// multitexture
	//
	if (COM_CheckParm("-nomtex"))
		Con_Warning ("Mutitexture disabled at command line\n");
	else
	{
		GL_MTexCoord2fFunc = glMultiTexCoord2f;
		GL_SelectTextureFunc = glActiveTexture;
		GL_ClientActiveTextureFunc = glClientActiveTexture;
		Con_Printf("FOUND: ARB_multitexture\n");
		gl_mtexable = true;
		glGetIntegerv(GL_MAX_TEXTURE_UNITS, &gl_max_texture_units);
		Con_Printf("GL_MAX_TEXTURE_UNITS: %d\n", (int)gl_max_texture_units);
	}

	// texture_env_combine
	//
	if (COM_CheckParm("-nocombine"))
		Con_Warning ("texture_env_combine disabled at command line\n");
	else
	{
		Con_Printf("FOUND: ARB_texture_env_combine\n");
		gl_texture_env_combine = true;
	}

	// texture_env_add
	//
	if (COM_CheckParm("-noadd"))
		Con_Warning ("texture_env_add disabled at command line\n");
	else
	{
		Con_Printf("FOUND: ARB_texture_env_add\n");
		gl_texture_env_add = true;
	}

	// texture_palette
	//
	if (COM_CheckParm("-nopalette"))
		Con_Warning ("paletted_texture disabled at command line\n");
	else
	{
		Con_Printf("FOUND: EXT_paletted_texture\n");
		gl_paletted_texture = true;
		GL_ColorTableFunc = glColorTableEXT;
	}

	// swap control
	//
	gl_swap_control = true;

	// anisotropic filtering
	//
	gl_max_anisotropy = 1;
	Con_Warning ("texture_filter_anisotropic not supported\n");

	// texture_non_power_of_two
	//
	if (COM_CheckParm("-notexturenpot"))
		Con_Warning ("texture_non_power_of_two disabled at command line\n");
	else if (GL_ParseExtensionList(gl_extensions, "GL_ARB_texture_non_power_of_two"))
	{
		Con_Printf("FOUND: ARB_texture_non_power_of_two\n");
		gl_texture_NPOT = true;
	}
	else
	{
		Con_Warning ("texture_non_power_of_two not supported\n");
	}

	// GLSL
	//
	Con_Warning ("GLSL not available\n");

	// glGenerateMipmap for warp textures
	// disabled for now so they don't take up extra memory
	if (COM_CheckParm("-nowarpmipmaps"))
		Con_Warning ("glGenerateMipmap disabled at command line\n");
	else
	{
		GL_GenerateMipmap = glGenerateMipmap;
		Con_Printf ("FOUND: glGenerateMipmap\n");
	}
}

/*
===============
GL_SetupState -- johnfitz

does all the stuff from GL_Init that needs to be done every time a new GL render context is created
===============
*/
static void GL_SetupState (void)
{
	glClearColor (0.15,0.15,0.15,0); //johnfitz -- originally 1,0,0,0
	glCullFace(GL_BACK); //johnfitz -- glquake used CCW with backwards culling -- let's do it right
	glFrontFace(GL_CW); //johnfitz -- glquake used CCW with backwards culling -- let's do it right
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);
	glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); //johnfitz
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glDepthRange (0, 1); //johnfitz -- moved here becuase gl_ztrick is gone.
	glDepthFunc (GL_LEQUAL); //johnfitz -- moved here becuase gl_ztrick is gone.
}

/*
===============
GL_Init
===============
*/
static void GL_Init (void)
{
	gl_vendor = (const char *) glGetString (GL_VENDOR);
	gl_renderer = (const char *) glGetString (GL_RENDERER);
	gl_version = (const char *) glGetString (GL_VERSION);
	gl_extensions = (const char *) glGetString (GL_EXTENSIONS);

	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);
	
	if (gl_version == NULL || sscanf(gl_version, "%d.%d", &gl_version_major, &gl_version_minor) < 2)
	{
		gl_version_major = 0;
		gl_version_minor = 0;
	}

	GL_CheckExtensions (); //johnfitz
}

/*
=================
GL_BeginRendering -- sets values of glx, gly, glwidth, glheight
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = vid.width;
	*height = vid.height;
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering (void)
{
	if (!scr_skipupdate)
	{
		pbgl_swap_buffers();
	}
}


void	VID_Shutdown (void)
{
	if (vid_initialized)
	{
		VID_Gamma_Shutdown (); //johnfitz
		pbgl_shutdown ();
		PL_VID_Shutdown ();
	}
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
static void ClearAllStates (void)
{
	Key_ClearStates ();
	IN_ClearStates ();
}


//==========================================================================
//
//  COMMANDS
//
//==========================================================================

/*
=================
VID_DescribeCurrentMode_f
=================
*/
static void VID_DescribeCurrentMode_f (void)
{
	Con_Printf("%dx%dx%d %dHz %s\n",
		VID_GetCurrentWidth(),
		VID_GetCurrentHeight(),
		VID_GetCurrentBPP(),
		VID_GetCurrentRefreshRate(),
		VID_GetFullscreen() ? "fullscreen" : "windowed");
}

/*
=================
VID_DescribeModes_f -- johnfitz -- changed formatting, and added refresh rates after each mode.
=================
*/
static void VID_DescribeModes_f (void)
{
	int	i;
	int	lastwidth, lastheight, lastbpp, count;

	lastwidth = lastheight = lastbpp = count = 0;

	for (i = 0; i < nummodes; i++)
	{
		if (lastwidth != modelist[i].width || lastheight != modelist[i].height || lastbpp != modelist[i].bpp)
		{
			if (count > 0)
				Con_SafePrintf ("\n");
			Con_SafePrintf ("   %4i x %4i x %i : %i", modelist[i].width, modelist[i].height, modelist[i].bpp, modelist[i].refreshrate);
			lastwidth = modelist[i].width;
			lastheight = modelist[i].height;
			lastbpp = modelist[i].bpp;
			count++;
		}
	}
	Con_Printf ("\n%i modes\n", count);
}

/*
===================
VID_FSAA_f -- ericw -- warn that vid_fsaa requires engine restart
===================
*/
static void VID_FSAA_f (cvar_t *var)
{

}

//==========================================================================
//
//  INIT
//
//==========================================================================

/*
=================
VID_InitModelist
=================
*/
static void VID_InitModelist (void)
{
	void *ptr = NULL;
	VIDEO_MODE vm = { 0 };
	nummodes = 0;

	while (XVideoListModes(&vm, 0, 0, &ptr))
	{
		modelist[nummodes].width = vm.width;
		modelist[nummodes].height = vm.height;
		modelist[nummodes].bpp = vm.bpp;
		modelist[nummodes].refreshrate = vm.refresh;
		nummodes++;
	}

	if (nummodes == 0)
	{
		modelist[0].width = xbox_vidmode.width;
		modelist[0].height = xbox_vidmode.height;
		modelist[0].bpp = xbox_vidmode.bpp;
		modelist[0].refreshrate = xbox_vidmode.refresh;
		nummodes = 1;
	}
}

/*
===================
VID_Init
===================
*/
void	VID_Init (void)
{
	int		p, width, height, refreshrate, bpp;
	qboolean	fullscreen;
	const char	*read_vars[] = { "vid_fullscreen",
					 "vid_width",
					 "vid_height",
					 "vid_refreshrate",
					 "vid_bpp",
					 "vid_vsync"};
#define num_readvars	Q_COUNTOF(read_vars)

	Cvar_RegisterVariable (&vid_fullscreen); //johnfitz
	Cvar_RegisterVariable (&vid_width); //johnfitz
	Cvar_RegisterVariable (&vid_height); //johnfitz
	Cvar_RegisterVariable (&vid_refreshrate); //johnfitz
	Cvar_RegisterVariable (&vid_bpp); //johnfitz
	Cvar_RegisterVariable (&vid_vsync); //johnfitz
	Cvar_SetCallback (&vid_fullscreen, VID_Changed_f);
	Cvar_SetCallback (&vid_width, VID_Changed_f);
	Cvar_SetCallback (&vid_height, VID_Changed_f);
	Cvar_SetCallback (&vid_refreshrate, VID_Changed_f);
	Cvar_SetCallback (&vid_bpp, VID_Changed_f);
	Cvar_SetCallback (&vid_vsync, VID_Changed_f);

	Cmd_AddCommand ("vid_unlock", VID_Unlock); //johnfitz
	Cmd_AddCommand ("vid_restart", VID_Restart); //johnfitz
	Cmd_AddCommand ("vid_test", VID_Test); //johnfitz
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	Cvar_SetValueQuick (&vid_bpp, 32);

	if (CFG_OpenConfig("config.cfg") == 0)
	{
		CFG_ReadCvars(read_vars, num_readvars);
		CFG_CloseConfig();
	}
	CFG_ReadCvarOverrides(read_vars, num_readvars);

	xbox_vidmode = XVideoGetMode();

	VID_InitModelist();

	width = (int)vid_width.value;
	height = (int)vid_height.value;
	refreshrate = (int)vid_refreshrate.value;
	bpp = (int)vid_bpp.value;
	fullscreen = (int)vid_fullscreen.value;

	if (!VID_ValidMode(width, height, refreshrate, bpp, fullscreen))
	{
		width = 640;
		height = 480;
		refreshrate = REFRESH_DEFAULT;
		bpp = 32;
		fullscreen = true;
	}

	vid_initialized = true;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	VID_SetMode (width, height, refreshrate, bpp, fullscreen);

	GL_Init ();
	GL_SetupState ();
	Cmd_AddCommand ("gl_info", GL_Info_f); //johnfitz

	//johnfitz -- removed code creating "glquake" subdirectory

	vid_menucmdfn = VID_Menu_f; //johnfitz
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	VID_Gamma_Init(); //johnfitz
	VID_Menu_Init(); //johnfitz

	//QuakeSpasm: current vid settings should override config file settings.
	//so we have to lock the vid mode from now until after all config files are read.
	vid_locked = true;
}

// new proc by S.A., called by alt-return key binding.
void	VID_Toggle (void)
{

}

/*
================
VID_SyncCvars -- johnfitz -- set vid cvars to match current video mode
================
*/
void VID_SyncCvars (void)
{
	Cvar_SetValueQuick (&vid_width, VID_GetCurrentWidth());
	Cvar_SetValueQuick (&vid_height, VID_GetCurrentHeight());
	Cvar_SetValueQuick (&vid_refreshrate, VID_GetCurrentRefreshRate());
	Cvar_SetValueQuick (&vid_bpp, VID_GetCurrentBPP());
	Cvar_SetQuick (&vid_fullscreen, VID_GetFullscreen() ? "1" : "0");
	Cvar_SetQuick (&vid_vsync, VID_GetVSync() ? "1" : "0");

	vid_changed = false;
}

//==========================================================================
//
//  NEW VIDEO MENU -- johnfitz
//
//==========================================================================

enum {
	VID_OPT_MODE,
	VID_OPT_BPP,
	VID_OPT_REFRESHRATE,
	VID_OPT_FULLSCREEN,
	VID_OPT_VSYNC,
	VID_OPT_TEST,
	VID_OPT_APPLY,
	VIDEO_OPTIONS_ITEMS
};

static int	video_options_cursor = 0;

typedef struct {
	int width,height;
} vid_menu_mode;

//TODO: replace these fixed-length arrays with hunk_allocated buffers
static vid_menu_mode vid_menu_modes[MAX_MODE_LIST];
static int vid_menu_nummodes = 0;

static int vid_menu_bpps[MAX_BPPS_LIST];
static int vid_menu_numbpps = 0;

static int vid_menu_rates[MAX_RATES_LIST];
static int vid_menu_numrates=0;

/*
================
VID_Menu_Init
================
*/
static void VID_Menu_Init (void)
{
	int i, j, h, w;

	for (i = 0; i < nummodes; i++)
	{
		w = modelist[i].width;
		h = modelist[i].height;

		for (j = 0; j < vid_menu_nummodes; j++)
		{
			if (vid_menu_modes[j].width == w &&
				vid_menu_modes[j].height == h)
				break;
		}

		if (j == vid_menu_nummodes)
		{
			vid_menu_modes[j].width = w;
			vid_menu_modes[j].height = h;
			vid_menu_nummodes++;
		}
	}
}

/*
================
VID_Menu_RebuildBppList

regenerates bpp list based on current vid_width and vid_height
================
*/
static void VID_Menu_RebuildBppList (void)
{
	int i, j, b;

	vid_menu_numbpps = 0;

	for (i = 0; i < nummodes; i++)
	{
		if (vid_menu_numbpps >= MAX_BPPS_LIST)
			break;

		//bpp list is limited to bpps available with current width/height
		if (modelist[i].width != vid_width.value ||
			modelist[i].height != vid_height.value)
			continue;

		b = modelist[i].bpp;

		for (j = 0; j < vid_menu_numbpps; j++)
		{
			if (vid_menu_bpps[j] == b)
				break;
		}

		if (j == vid_menu_numbpps)
		{
			vid_menu_bpps[j] = b;
			vid_menu_numbpps++;
		}
	}

	//if there are no valid fullscreen bpps for this width/height, just pick one
	if (vid_menu_numbpps == 0)
	{
		Cvar_SetValueQuick (&vid_bpp, (float)modelist[0].bpp);
		return;
	}

	//if vid_bpp is not in the new list, change vid_bpp
	for (i = 0; i < vid_menu_numbpps; i++)
		if (vid_menu_bpps[i] == (int)(vid_bpp.value))
			break;

	if (i == vid_menu_numbpps)
		Cvar_SetValueQuick (&vid_bpp, (float)vid_menu_bpps[0]);
}

/*
================
VID_Menu_RebuildRateList

regenerates rate list based on current vid_width, vid_height and vid_bpp
================
*/
static void VID_Menu_RebuildRateList (void)
{
	int i, j, r;

	vid_menu_numrates = 0;

	for (i = 0; i < nummodes; i++)
	{
		//rate list is limited to rates available with current width/height/bpp
		if (modelist[i].width != vid_width.value ||
		    modelist[i].height != vid_height.value ||
		    modelist[i].bpp != vid_bpp.value)
			continue;

		r = modelist[i].refreshrate;

		for (j = 0; j < vid_menu_numrates; j++)
		{
			if (vid_menu_rates[j] == r)
				break;
		}

		if (j == vid_menu_numrates)
		{
			vid_menu_rates[j] = r;
			vid_menu_numrates++;
		}
	}

	//if there are no valid fullscreen refreshrates for this width/height, just pick one
	if (vid_menu_numrates == 0)
	{
		Cvar_SetValue ("vid_refreshrate",(float)modelist[0].refreshrate);
		return;
	}

	//if vid_refreshrate is not in the new list, change vid_refreshrate
	for (i = 0; i < vid_menu_numrates; i++)
		if (vid_menu_rates[i] == (int)(vid_refreshrate.value))
			break;

	if (i == vid_menu_numrates)
		Cvar_SetValue ("vid_refreshrate",(float)vid_menu_rates[0]);
}

/*
================
VID_Menu_ChooseNextMode

chooses next resolution in order, then updates vid_width and
vid_height cvars, then updates bpp and refreshrate lists
================
*/
static void VID_Menu_ChooseNextMode (int dir)
{
	int i;

	if (vid_menu_nummodes)
	{
		for (i = 0; i < vid_menu_nummodes; i++)
		{
			if (vid_menu_modes[i].width == vid_width.value &&
				vid_menu_modes[i].height == vid_height.value)
				break;
		}

		if (i == vid_menu_nummodes) //can't find it in list, so it must be a custom windowed res
		{
			i = 0;
		}
		else
		{
			i += dir;
			if (i >= vid_menu_nummodes)
				i = 0;
			else if (i < 0)
				i = vid_menu_nummodes-1;
		}

		Cvar_SetValueQuick (&vid_width, (float)vid_menu_modes[i].width);
		Cvar_SetValueQuick (&vid_height, (float)vid_menu_modes[i].height);
		VID_Menu_RebuildBppList ();
		VID_Menu_RebuildRateList ();
	}
}

/*
================
VID_Menu_ChooseNextBpp

chooses next bpp in order, then updates vid_bpp cvar
================
*/
static void VID_Menu_ChooseNextBpp (int dir)
{
	int i;

	if (vid_menu_numbpps)
	{
		for (i = 0; i < vid_menu_numbpps; i++)
		{
			if (vid_menu_bpps[i] == vid_bpp.value)
				break;
		}

		if (i == vid_menu_numbpps) //can't find it in list
		{
			i = 0;
		}
		else
		{
			i += dir;
			if (i >= vid_menu_numbpps)
				i = 0;
			else if (i < 0)
				i = vid_menu_numbpps-1;
		}

		Cvar_SetValueQuick (&vid_bpp, (float)vid_menu_bpps[i]);
	}
}

/*
================
VID_Menu_ChooseNextRate

chooses next refresh rate in order, then updates vid_refreshrate cvar
================
*/
static void VID_Menu_ChooseNextRate (int dir)
{
	int i;

	for (i = 0; i < vid_menu_numrates; i++)
	{
		if (vid_menu_rates[i] == vid_refreshrate.value)
			break;
	}

	if (i == vid_menu_numrates) //can't find it in list
	{
		i = 0;
	}
	else
	{
		i += dir;
		if (i >= vid_menu_numrates)
			i = 0;
		else if (i < 0)
			i = vid_menu_numrates-1;
	}

	Cvar_SetValue ("vid_refreshrate",(float)vid_menu_rates[i]);
}

/*
================
VID_MenuKey
================
*/
static void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
		VID_SyncCvars (); //sync cvars before leaving menu. FIXME: there are other ways to leave menu
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor--;
		if (video_options_cursor < 0)
			video_options_cursor = VIDEO_OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor++;
		if (video_options_cursor >= VIDEO_OPTIONS_ITEMS)
			video_options_cursor = 0;
		break;

	case K_LEFTARROW:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case VID_OPT_MODE:
			VID_Menu_ChooseNextMode (1);
			break;
		case VID_OPT_BPP:
			VID_Menu_ChooseNextBpp (1);
			break;
		case VID_OPT_REFRESHRATE:
			VID_Menu_ChooseNextRate (1);
			break;
		case VID_OPT_FULLSCREEN:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case VID_OPT_VSYNC:
			Cbuf_AddText ("toggle vid_vsync\n"); // kristian
			break;
		default:
			break;
		}
		break;

	case K_RIGHTARROW:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case VID_OPT_MODE:
			VID_Menu_ChooseNextMode (-1);
			break;
		case VID_OPT_BPP:
			VID_Menu_ChooseNextBpp (-1);
			break;
		case VID_OPT_REFRESHRATE:
			VID_Menu_ChooseNextRate (-1);
			break;
		case VID_OPT_FULLSCREEN:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case VID_OPT_VSYNC:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		default:
			break;
		}
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
		m_entersound = true;
		switch (video_options_cursor)
		{
		case VID_OPT_MODE:
			VID_Menu_ChooseNextMode (1);
			break;
		case VID_OPT_BPP:
			VID_Menu_ChooseNextBpp (1);
			break;
		case VID_OPT_REFRESHRATE:
			VID_Menu_ChooseNextRate (1);
			break;
		case VID_OPT_FULLSCREEN:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case VID_OPT_VSYNC:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		case VID_OPT_TEST:
			Cbuf_AddText ("vid_test\n");
			break;
		case VID_OPT_APPLY:
			Cbuf_AddText ("vid_restart\n");
			key_dest = key_game;
			m_state = m_none;
			IN_Activate();
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

/*
================
VID_MenuDraw
================
*/
static void VID_MenuDraw (void)
{
	int i, y;
	qpic_t *p;
	const char *title;

	y = 4;

	// plaque
	p = Draw_CachePic ("gfx/qplaque.lmp");
	M_DrawTransPic (16, y, p);

	//p = Draw_CachePic ("gfx/vidmodes.lmp");
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, y, p);

	y += 28;

	// title
	title = "Video Options";
	M_PrintWhite ((320-8*strlen(title))/2, y, title);

	y += 16;

	// options
	for (i = 0; i < VIDEO_OPTIONS_ITEMS; i++)
	{
		switch (i)
		{
		case VID_OPT_MODE:
			M_Print (16, y, "        Video mode");
			M_Print (184, y, va("%ix%i", (int)vid_width.value, (int)vid_height.value));
			break;
		case VID_OPT_BPP:
			M_Print (16, y, "       Color depth");
			M_Print (184, y, va("%i", (int)vid_bpp.value));
			break;
		case VID_OPT_REFRESHRATE:
			M_Print (16, y, "      Refresh rate");
			M_Print (184, y, va("%i", (int)vid_refreshrate.value));
			break;
		case VID_OPT_FULLSCREEN:
			M_Print (16, y, "        Fullscreen");
			M_DrawCheckbox (184, y, (int)vid_fullscreen.value);
			break;
		case VID_OPT_VSYNC:
			M_Print (16, y, "     Vertical sync");
			if (gl_swap_control)
				M_DrawCheckbox (184, y, (int)vid_vsync.value);
			else
				M_Print (184, y, "N/A");
			break;
		case VID_OPT_TEST:
			y += 8; //separate the test and apply items
			M_Print (16, y, "      Test changes");
			break;
		case VID_OPT_APPLY:
			M_Print (16, y, "     Apply changes");
			break;
		}

		if (video_options_cursor == i)
			M_DrawCharacter (168, y, 12+((int)(realtime*4)&1));

		y += 8;
	}
}

/*
================
VID_Menu_f
================
*/
static void VID_Menu_f (void)
{
	IN_Deactivate(modestate == MS_WINDOWED);
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;

	//set all the cvars to match the current mode when entering the menu
	VID_SyncCvars ();

	//set up bpp and rate lists based on current cvars
	VID_Menu_RebuildBppList ();
	VID_Menu_RebuildRateList ();
}

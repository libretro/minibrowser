#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>

#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
#include <sys/timer.h>
#elif defined(XENON)
#include <time/time.h>
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
#include <unistd.h>
#elif defined(PSP)
#include <pspthreadman.h>
#elif defined(VITA)
#include <psp2/kernel/threadmgr.h>
#elif defined(_3DS)
#include <3ds.h>
#else
#include <time.h>
#endif

#include "libretro.h"
#include "minibrowser.h"
#include <QApplication>
#include <QFontDatabase>
#include <QFile>
#include <QtPlugin>
Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)

#ifdef RARCH_INTERNAL
#include "internal_cores.h"
#define NETRETROPAD_CORE_PREFIX(s) libretro_netretropad_##s
#else
#define NETRETROPAD_CORE_PREFIX(s) s
#endif

#define DESC_NUM_PORTS(desc) ((desc)->port_max - (desc)->port_min + 1)
#define DESC_NUM_INDICES(desc) ((desc)->index_max - (desc)->index_min + 1)
#define DESC_NUM_IDS(desc) ((desc)->id_max - (desc)->id_min + 1)

#define DESC_OFFSET(desc, port, index, id) ( \
   port * ((desc)->index_max - (desc)->index_min + 1) * ((desc)->id_max - (desc)->id_min + 1) + \
   index * ((desc)->id_max - (desc)->id_min + 1) + \
   id \
)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#if defined(_WIN32) || defined(__INTEL_COMPILER)
#define INLINE __inline
#elif defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
#define INLINE inline
#elif defined(__GNUC__)
#define INLINE __inline__
#else
#define INLINE
#endif

#define WIDTH 1920
#define HEIGHT 1080

/**
 * retro_sleep:
 * @msec         : amount in milliseconds to sleep
 *
 * Sleeps for a specified amount of milliseconds (@msec).
 **/
static INLINE void retro_sleep(unsigned msec)
{
#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   sys_timer_usleep(1000 * msec);
#elif defined(PSP) || defined(VITA)
   sceKernelDelayThread(1000 * msec);
#elif defined(_3DS)
   svcSleepThread(1000000 * (s64)msec);
#elif defined(_WIN32)
   Sleep(msec);
#elif defined(XENON)
   udelay(1000 * msec);
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
   usleep(1000 * msec);
#else
   struct timespec tv;
   tv.tv_sec = msec / 1000;
   tv.tv_nsec = (msec % 1000) * 1000000;
   nanosleep(&tv, NULL);
#endif
}

struct descriptor {
   int device;
   int port_min;
   int port_max;
   int index_min;
   int index_max;
   int id_min;
   int id_max;
   uint16_t *value;
};

struct remote_joypad_message {
   int port;
   int device;
   int index;
   int id;
   uint16_t state;
};

static struct retro_log_callback logger;

static retro_log_printf_t NETRETROPAD_CORE_PREFIX(log_cb);
static retro_video_refresh_t NETRETROPAD_CORE_PREFIX(video_cb);
static retro_audio_sample_t NETRETROPAD_CORE_PREFIX(audio_cb);
static retro_audio_sample_batch_t NETRETROPAD_CORE_PREFIX(audio_batch_cb);
static retro_environment_t NETRETROPAD_CORE_PREFIX(environ_cb);
static retro_input_poll_t NETRETROPAD_CORE_PREFIX(input_poll_cb);
static retro_input_state_t NETRETROPAD_CORE_PREFIX(input_state_cb);

static uint8_t *frame_buf;

static struct descriptor joypad = {
   .device = RETRO_DEVICE_JOYPAD,
   .port_min = 0,
   .port_max = 0,
   .index_min = 0,
   .index_max = 0,
   .id_min = RETRO_DEVICE_ID_JOYPAD_B,
   .id_max = RETRO_DEVICE_ID_JOYPAD_R3,
   .value = NULL
};

static struct descriptor analog = {
   .device = RETRO_DEVICE_ANALOG,
   .port_min = 0,
   .port_max = 0,
   .index_min = RETRO_DEVICE_INDEX_ANALOG_LEFT,
   .index_max = RETRO_DEVICE_INDEX_ANALOG_RIGHT,
   .id_min = RETRO_DEVICE_ID_ANALOG_X,
   .id_max = RETRO_DEVICE_ID_ANALOG_Y,
   .value = NULL
};

static struct descriptor keyboard = {
   .device = RETRO_DEVICE_KEYBOARD,
   .port_min = 0,
   .port_max = 0,
   .index_min = 0,
   .index_max = 0,
   .id_min = RETROK_BACKSPACE,
   .id_max = RETROK_UNDO,
   .value = NULL
};

static struct descriptor mouse = {
   .device = RETRO_DEVICE_MOUSE,
   .port_min = 0,
   .port_max = 0,
   .index_min = 0,
   .index_max = 0,
   .id_min = RETRO_DEVICE_ID_MOUSE_X,
   .id_max = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN,
   .value = NULL
};

static struct descriptor *descriptors[] = {
   &joypad,
   &analog,
   &keyboard,
   &mouse
};

static QApplication *browserApp;
static MiniBrowser *browserWin;

static char browser_name[] = "minibrowser";
static char *browser_argv[] = {browser_name, NULL};
static int browser_argc = ARRAY_SIZE(browser_argv) - 1;

static uint16_t x_coord;
static uint16_t y_coord;

/* Borrowed from RetroArch/gfx/drivers_font_renderer/freetype.c */
static const char *font_paths[] = {
#if defined(_WIN32)
   "C:\\Windows\\Fonts\\consola.ttf",
   "C:\\Windows\\Fonts\\verdana.ttf",
#elif defined(__APPLE__)
   "/Library/Fonts/Microsoft/Candara.ttf",
   "/Library/Fonts/Verdana.ttf",
   "/Library/Fonts/Tahoma.ttf",
#else
   "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
   "/usr/share/fonts/TTF/DejaVuSans.ttf",
   "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf",
   "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
   "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
   "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
   "/usr/share/fonts/TTF/Vera.ttf",
   "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
#endif
   ":/unifont.ttf", /* Embedded in Qt resource file */
   "osd-font.ttf", /* Magic font to search for, useful for distribution. */
};

void NETRETROPAD_CORE_PREFIX(retro_init)(void)
{
   struct descriptor *desc;
   int size;
   unsigned i;

   frame_buf = NULL;

   qputenv("GST_PLUGIN_SYSTEM_PATH", "");

   browserApp = new QApplication(browser_argc, browser_argv);

   Q_INIT_RESOURCE(res);

   for (i = 0; i < ARRAY_SIZE(font_paths); i++)
   {
      const char *path = font_paths[i];
      QFile fontFile(path);

      if (QFile::exists(path) && fontFile.open(QIODevice::ReadOnly))
      {
         QByteArray fontData = fontFile.readAll();
         QFontDatabase::addApplicationFontFromData(fontData);
         fontFile.close();
      }
   }

   browserWin = new MiniBrowser;
   browserWin->resize(WIDTH, HEIGHT);
   browserWin->setImage(WIDTH, HEIGHT, QImage::Format_RGB32);
   browserWin->show();
   browserApp->processEvents();

   /* Allocate descriptor values */
   for (i = 0; i < ARRAY_SIZE(descriptors); i++) {
      desc = descriptors[i];
      size = DESC_NUM_PORTS(desc) * DESC_NUM_INDICES(desc) * DESC_NUM_IDS(desc);
      descriptors[i]->value = (uint16_t*)calloc(size, sizeof(uint16_t));
   }
}

void NETRETROPAD_CORE_PREFIX(retro_deinit)(void)
{
   unsigned i;

   Q_CLEANUP_RESOURCE(res);

   if (frame_buf)
      free(frame_buf);
   frame_buf = NULL;

   /* Free descriptor values */
   for (i = 0; i < ARRAY_SIZE(descriptors); i++) {
      free(descriptors[i]->value);
      descriptors[i]->value = NULL;
   }
}

unsigned NETRETROPAD_CORE_PREFIX(retro_api_version)(void)
{
   return RETRO_API_VERSION;
}

void NETRETROPAD_CORE_PREFIX(retro_set_controller_port_device)(
      unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void NETRETROPAD_CORE_PREFIX(retro_get_system_info)(
      struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "MiniBrowser";
   info->library_version  = "1.0";
   info->need_fullpath    = false;
   info->valid_extensions = ""; /* Nothing. */
}

void NETRETROPAD_CORE_PREFIX(retro_get_system_av_info)(
      struct retro_system_av_info *info)
{
   info->timing.fps = 60.0;
   info->timing.sample_rate = 30000.0;

   info->geometry.base_width  = WIDTH;
   info->geometry.base_height = HEIGHT;
   info->geometry.max_width   = WIDTH;
   info->geometry.max_height  = HEIGHT;
   info->geometry.aspect_ratio = 16.0 / 9.0;
}

void NETRETROPAD_CORE_PREFIX(retro_set_environment)(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
      { NULL, NULL },
   };
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);


   NETRETROPAD_CORE_PREFIX(environ_cb) = cb;
   bool no_content = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   NETRETROPAD_CORE_PREFIX(environ_cb)(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logger))
      NETRETROPAD_CORE_PREFIX(log_cb) = logger.log;
}

static void netretropad_check_variables(void)
{
}

void NETRETROPAD_CORE_PREFIX(retro_set_audio_sample)(retro_audio_sample_t cb)
{
   NETRETROPAD_CORE_PREFIX(audio_cb) = cb;
}

void NETRETROPAD_CORE_PREFIX(retro_set_audio_sample_batch)(
      retro_audio_sample_batch_t cb)
{
   NETRETROPAD_CORE_PREFIX(audio_batch_cb) = cb;
}

void NETRETROPAD_CORE_PREFIX(retro_set_input_poll)(retro_input_poll_t cb)
{
   NETRETROPAD_CORE_PREFIX(input_poll_cb) = cb;
}

void NETRETROPAD_CORE_PREFIX(retro_set_input_state)(retro_input_state_t cb)
{
   NETRETROPAD_CORE_PREFIX(input_state_cb) = cb;
}

void NETRETROPAD_CORE_PREFIX(retro_set_video_refresh)(retro_video_refresh_t cb)
{
   NETRETROPAD_CORE_PREFIX(video_cb) = cb;
}

void NETRETROPAD_CORE_PREFIX(retro_reset)(void)
{
   x_coord = 0;
   y_coord = 0;
}

static void retropad_update_input(void)
{
   struct descriptor *desc;
   /*struct remote_joypad_message msg;*/
   uint16_t state;
   uint16_t old;
   int offset;
   int port;
   int index;
   int id;
   unsigned i;

   /* Poll input */
   NETRETROPAD_CORE_PREFIX(input_poll_cb)();

   /* Parse descriptors */
   for (i = 0; i < ARRAY_SIZE(descriptors); i++)
   {
      /* Get current descriptor */
      desc = descriptors[i];

      /* Go through range of ports/indices/IDs */
      for (port = desc->port_min; port <= desc->port_max; port++)
      {
         for (index = desc->index_min; index <= desc->index_max; index++)
         {
            for (id = desc->id_min; id <= desc->id_max; id++)
            {
               /* Compute offset into array */
               offset = DESC_OFFSET(desc, port, index, id);

               /* Get old state */
               old = desc->value[offset];

               /* Get new state */
               state = NETRETROPAD_CORE_PREFIX(input_state_cb)(
                  port,
                  desc->device,
                  index,
                  id);

               /* Continue if state is unchanged */
               if (state == old)
                  continue;

               /* Update state */
               desc->value[offset] = state;

               /* Attempt to send updated state */
               /*msg.port = port;
               msg.device = desc->device;
               msg.index = index;
               msg.id = id;
               msg.state = state;*/
            }
         }
      }
   }
}

static inline QtKey retrokey_to_qt(unsigned button, uint32_t character, uint16_t rkmod) {
   Qt::KeyboardModifier mod = Qt::NoModifier;

   switch (rkmod)
   {
      case RETROKMOD_NONE:
         break;
      case RETROKMOD_SHIFT:
         mod = Qt::ShiftModifier;
         break;
      case RETROKMOD_CTRL:
         mod = Qt::ControlModifier;
         break;
      case RETROKMOD_ALT:
         mod = Qt::AltModifier;
         break;
      case RETROKMOD_META:
         mod = Qt::MetaModifier;
         break;
      case RETROKMOD_NUMLOCK:
         mod = Qt::KeypadModifier;
         break;
      default:
         break;
   }

   if (button == 0)
   {
      switch (character)
      {
         case RETROK_a-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_b-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_c-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_d-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_e-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_f-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_g-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_h-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_i-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_j-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_k-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_l-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_m-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_n-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_o-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_p-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_q-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_r-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_s-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_t-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_u-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_v-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_w-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_x-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_y-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         case RETROK_z-32: return QtKey(static_cast<Qt::Key>(0), character, mod);
         default:
            break;
      }
   }
   else
   {
      switch (button)
      {
         case RETROK_BACKSPACE: return QtKey(Qt::Key_Backspace, character, mod);
         case RETROK_TAB: return QtKey(Qt::Key_Tab, character, mod);
         case RETROK_CLEAR: return QtKey(Qt::Key_Clear, character, mod);
         case RETROK_RETURN: return QtKey(Qt::Key_Return, character, mod);
         case RETROK_PAUSE: return QtKey(Qt::Key_Pause, character, mod);
         case RETROK_ESCAPE: return QtKey(Qt::Key_Escape, character, mod);
         case RETROK_SPACE: return QtKey(Qt::Key_Space, character, mod);
         case RETROK_EXCLAIM: return QtKey(Qt::Key_Exclam, character, mod);
         case RETROK_QUOTEDBL: return QtKey(Qt::Key_QuoteDbl, character, mod);
         case RETROK_HASH: return QtKey(Qt::Key_NumberSign, character, mod);
         case RETROK_DOLLAR: return QtKey(Qt::Key_Dollar, character, mod);
         case RETROK_AMPERSAND: return QtKey(Qt::Key_Ampersand, character, mod);
         case RETROK_QUOTE: return QtKey(Qt::Key_Apostrophe, character, mod);
         case RETROK_LEFTPAREN: return QtKey(Qt::Key_ParenLeft, character, mod);
         case RETROK_RIGHTPAREN: return QtKey(Qt::Key_ParenRight, character, mod);
         case RETROK_ASTERISK: return QtKey(Qt::Key_Asterisk, character, mod);
         case RETROK_PLUS: return QtKey(Qt::Key_Plus, character, mod);
         case RETROK_COMMA: return QtKey(Qt::Key_Comma, character, mod);
         case RETROK_MINUS: return QtKey(Qt::Key_Minus, character, mod);
         case RETROK_PERIOD: return QtKey(Qt::Key_Period, character, mod);
         case RETROK_SLASH: return QtKey(Qt::Key_Slash, character, mod);
         case RETROK_0: return QtKey(Qt::Key_0, character, mod);
         case RETROK_1: return QtKey(Qt::Key_1, character, mod);
         case RETROK_2: return QtKey(Qt::Key_2, character, mod);
         case RETROK_3: return QtKey(Qt::Key_3, character, mod);
         case RETROK_4: return QtKey(Qt::Key_4, character, mod);
         case RETROK_5: return QtKey(Qt::Key_5, character, mod);
         case RETROK_6: return QtKey(Qt::Key_6, character, mod);
         case RETROK_7: return QtKey(Qt::Key_7, character, mod);
         case RETROK_8: return QtKey(Qt::Key_8, character, mod);
         case RETROK_9: return QtKey(Qt::Key_9, character, mod);
         case RETROK_COLON: return QtKey(Qt::Key_Colon, character, mod);
         case RETROK_SEMICOLON: return QtKey(Qt::Key_Semicolon, character, mod);
         case RETROK_LESS: return QtKey(Qt::Key_Less, character, mod);
         case RETROK_EQUALS: return QtKey(Qt::Key_Equal, character, mod);
         case RETROK_GREATER: return QtKey(Qt::Key_Greater, character, mod);
         case RETROK_QUESTION: return QtKey(Qt::Key_Question, character, mod);
         case RETROK_AT: return QtKey(Qt::Key_At, character, mod);
         case RETROK_LEFTBRACKET: return QtKey(Qt::Key_BracketLeft, character, mod);
         case RETROK_BACKSLASH: return QtKey(Qt::Key_Backslash, character, mod);
         case RETROK_RIGHTBRACKET: return QtKey(Qt::Key_BracketRight, character, mod);
         case RETROK_CARET: return QtKey(Qt::Key_AsciiCircum, character, mod);
         case RETROK_UNDERSCORE: return QtKey(Qt::Key_Underscore, character, mod);
         case RETROK_BACKQUOTE: return QtKey(Qt::Key_QuoteLeft, character, mod);
         case RETROK_a: return QtKey(static_cast<Qt::Key>(Qt::Key_A + 32), character, mod);
         case RETROK_b: return QtKey(static_cast<Qt::Key>(Qt::Key_B + 32), character, mod);
         case RETROK_c: return QtKey(static_cast<Qt::Key>(Qt::Key_C + 32), character, mod);
         case RETROK_d: return QtKey(static_cast<Qt::Key>(Qt::Key_D + 32), character, mod);
         case RETROK_e: return QtKey(static_cast<Qt::Key>(Qt::Key_E + 32), character, mod);
         case RETROK_f: return QtKey(static_cast<Qt::Key>(Qt::Key_F + 32), character, mod);
         case RETROK_g: return QtKey(static_cast<Qt::Key>(Qt::Key_G + 32), character, mod);
         case RETROK_h: return QtKey(static_cast<Qt::Key>(Qt::Key_H + 32), character, mod);
         case RETROK_i: return QtKey(static_cast<Qt::Key>(Qt::Key_I + 32), character, mod);
         case RETROK_j: return QtKey(static_cast<Qt::Key>(Qt::Key_J + 32), character, mod);
         case RETROK_k: return QtKey(static_cast<Qt::Key>(Qt::Key_K + 32), character, mod);
         case RETROK_l: return QtKey(static_cast<Qt::Key>(Qt::Key_L + 32), character, mod);
         case RETROK_m: return QtKey(static_cast<Qt::Key>(Qt::Key_M + 32), character, mod);
         case RETROK_n: return QtKey(static_cast<Qt::Key>(Qt::Key_N + 32), character, mod);
         case RETROK_o: return QtKey(static_cast<Qt::Key>(Qt::Key_O + 32), character, mod);
         case RETROK_p: return QtKey(static_cast<Qt::Key>(Qt::Key_P + 32), character, mod);
         case RETROK_q: return QtKey(static_cast<Qt::Key>(Qt::Key_Q + 32), character, mod);
         case RETROK_r: return QtKey(static_cast<Qt::Key>(Qt::Key_R + 32), character, mod);
         case RETROK_s: return QtKey(static_cast<Qt::Key>(Qt::Key_S + 32), character, mod);
         case RETROK_t: return QtKey(static_cast<Qt::Key>(Qt::Key_T + 32), character, mod);
         case RETROK_u: return QtKey(static_cast<Qt::Key>(Qt::Key_U + 32), character, mod);
         case RETROK_v: return QtKey(static_cast<Qt::Key>(Qt::Key_V + 32), character, mod);
         case RETROK_w: return QtKey(static_cast<Qt::Key>(Qt::Key_W + 32), character, mod);
         case RETROK_x: return QtKey(static_cast<Qt::Key>(Qt::Key_X + 32), character, mod);
         case RETROK_y: return QtKey(static_cast<Qt::Key>(Qt::Key_Y + 32), character, mod);
         case RETROK_z: return QtKey(static_cast<Qt::Key>(Qt::Key_Z + 32), character, mod);
         case RETROK_DELETE: return QtKey(Qt::Key_Delete, character, mod);

         case RETROK_KP0: return QtKey(Qt::Key_0, character, mod);
         case RETROK_KP1: return QtKey(Qt::Key_1, character, mod);
         case RETROK_KP2: return QtKey(Qt::Key_2, character, mod);
         case RETROK_KP3: return QtKey(Qt::Key_3, character, mod);
         case RETROK_KP4: return QtKey(Qt::Key_4, character, mod);
         case RETROK_KP5: return QtKey(Qt::Key_5, character, mod);
         case RETROK_KP6: return QtKey(Qt::Key_6, character, mod);
         case RETROK_KP7: return QtKey(Qt::Key_7, character, mod);
         case RETROK_KP8: return QtKey(Qt::Key_8, character, mod);
         case RETROK_KP9: return QtKey(Qt::Key_9, character, mod);
         case RETROK_KP_PERIOD: return QtKey(Qt::Key_Period, character, mod);
         case RETROK_KP_DIVIDE: return QtKey(Qt::Key_Slash, character, mod);
         case RETROK_KP_MULTIPLY: return QtKey(Qt::Key_multiply, character, mod);
         case RETROK_KP_MINUS: return QtKey(Qt::Key_Minus, character, mod);
         case RETROK_KP_PLUS: return QtKey(Qt::Key_Plus, character, mod);
         case RETROK_KP_ENTER: return QtKey(Qt::Key_Enter, character, mod);
         case RETROK_KP_EQUALS: return QtKey(Qt::Key_Equal, character, mod);

         case RETROK_UP: return QtKey(Qt::Key_Up, character, mod);
         case RETROK_DOWN: return QtKey(Qt::Key_Down, character, mod);
         case RETROK_RIGHT: return QtKey(Qt::Key_Right, character, mod);
         case RETROK_LEFT: return QtKey(Qt::Key_Left, character, mod);
         case RETROK_INSERT: return QtKey(Qt::Key_Insert, character, mod);
         case RETROK_HOME: return QtKey(Qt::Key_Home, character, mod);
         case RETROK_END: return QtKey(Qt::Key_End, character, mod);
         case RETROK_PAGEUP: return QtKey(Qt::Key_PageUp, character, mod);
         case RETROK_PAGEDOWN: return QtKey(Qt::Key_PageDown, character, mod);

         case RETROK_F1: return QtKey(Qt::Key_F1, character, mod);
         case RETROK_F2: return QtKey(Qt::Key_F2, character, mod);
         case RETROK_F3: return QtKey(Qt::Key_F3, character, mod);
         case RETROK_F4: return QtKey(Qt::Key_F4, character, mod);
         case RETROK_F5: return QtKey(Qt::Key_F5, character, mod);
         case RETROK_F6: return QtKey(Qt::Key_F6, character, mod);
         case RETROK_F7: return QtKey(Qt::Key_F7, character, mod);
         case RETROK_F8: return QtKey(Qt::Key_F8, character, mod);
         case RETROK_F9: return QtKey(Qt::Key_F9, character, mod);
         case RETROK_F10: return QtKey(Qt::Key_F10, character, mod);
         case RETROK_F11: return QtKey(Qt::Key_F11, character, mod);
         case RETROK_F12: return QtKey(Qt::Key_F12, character, mod);
         case RETROK_F13: return QtKey(Qt::Key_F13, character, mod);
         case RETROK_F14: return QtKey(Qt::Key_F14, character, mod);
         case RETROK_F15: return QtKey(Qt::Key_F15, character, mod);

         case RETROK_NUMLOCK: return QtKey(Qt::Key_NumLock, character, mod);
         case RETROK_CAPSLOCK: return QtKey(Qt::Key_CapsLock, character, mod);
         case RETROK_SCROLLOCK: return QtKey(Qt::Key_ScrollLock, character, mod);
         case RETROK_RSHIFT: return QtKey(Qt::Key_Shift, character, mod);
         case RETROK_LSHIFT: return QtKey(Qt::Key_Shift, character, mod);
         case RETROK_RCTRL: return QtKey(Qt::Key_Control, character, mod);
         case RETROK_LCTRL: return QtKey(Qt::Key_Control, character, mod);
         case RETROK_RALT: return QtKey(Qt::Key_Alt, character, mod);
         case RETROK_LALT: return QtKey(Qt::Key_Alt, character, mod);
         case RETROK_RMETA: return QtKey(Qt::Key_Meta, character, mod);
         case RETROK_LMETA: return QtKey(Qt::Key_Meta, character, mod);
         case RETROK_LSUPER: return QtKey(Qt::Key_Super_L, character, mod);
         case RETROK_RSUPER: return QtKey(Qt::Key_Super_R, character, mod);
         case RETROK_MODE: return QtKey(Qt::Key_Mode_switch, character, mod);
         case RETROK_COMPOSE: return QtKey(Qt::Key_Multi_key, character, mod);

         case RETROK_HELP: return QtKey(Qt::Key_Help, character, mod);
         case RETROK_PRINT: return QtKey(Qt::Key_Print, character, mod);
         case RETROK_SYSREQ: return QtKey(Qt::Key_SysReq, character, mod);
         case RETROK_BREAK: return QtKey(Qt::Key_Pause, character, mod);
         case RETROK_MENU: return QtKey(Qt::Key_Menu, character, mod);
         case RETROK_POWER: return QtKey(Qt::Key_PowerOff, character, mod);
         case RETROK_EURO: break;
         case RETROK_UNDO: return QtKey(Qt::Key_Undo, character, mod);

         default:
            break;
      }
   }

   return QtKey(static_cast<Qt::Key>(0));
}

void NETRETROPAD_CORE_PREFIX(retro_run)(void)
{
   int offset;
   int i;
   bool mouse_left;
   bool mouse_right;
   uint16_t new_x_coord;
   uint16_t new_y_coord;

   /* Update input states and send them if needed */
   retropad_update_input();

   mouse_left = mouse.value[DESC_OFFSET(&mouse, 0, 0, RETRO_DEVICE_ID_MOUSE_LEFT)];
   mouse_right = mouse.value[DESC_OFFSET(&mouse, 0, 0, RETRO_DEVICE_ID_MOUSE_RIGHT)];

   new_x_coord = x_coord + mouse.value[DESC_OFFSET(&mouse, 0, 0, RETRO_DEVICE_ID_MOUSE_X)];
   new_y_coord = y_coord + mouse.value[DESC_OFFSET(&mouse, 0, 0, RETRO_DEVICE_ID_MOUSE_Y)];

   browserWin->onMouseInput(QtMouse(QPoint(x_coord, y_coord), QPoint(new_x_coord, new_y_coord), mouse_left, mouse_right));

   x_coord += new_x_coord;
   y_coord += new_y_coord;

   /* Combine RetroPad input states into one value */
   /*for (i = joypad.id_min; i <= joypad.id_max; i++) {
      offset = DESC_OFFSET(&joypad, 0, 0, i);
      if (joypad.value[offset])
         input_state |= 1 << i;
   }*/

   for (i = joypad.id_min; i <= joypad.id_max; i++)
   {
      offset = DESC_OFFSET(&joypad, 0, 0, i);

      if (joypad.value[offset])
         browserWin->onRetroPadInput(offset);
   }

   /*for (i = keyboard.id_min; i <= keyboard.id_max; i++)
   {
      offset = DESC_OFFSET(&keyboard, 0, 0, i);

      if (keyboard.value[offset])
         browserWin->onRetroKeyInput(retrokey_to_qt(offset));
   }*/

   browserWin->render();
   browserApp->processEvents();

   NETRETROPAD_CORE_PREFIX(video_cb)(browserWin->getImage(), WIDTH, HEIGHT, WIDTH * 4);
}

static void keyboard_cb(bool down, unsigned keycode,
      uint32_t character, uint16_t mod)
{
   log_cb(RETRO_LOG_INFO, "Down: %s, Code: %d, Char: %u, Mod: %u.\n",
         down ? "yes" : "no", keycode, character, mod);

   browserWin->onRetroKeyInput(retrokey_to_qt(keycode, character, mod), down);
}

bool NETRETROPAD_CORE_PREFIX(retro_load_game)(const struct retro_game_info *)
{
   netretropad_check_variables();

   struct retro_keyboard_callback cb = { keyboard_cb };
   environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &cb);

   return true;
}

void NETRETROPAD_CORE_PREFIX(retro_unload_game)(void)
{}

unsigned NETRETROPAD_CORE_PREFIX(retro_get_region)(void)
{
   return RETRO_REGION_NTSC;
}

bool NETRETROPAD_CORE_PREFIX(retro_load_game_special)(unsigned type,
      const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t NETRETROPAD_CORE_PREFIX(retro_serialize_size)(void)
{
   return 0;
}

bool NETRETROPAD_CORE_PREFIX(retro_serialize)(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool NETRETROPAD_CORE_PREFIX(retro_unserialize)(const void *data,
      size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void *NETRETROPAD_CORE_PREFIX(retro_get_memory_data)(unsigned id)
{
   (void)id;
   return NULL;
}

size_t NETRETROPAD_CORE_PREFIX(retro_get_memory_size)(unsigned id)
{
   (void)id;
   return 0;
}

void NETRETROPAD_CORE_PREFIX(retro_cheat_reset)(void)
{}

void NETRETROPAD_CORE_PREFIX(retro_cheat_set)(unsigned idx,
      bool enabled, const char *code)
{
   (void)idx;
   (void)enabled;
   (void)code;
}

#include <Display.h>
#include <Utils.h>
#include <SDL.h>
#include <Surface.h>
#include <ExternalInterface.h>

namespace nme
{

void SetGlobalPollMethod(Stage::PollMethod inMethod);

class SDLSurf : public Surface
{
public:
   SDLSurf(SDL_Surface *inSurf,uint32 inFlags,bool inDelete) : mSurf(inSurf)
   {
      mDelete = inDelete;
   }
   ~SDLSurf()
   {
      if (mDelete)
         SDL_FreeSurface(mSurf);
   }

   int Width() const  { return mSurf->w; }
   int Height() const  { return mSurf->h; }
   PixelFormat Format()  const
   {
      if (mSurf->flags & SDL_SRCALPHA)
          return pfARGB;
      return pfXRGB;
   }
   const uint8 *GetBase() const { return (const uint8 *)mSurf->pixels; }
   int GetStride() const { return mSurf->pitch; }

   void Clear(uint32 inColour,const Rect *inRect)
   {
      SDL_Rect r;
      SDL_Rect *rect_ptr = 0;
      if (inRect)
      {
         rect_ptr = &r;
         r.x = inRect->x;
         r.y = inRect->y;
         r.w = inRect->w;
         r.h = inRect->h;
      }

      SDL_FillRect(mSurf,rect_ptr,SDL_MapRGBA(mSurf->format,
            inColour>>16, inColour>>8, inColour, inColour>>24 )  );
   }

   RenderTarget BeginRender(const Rect &inRect)
   {
      if (SDL_MUSTLOCK(mSurf) )
         SDL_LockSurface(mSurf);
      return RenderTarget(Rect(Width(),Height()), Format(),
         (uint8 *)mSurf->pixels, mSurf->pitch);
   }
   void EndRender()
   {
      if (SDL_MUSTLOCK(mSurf) )
         SDL_UnlockSurface(mSurf);
   }

   void BlitTo(const RenderTarget &outTarget,
               const Rect &inSrcRect,int inPosX, int inPosY,
               BlendMode inBlend, const BitmapCache *inMask,
               uint32 inTint=0xffffff )
   {
   }


   SDL_Surface *mSurf;
   bool  mDelete;
};





class SDLStage : public Stage
{
public:
   SDLStage(SDL_Surface *inSurface,uint32 inFlags,bool inIsOpenGL)
   {
      mIsOpenGL = inIsOpenGL;
      mSDLSurface = inSurface;
      if (mIsOpenGL)
      {
         mOpenGLContext = HardwareContext::CreateOpenGL(0,0);
         mOpenGLContext->SetWindowSize(inSurface->w, inSurface->h);
         mPrimarySurface = new HardwareSurface(mOpenGLContext);
      }
      else
      {
         mOpenGLContext = 0;
         mPrimarySurface = new SDLSurf(inSurface,inFlags,inIsOpenGL);
      }
      mPrimarySurface->IncRef();
   }
   ~SDLStage()
   {
      if (!mIsOpenGL)
         SDL_FreeSurface(mSDLSurface);
      mPrimarySurface->DecRef();
   }

   void ProcessEvent(Event &inEvent)
   {
      HandleEvent(inEvent);
   }

   void Flip()
   {
      if (mIsOpenGL)
      {
         SDL_GL_SwapBuffers();
      }
      else
      {
         SDL_Flip( mSDLSurface );
      }
   }
   void GetMouse()
   {
   }

    void SetPollMethod(PollMethod inMethod)
    {
       SetGlobalPollMethod(inMethod);
    }


   Surface *GetPrimarySurface()
   {
      return mPrimarySurface;
   }

   HardwareContext *mOpenGLContext;
   SDL_Surface *mSDLSurface;
   Surface     *mPrimarySurface;
   double       mFrameRate;
   bool         mIsOpenGL;
};


class SDLFrame : public Frame
{
public:
   SDLFrame(SDL_Surface *inSurface, uint32 inFlags, bool inIsOpenGL)
   {
      mFlags = inFlags;
      mStage = new SDLStage(inSurface,mFlags,inIsOpenGL);
      mStage->IncRef();
      // SetTimer(mHandle,timerFrame, 10,0);
   }
   ~SDLFrame()
   {
      mStage->DecRef();
   }

   void ProcessEvent(Event &inEvent)
   {
      mStage->ProcessEvent(inEvent);
   }
   // --- Frame Interface ----------------------------------------------------

   void SetTitle()
   {
   }
   void SetIcon()
   {
   }
   Stage *GetStage()
   {
      return mStage;
   }


   SDLStage *mStage;
   uint32 mFlags;
};


// --- When using the simple window class -----------------------------------------------

extern "C" void MacBoot( /*void (*)()*/ );


SDLFrame *sgSDLFrame = 0;

Frame *CreateMainFrame(int inWidth,int inHeight,unsigned int inFlags,
          const char *inTitle, const char *inIcon)
{
#ifdef HX_MACOS
   MacBoot();
#endif

   unsigned int sdl_flags = 0;
   bool fullscreen = (inFlags & wfFullScreen) != 0;
   bool opengl = (inFlags & wfHardware) != 0;
   bool resizable = (inFlags & wfResizable) != 0;

   Rect r(100,100,inWidth,inHeight);

   Uint32 init_flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
   if (opengl)
      init_flags |= SDL_OPENGL;

   //  SDL_GL_DEPTH_SIZE = 0;

   init_flags |= SDL_INIT_JOYSTICK;

   if ( SDL_Init( init_flags ) == -1 )
   {
      // SDL_GetError()
      return 0;
   }

   SDL_EnableUNICODE(1);
   SDL_EnableKeyRepeat(500,30);

   sdl_flags = SDL_HWSURFACE;

   if ( resizable )
      sdl_flags |= SDL_RESIZABLE;

   if ( fullscreen )
      sdl_flags |= SDL_FULLSCREEN;


   int use_w = fullscreen ? 0 : inWidth;
   int use_h = fullscreen ? 0 : inHeight;

#ifdef IPHONE
   sdl_flags |= SDL_NOFRAME;
#else
   // SDL_WM_SetIcon( icn, NULL );
#endif

   SDL_Surface* screen = 0;
   bool is_opengl = false;
   if (opengl)
   {
      /* Initialize the display */
      SDL_GL_SetAttribute(SDL_GL_RED_SIZE,  8 );
      SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8 );
      SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8 );
      SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);
      SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

      if ( inFlags & wfVSync )
      {
         SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
      }

      sdl_flags |= SDL_OPENGL;
      if (!(screen = SDL_SetVideoMode( use_w, use_h, 32, sdl_flags | SDL_OPENGL)))
      {
         sdl_flags &= ~SDL_OPENGL;
         fprintf(stderr, "Couldn't set OpenGL mode: %s\n", SDL_GetError());
      }
      else
      {
        is_opengl = true;
        //Not great either way
        //glEnable( GL_LINE_SMOOTH );  
        //glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);  
      }
   }


   if (!screen)
   {
      sdl_flags |= SDL_DOUBLEBUF;
      screen = SDL_SetVideoMode( use_w, use_h, 32, sdl_flags );
      printf("Flags %p\n",sdl_flags);
      if (!screen)
      {
         // SDL_GetError()
         return 0;
      }
   }

	printf("Screen red mask %06x\n", screen->format->Rmask );
   HintColourOrder( opengl || screen->format->Rmask==0xff );

   #ifndef IPHONE
   SDL_WM_SetCaption( inTitle, 0 );
   #endif

   #ifdef NME_MIXER
   if ( Mix_OpenAudio( MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS,4096 )!= 0 )
      printf("unable to initialize the sound support\n");
   #endif

   sgSDLFrame =  new SDLFrame( screen, sdl_flags, is_opengl );
   return sgSDLFrame;
}

bool sgDead = false;

void TerminateMainLoop()
{
   #ifdef NME_MIXER
   Mix_CloseAudio();
   #endif
   sgDead = true;
}

static Stage::PollMethod sgPollMethod = Stage::pollAlways;
static SDL_TimerID  sgTimerID = 0;

Uint32 DoPoll(Uint32 interval, void *)
{
    // Ping off an event - any event will force the frame check.
    SDL_Event event;
    SDL_UserEvent userevent;
    /* In this example, our callback pushes an SDL_USEREVENT event
    into the queue, and causes ourself to be called again at the
    same interval: */
    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;
    event.type = SDL_USEREVENT;
    event.user = userevent;
    SDL_PushEvent(&event);
    return interval;
}


void SetGlobalPollMethod(Stage::PollMethod inMethod)
{
   if (inMethod!=sgPollMethod)
   {
      sgPollMethod = inMethod;
      if (sgPollMethod==Stage::pollTimer)
      {
         sgTimerID = SDL_AddTimer(1, DoPoll, 0);
      }
      else if (sgTimerID)
      {
         SDL_RemoveTimer(sgTimerID);
         sgTimerID = 0;
      }
   }
}

void ProcessEvent(SDL_Event &inEvent)
{
  switch(inEvent.type)
   {
      case SDL_QUIT:
      {
         Event close(etQuit);
         sgSDLFrame->ProcessEvent(close);
         break;
      }
      case SDL_MOUSEMOTION:
      {
         Event mouse(etMouseMove,inEvent.motion.x,inEvent.motion.y);
         sgSDLFrame->ProcessEvent(mouse);
         break;
      }
      case SDL_VIDEORESIZE:
      {
         break;
      }
   }
}

#ifdef NME_MIXER
int id = soundGetNextDoneChannel();
if (id>=0)
{
}
#endif



void MainLoop()
{
   SDL_Event event;
   while(!sgDead)
   {
      while ( SDL_PollEvent(&event) )
      {
         ProcessEvent(event);
         if (sgDead) break;
      }
     
      if (sgPollMethod!=Stage::pollNever)
      {
         Event poll(etPoll);
         sgSDLFrame->ProcessEvent(poll);
      }
      
      if (!sgDead && sgPollMethod!=Stage::pollAlways)
      {
         SDL_WaitEvent(&event);
         ProcessEvent(event);
      }
   }

   Event kill(etDestroyHandler);
   sgSDLFrame->ProcessEvent(kill);
   SDL_Quit();
}


Frame *CreateTopLevelWindow(int inWidth,int inHeight,unsigned int inFlags, wchar_t *inTitle, wchar_t *inIcon )
{

}



} // end namespace nme


         #if 0
         if (event.type == SDL_KEYDOWN)
         {
       //alloc_field( evt, val_id( "type" ), alloc_int( et_keydown ) );
       //alloc_field( evt, val_id( "key" ), alloc_int( event.key.keysym.sym ) );
       //alloc_field( evt, val_id( "char" ), alloc_int( event.key.keysym.unicode ) );
       //alloc_field( evt, val_id( "shift" ), alloc_bool( event.key.keysym.mod & KMOD_SHIFT ) );
       //alloc_field( evt, val_id( "ctrl" ), alloc_bool( event.key.keysym.mod & KMOD_CTRL ) );
       //alloc_field( evt, val_id( "alt" ), alloc_bool( event.key.keysym.mod & KMOD_ALT ) );
       //return evt;
         }
         if (event.type == SDL_KEYUP)
         {
         }
         if (event.type == SDL_MOUSEMOTION)
         {
       alloc_field( evt, val_id( "type" ), alloc_int( et_motion ) );
       alloc_field( evt, val_id( "state" ), alloc_int( event.motion.state ) );
       alloc_field( evt, val_id( "x" ), alloc_int( event.motion.x ) );
       alloc_field( evt, val_id( "y" ), alloc_int( event.motion.y ) );
       alloc_field( evt, val_id( "xrel" ), alloc_int( event.motion.xrel ) );
       alloc_field( evt, val_id( "yrel" ), alloc_int( event.motion.yrel ) );
       return evt;
         }
         if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
         {
       alloc_field( evt, val_id( "type" ), alloc_int( event.type == SDL_MOUSEBUTTONUP ?
                             et_button_up : et_button_down ) );
       alloc_field( evt, val_id( "state" ), alloc_int( event.button.state ) );
       alloc_field( evt, val_id( "x" ), alloc_int( event.button.x ) );
       alloc_field( evt, val_id( "y" ), alloc_int( event.button.y ) );
       alloc_field( evt, val_id( "which" ), alloc_int( event.button.which ) );
       alloc_field( evt, val_id( "button" ), alloc_int( event.button.button ) );
       return evt;
         }
         if (event.type == SDL_JOYAXISMOTION)
         {
       alloc_field( evt, val_id( "type" ), alloc_int( et_jaxis ) );
       alloc_field( evt, val_id( "axis" ), alloc_int( event.jaxis.axis ) );
       alloc_field( evt, val_id( "value" ), alloc_int( event.jaxis.value ) );
       alloc_field( evt, val_id( "which" ), alloc_int( event.jaxis.which ) );
       return evt;
         }
         if (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP)
         {
       alloc_field( evt, val_id( "type" ), alloc_int( et_jbutton ) );
       alloc_field( evt, val_id( "button" ), alloc_int( event.jbutton.button ) );
       alloc_field( evt, val_id( "state" ), alloc_int( event.jbutton.state ) );
       alloc_field( evt, val_id( "which" ), alloc_int( event.jbutton.which ) );
       return evt;
         }
         if (event.type == SDL_JOYHATMOTION)
         {
       alloc_field( evt, val_id( "type" ), alloc_int( et_jhat ) );
       alloc_field( evt, val_id( "button" ), alloc_int( event.jhat.hat ) );
       alloc_field( evt, val_id( "value" ), alloc_int( event.jhat.value ) );
       alloc_field( evt, val_id( "which" ), alloc_int( event.jhat.which ) );
       return evt;
         }
         if (event.type == SDL_JOYBALLMOTION)
         {
       alloc_field( evt, val_id( "type" ), alloc_int( et_jball ) );
       alloc_field( evt, val_id( "ball" ), alloc_int( event.jball.ball ) );
       alloc_field( evt, val_id( "xrel" ), alloc_int( event.jball.xrel ) );
       alloc_field( evt, val_id( "yrel" ), alloc_int( event.jball.yrel ) );
       alloc_field( evt, val_id( "which" ), alloc_int( event.jball.which ) );
       return evt;
         }

         if (event.type==SDL_VIDEORESIZE)
         {
       alloc_field( evt, val_id( "type" ), alloc_int( et_resize ) );
       alloc_field( evt, val_id( "width" ), alloc_int( event.resize.w ) );
       alloc_field( evt, val_id( "height" ), alloc_int( event.resize.h ) );
       return evt;
         }
         #endif


#if 0
/*
 */


// As opposed to opengl hardware ..
static bool sUseSystemHardware = false;

SDL_Surface *CreateRGB(int inWidth,int inHeight,bool inAlpha,bool inHardware)
{
   if (sgC0IsRed)
      return SDL_CreateRGBSurface((inHardware?SDL_HWSURFACE:SDL_SWSURFACE)|(inAlpha ? SDL_SRCALPHA : 0),
            inWidth, inHeight, (inHardware||inAlpha) ? 32 : 24,
            0x000000ff, 0x0000ff00, 0x00ff0000, inAlpha ? 0xff000000 : 0);
   else
      return SDL_CreateRGBSurface((inHardware?SDL_HWSURFACE:SDL_SWSURFACE)|(inAlpha ? SDL_SRCALPHA : 0),
            inWidth, inHeight, (inHardware||inAlpha) ? 32 : 24,
            0x00ff0000, 0x0000ff00, 0x000000ff, inAlpha ? 0xff000000 : 0);
}



static value nme_surface_clear( value surf, value c )
{
   val_check_kind( surf, k_surf );

   val_check( c, int );
   SDL_Surface* scr = SURFACE(surf);

   Uint8 r = RRGB( c );
   Uint8 g = GRGB( c );
   Uint8 b = BRGB( c );

        #ifdef NME_ANY_GL
        if (IsOpenGLScreen(scr))
        {
           int w = scr->w;
           int h = scr->h;
           glDisable(GL_CLIP_PLANE0);
           glViewport(0,0,w,h);
           glMatrixMode(GL_PROJECTION);
           glLoadIdentity();
           nmeOrtho(w,h);
           glMatrixMode(GL_MODELVIEW);
           glLoadIdentity();
           glClearColor((GLclampf)(r/255.0),
                        (GLclampf)(g/255.0),
                        (GLclampf)(b/255.0),
                        (GLclampf)1.0 );
           glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        else
        #endif
        {
      SDL_FillRect( scr, NULL, SDL_MapRGB( scr->format, r, g, b ) );
        }

   return alloc_int( 0 );

}



value nme_set_clip_rect(value inSurface, value inRect)
{
   SDL_Rect rect;
   if (!val_is_null(inRect))
   {
      rect.x = (int)val_number( val_field(inRect, val_id("x")) );
      rect.y = (int)val_number( val_field(inRect, val_id("y")) );
      rect.w = (int)val_number( val_field(inRect, val_id("width")) );
      rect.h = (int)val_number( val_field(inRect, val_id("height")) );

   }
   else
      memset(&rect,0,sizeof(rect));

   if (val_is_kind(inSurface,k_surf))
   {
      SDL_Surface *surface = SURFACE(inSurface);

      if (IsOpenGLScreen(surface))
      {
         if (val_is_null(inRect))
         {
            sDoScissor = false;
            glDisable(GL_SCISSOR_TEST);
         }
         else
         {
            sDoScissor = true;
            glEnable(GL_SCISSOR_TEST);
            sScissorRect = rect;
            glScissor(sScissorRect.x,sScissorRect.y,
                      sScissorRect.w,sScissorRect.h);
         }
      }
      else
      {
         if (val_is_null(inRect))
         {
            SDL_SetClipRect(surface,0);
            SDL_GetClipRect(surface,&rect);
         }
         else
         {
            SDL_SetClipRect(surface,&rect);
         }
      }
   }

   return AllocRect(rect);
}

value nme_get_clip_rect(value inSurface)
{
   SDL_Rect rect;
   memset(&rect,0,sizeof(rect));

   if (val_is_kind(inSurface,k_surf))
   {
      SDL_Surface *surface = SURFACE(inSurface);

      if (IsOpenGLScreen(surface))
      {
         if (sDoScissor)
            rect = sScissorRect;
         else
         {
            rect.w = sOpenGLScreen->w;
            rect.h = sOpenGLScreen->h;
         }
      }
      else
      {
         SDL_GetClipRect(surface,&rect);
      }
   }

   return AllocRect(rect);
}

SDL_Cursor *CreateCursor(const char *image[],int inHotX,int inHotY)
{
  int i, row, col;
  Uint8 data[4*32];
  Uint8 mask[4*32];

  i = -1;
  for ( row=0; row<32; ++row ) {
    for ( col=0; col<32; ++col ) {
      if ( col % 8 ) {
        data[i] <<= 1;
        mask[i] <<= 1;
      } else {
        ++i;
        data[i] = mask[i] = 0;
      }
      switch (image[row][col]) {
        case 'X':
          data[i] |= 0x01;
          mask[i] |= 0x01;
          break;
        case '.':
          mask[i] |= 0x01;
          break;
        case ' ':
          break;
      }
    }
  }
  return SDL_CreateCursor(data, mask, 32, 32, inHotX, inHotY);
}

static const char *sTextCursorData[] = {
  "                                ",
  "                                ",
  "XX XX                           ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "  X                             ",
  "XX XX                           ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
};



#define CURSOR_NONE   0
#define CURSOR_NORMAL 1
#define CURSOR_TEXT   2

SDL_Cursor *sDefaultCursor = 0;
SDL_Cursor *sTextCursor = 0;


value nme_set_cursor(value inCursor)
{
   val_check(inCursor,int);

   if (sDefaultCursor==0)
      sDefaultCursor = SDL_GetCursor();

   int c = val_int(inCursor);

   if (c==CURSOR_NONE)
      SDL_ShowCursor(false);
   else
   {
      SDL_ShowCursor(true);

      if (c==CURSOR_NORMAL)
         SDL_SetCursor(sDefaultCursor);
      else
      {
         if (sTextCursor==0)
            sTextCursor = CreateCursor(sTextCursorData,1,13);
         SDL_SetCursor(sTextCursor);
      }
   }

   return alloc_int(0);
}

value nme_get_mouse_position()
{
   int x,y;

   #ifdef SDL13
   SDL_GetMouseState(0,&x,&y);
   #else
   SDL_GetMouseState(&x,&y);
   #endif

   value pos = alloc_empty_object();
   alloc_field( pos, val_id( "x" ), alloc_int( x ) );
   alloc_field( pos, val_id( "y" ), alloc_int( y ) );
   return pos;
}


#define NME_FULLSCREEN 0x0001
#define NME_OPENGL_FLAG  0x0002
#define NME_RESIZABLE  0x0004
#define NME_HWSURF     0x0008
#define NME_VSYNC      0x0010

#ifdef __APPLE__

extern "C" void MacBoot( /*void (*)()*/ );

#endif

value nme_resize_surface(value inW, value inH)
{
   val_check( inW, int );
   val_check( inH, int );
   int w = val_int(inW);
   int h = val_int(inH);
   SDL_Surface *screen = gCurrentScreen;

   #ifndef __APPLE__
   if (is_opengl)
   {
      // Little hack to help windows
      screen->w = w;
      screen->h = h;
   }
   else
   #endif
   {
      nme_resize_id ++;
      // Calling this recreates the gl context and we loose all our textures and
      // display lists. So Work around it.
      gCurrentScreen = screen = SDL_SetVideoMode(w, h, 32, sdl_flags );
   }

   return alloc_abstract( k_surf, screen );
}





#endif
/* OpenGL 1.1 presentation only; all calls belong to the SDL video thread.
   Resolve through SDL so neither Windows nor Linux needs a GL link dependency. */
#include <SDL/SDL_opengl.h>
#define WG_GL_FUNCTIONS(X) \
 X(const GLubyte *,GetString,(GLenum)) \
 X(GLenum,GetError,(void)) \
 X(void,GetIntegerv,(GLenum,GLint *)) \
 X(void,GenTextures,(GLsizei,GLuint *)) \
 X(void,DeleteTextures,(GLsizei,const GLuint *)) \
 X(void,BindTexture,(GLenum,GLuint)) \
 X(void,TexParameteri,(GLenum,GLenum,GLint)) \
 X(void,TexImage2D,(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const GLvoid *)) \
 X(void,TexSubImage2D,(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const GLvoid *)) \
 X(void,PixelStorei,(GLenum,GLint)) \
 X(void,Viewport,(GLint,GLint,GLsizei,GLsizei)) \
 X(void,ClearColor,(GLclampf,GLclampf,GLclampf,GLclampf)) \
 X(void,Clear,(GLbitfield)) \
 X(void,Enable,(GLenum)) \
 X(void,Disable,(GLenum)) \
 X(void,MatrixMode,(GLenum)) \
 X(void,LoadIdentity,(void)) \
 X(void,Begin,(GLenum)) \
 X(void,End,(void)) \
 X(void,TexCoord2f,(GLfloat,GLfloat)) \
 X(void,Vertex2f,(GLfloat,GLfloat))
#define WG_GL_DECLARE(result,name,args) static result (APIENTRY *wg_gl##name) args;
WG_GL_FUNCTIONS(WG_GL_DECLARE)
#undef WG_GL_DECLARE
/* 0=auto, 1=OpenGL (strict), 2=surface. */
static int render_choice,gl_active,gl_acceleration=-1;
static const char *gl_library_path;
static char gl_library_storage[32768];
static int gl_library_loaded;
static char gl_report[1024];
static int set_gl_acceleration(const char *value)
{
    if(!strcmp(value,"auto"))gl_acceleration=-1;
    else if(!strcmp(value,"hardware"))gl_acceleration=1;
    else if(!strcmp(value,"software"))gl_acceleration=0;
    else return 0;
    return 1;
}
static GLuint gl_texture;
static unsigned char gl_pixels[320*200*3];
static int set_renderer(const char *value)
{
    if(!strcmp(value,"auto"))render_choice=0;
    else if(!strcmp(value,"opengl"))render_choice=1;
    else if(!strcmp(value,"surface"))render_choice=2;
    else return 0;
    return 1;
}
static int gl_software_renderer(const char *renderer)
{
    char lower[256];size_t i;
    if(!renderer)return 1;
    for(i=0;i<sizeof(lower)-1 && renderer[i];++i) {
        unsigned char c=(unsigned char)renderer[i];
        lower[i]=(char)(c>='A' && c<='Z'?c+('a'-'A'):c);
    }
    lower[i]=0;
    return strstr(lower,"software") || strstr(lower,"llvmpipe") ||
        strstr(lower,"softpipe") || strstr(lower,"gdi generic") ||
        strstr(lower,"mesa x11");
}
static void gl_close(void)
{
    if(gl_texture)wg_glDeleteTextures(1,&gl_texture);
    gl_texture=0;gl_active=0;
}
static int gl_open(void)
{
    const char *renderer;GLint maximum=0;void *address;int acceleration=-1,software;
#define WG_GL_LOAD(result,name,args) \
    address=SDL_GL_GetProcAddress("gl" #name); \
    if(!address) {if(render_choice==1)fprintf(stderr,"SDL1 OpenGL: missing gl%s\n",#name);return 0;} \
    memcpy((void *)&wg_gl##name,&address,sizeof(wg_gl##name));
    WG_GL_FUNCTIONS(WG_GL_LOAD)
#undef WG_GL_LOAD
    renderer=(const char *)wg_glGetString(GL_RENDERER);
    (void)SDL_GL_GetAttribute(SDL_GL_ACCELERATED_VISUAL,&acceleration);
    software=gl_software_renderer(renderer) || acceleration==0;
    if((gl_acceleration==0 && !software) || (gl_acceleration==1 && (software || acceleration!=1))) {
        fprintf(stderr,"SDL1 OpenGL: requested %s rendering is unavailable (renderer: %s)\n",
            gl_acceleration==0?"software":"hardware",renderer?renderer:"unknown");return 0;
    }
    snprintf(gl_report,sizeof(gl_report),
        "  GL library: %s\n  GL acceleration requested: %s\n  GL vendor: %s\n  GL renderer: %s\n  GL version: %s\n  GL acceleration: %s\n  GL filtering: nearest-neighbor\n",
        gl_library_path?gl_library_path:"system default",
        gl_acceleration<0?"auto":gl_acceleration?"hardware":"software",
        wg_glGetString(GL_VENDOR),renderer?renderer:"unknown",wg_glGetString(GL_VERSION),
        software?"software":acceleration==1?"hardware":"unknown");
    wg_glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    if(maximum<512)return 0;
    wg_glGenTextures(1,&gl_texture);wg_glBindTexture(GL_TEXTURE_2D,gl_texture);
    wg_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    wg_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    wg_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    wg_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    /* Power-of-two storage works on GL 1.1; upload only the 320x200 image. */
    wg_glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,512,256,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);
    if(!gl_texture || wg_glGetError()!=GL_NO_ERROR) {gl_close();return 0;}
    wg_glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    wg_glDisable(GL_DEPTH_TEST);wg_glDisable(GL_BLEND);wg_glDisable(GL_DITHER);
    wg_glEnable(GL_TEXTURE_2D);
    wg_glMatrixMode(GL_PROJECTION);wg_glLoadIdentity();
    wg_glMatrixMode(GL_MODELVIEW);wg_glLoadIdentity();
    wg_glClearColor(0,0,0,1);gl_active=1;
    return 1;
}
static void gl_present(const uint8_t *pixels,const uint8_t *palette,int left,int top,int w,int h,int height)
{
    unsigned i;
    for(i=0;i<320U*200U;++i)memcpy(gl_pixels+i*3,palette+pixels[i]*3,3);
    wg_glBindTexture(GL_TEXTURE_2D,gl_texture);
    wg_glTexSubImage2D(GL_TEXTURE_2D,0,0,0,320,200,GL_RGB,GL_UNSIGNED_BYTE,gl_pixels);
    wg_glClear(GL_COLOR_BUFFER_BIT);
    wg_glViewport(left,height-top-h,w,h);
    wg_glBegin(GL_QUADS);
    wg_glTexCoord2f(0,0);wg_glVertex2f(-1,1);
    wg_glTexCoord2f(320.0f/512,0);wg_glVertex2f(1,1);
    wg_glTexCoord2f(320.0f/512,200.0f/256);wg_glVertex2f(1,-1);
    wg_glTexCoord2f(0,200.0f/256);wg_glVertex2f(-1,-1);
    wg_glEnd();SDL_GL_SwapBuffers();
}

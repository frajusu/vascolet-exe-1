/* Browser-facing SDL 1.2 video/input/audio adapter. MIT license.
 * Native SDL still implements image surfaces and software drawing. Only the
 * unavailable Linux display/audio devices are replaced by browser messages.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <termios.h>
#include <poll.h>
#include <dirent.h>
#include <sys/stat.h>
typedef struct {int16_t x,y;uint16_t w,h;} Rect;
typedef struct {void*palette;uint8_t bits,bytes,rloss,gloss,bloss,aloss,rshift,gshift,bshift,ashift;uint32_t rmask,gmask,bmask,amask,colorkey;uint8_t alpha;} Format;
typedef struct Surface {uint32_t flags;Format*format;int w,h;uint16_t pitch;void*pixels;int offset;void*hwdata;Rect clip;uint32_t unused,locked;void*map;uint32_t version;int refcount;} Surface;
typedef struct {uint32_t flags,memory;Format*format;int w,h;} VideoInfo;
static Surface*screen;
static Surface*uploaded[2048];
static int uploaded_count,gpu_frame;
static void*native;
static uint8_t commands[131072];
static uint32_t command_size;
static void*symbol(const char*name){if(!native)native=dlopen("libNAT-1.2.so.0",RTLD_NOW|RTLD_LOCAL);void*f=dlsym(native,name);if(!f){fprintf(stderr,"SDL bridge missing %s: %s\n",name,dlerror());_exit(90);}return f;}
static void flush_commands(void){if(command_size){write(1,commands,command_size);command_size=0;}}
static void emit(uint32_t kind,const void*data,uint32_t size){uint32_t header[]={0x4c445356,kind,size};if(size+12>sizeof(commands)){flush_commands();uint8_t*message=malloc(size+12);memcpy(message,header,12);if(size)memcpy(message+12,data,size);write(1,message,size+12);free(message);return;}if(command_size+size+12>sizeof(commands))flush_commands();memcpy(commands+command_size,header,12);if(size)memcpy(commands+command_size+12,data,size);command_size+=size+12;if(kind==2||kind==13||kind==22)flush_commands();}
static void named(uint32_t kind,uint32_t id,const char*name){size_t len=strlen(name)+1;char*data=malloc(len+4);memcpy(data,&id,4);memcpy(data+4,name,len);emit(kind,data,len+4);free(data);}
static void upload(Surface*s){for(int n=0;n<uploaded_count;n++)if(uploaded[n]==s)return;int(*lock)(Surface*)=symbol("SDL_LockSurface");void(*unlock)(Surface*)=symbol("SDL_UnlockSurface");if(lock(s)!=0||!s->pixels)return;if(uploaded_count<2048)uploaded[uploaded_count++]=s;uint32_t meta[]={(uint32_t)(uintptr_t)s,(uint32_t)s->w,(uint32_t)s->h,s->pitch,s->format->bytes,s->format->rmask,s->format->gmask,s->format->bmask,s->format->amask,s->flags,s->format->colorkey};uint8_t*data=malloc(sizeof(meta)+s->pitch*s->h);memcpy(data,meta,sizeof(meta));memcpy(data+sizeof(meta),s->pixels,s->pitch*s->h);emit(5,data,sizeof(meta)+s->pitch*s->h);free(data);unlock(s);}
int SDL_UpperBlit(Surface*src,Rect*sr,Surface*dest,Rect*dr){if(!src||!dest)return-1;if(dest!=screen){int(*real)(Surface*,Rect*,Surface*,Rect*)=symbol("SDL_UpperBlit");return real(src,sr,dest,dr);}upload(src);int32_t data[]={(uint32_t)(uintptr_t)src,sr?sr->x:0,sr?sr->y:0,sr?sr->w:src->w,sr?sr->h:src->h,dr?dr->x:0,dr?dr->y:0,(src->flags&0x10000)?src->format->alpha:255};emit(6,data,sizeof(data));gpu_frame=1;return 0;}
int SDL_FillRect(Surface*dest,Rect*rect,uint32_t color){if(dest!=screen){int(*real)(Surface*,Rect*,uint32_t)=symbol("SDL_FillRect");return real(dest,rect,color);}uint32_t data[]={rect?rect->x:0,rect?rect->y:0,rect?rect->w:dest->w,rect?rect->h:dest->h,color,dest->format->bits,dest->w,dest->h};emit(7,data,sizeof(data));gpu_frame=1;return 0;}
void SDL_FreeSurface(Surface*s){for(int n=0;n<uploaded_count;n++)if(uploaded[n]==s){uploaded[n]=uploaded[--uploaded_count];uint32_t id=(uintptr_t)s;emit(8,&id,4);break;}void(*real)(Surface*)=symbol("SDL_FreeSurface");real(s);}
static uint8_t keys[323],oldkeys[323],mousebuttons,oldbuttons;
static uint16_t mx,my,oldmx,oldmy;
static uint8_t events[256][24];
static unsigned event_read,event_write;
static void channel_ended(unsigned channel);
static void export_saves(void);
static uint8_t*new_event(void){if(event_write-event_read>=256)event_read++;uint8_t*e=events[event_write++%256];memset(e,0,24);return e;}
static void key_event(unsigned code,unsigned down){if(code>=323||keys[code]==down)return;keys[code]=down;uint8_t*e=new_event();e[0]=down?2:3;e[2]=down;memcpy(e+8,&code,4);uint16_t unicode=code<128?code:0;memcpy(e+16,&unicode,2);}
static void mouse_event(unsigned x,unsigned y,unsigned buttons){if(x!=mx||y!=my){uint8_t*e=new_event();e[0]=4;e[2]=buttons;uint16_t xx=x,yy=y;int16_t dx=x-mx,dy=y-my;memcpy(e+4,&xx,2);memcpy(e+6,&yy,2);memcpy(e+8,&dx,2);memcpy(e+10,&dy,2);}if(buttons!=mousebuttons){uint8_t*e=new_event();e[0]=buttons?5:6;e[2]=1;e[3]=buttons?1:0;uint16_t xx=x,yy=y;memcpy(e+4,&xx,2);memcpy(e+6,&yy,2);}mx=x;my=y;mousebuttons=buttons;}
static void input(void){static int initialized,at;static char line[96];if(!initialized){initialized=1;struct termios settings;if(tcgetattr(0,&settings)==0){cfmakeraw(&settings);tcsetattr(0,TCSANOW,&settings);}int flags=fcntl(0,F_GETFL);if(flags>=0)fcntl(0,F_SETFL,flags|O_NONBLOCK);}struct pollfd descriptor={0,POLLIN,0};while(poll(&descriptor,1,0)>0){char byte;if(read(0,&byte,1)!=1)break;if(byte=='\n'||byte=='\r'){line[at]=0;unsigned a,b,c;if(sscanf(line,"K:%u:%u",&a,&b)==2)key_event(a,b);else if(sscanf(line,"M:%u:%u:%u",&a,&b,&c)==3)mouse_event(a,b,c);else if(sscanf(line,"A:%u:%u",&a,&b)==2&&!b)channel_ended(a);else if(!strcmp(line,"E"))export_saves();at=0;}else if(at<95)line[at++]=byte;else at=0;}}
int SDL_Init(uint32_t flags){int(*init)(uint32_t)=symbol("SDL_Init"),(*sub)(uint32_t)=symbol("SDL_InitSubSystem");int result=init(0x100000);return result?result:sub(flags&0x21);}
int SDL_InitSubSystem(uint32_t flags){int(*real)(uint32_t)=symbol("SDL_InitSubSystem");return real(flags&0x21);}
void SDL_QuitSubSystem(uint32_t flags){}
Surface*SDL_SetVideoMode(int w,int h,int bpp,uint32_t flags){Surface*(*real)(int,int,int,uint32_t)=symbol("SDL_SetVideoMode");screen=real(w,h,bpp,0);return screen;}
Surface*SDL_GetVideoSurface(void){return screen;}
const VideoInfo*SDL_GetVideoInfo(void){static VideoInfo info={0};if(!info.format){Surface*(*create)(uint32_t,int,int,int,uint32_t,uint32_t,uint32_t,uint32_t)=symbol("SDL_CreateRGBSurface");Surface*s=create(0,1,1,16,0xf800,0x07e0,0x001f,0);info.format=s->format;info.w=800;info.h=600;info.flags=2;}return &info;}
int SDL_VideoModeOK(int w,int h,int bpp,uint32_t flags){return bpp?bpp:16;}
Surface*SDL_DisplayFormat(Surface*s){if(!s)return 0;Surface*(*convert)(Surface*,Format*,uint32_t)=symbol("SDL_ConvertSurface");return convert(s,screen?screen->format:s->format,0);}
int SDL_Flip(Surface*s){if(!s)s=screen;if(!s||!s->pixels)return 0;if(gpu_frame){uint32_t size[]={s->w,s->h};emit(2,size,sizeof(size));return 0;}uint32_t meta[]={(uint32_t)s->w,(uint32_t)s->h,s->pitch,s->format->bytes,s->format->rmask,s->format->gmask,s->format->bmask,s->format->amask};uint32_t size=sizeof(meta)+s->pitch*s->h;uint32_t head[]={0x4c445356,1,size};write(1,head,sizeof(head));write(1,meta,sizeof(meta));write(1,s->pixels,s->pitch*s->h);return 0;}
void SDL_UpdateRects(Surface*s,int n,Rect*r){SDL_Flip(s);}
int SDL_PollEvent(void*event){input();if(event_read==event_write)return 0;if(event)memcpy(event,events[event_read++%256],24);return 1;}
uint8_t SDL_GetMouseState(int*x,int*y){input();if(x)*x=mx;if(y)*y=my;return mousebuttons;}
int SDL_EnableUNICODE(int enable){return 1;}
int SDL_ShowCursor(int toggle){return 1;}
void SDL_WarpMouse(uint16_t x,uint16_t y){mx=x;my=y;}
void SDL_WM_SetCaption(const char*title,const char*icon){}
int SDL_GL_SetAttribute(int attribute,int value){return 0;}
void SDL_GL_SwapBuffers(void){SDL_Flip(screen);}
typedef struct {void*rw;char*name;} FileRecord;
static FileRecord records[1024];
void*SDL_RWFromFile(const char*file,const char*mode){void*(*f)(const char*,const char*)=symbol("SDL_RWFromFile");void*rw=f(file,mode);for(int n=0;n<1024;n++)if(!records[n].rw||records[n].rw==rw){records[n].rw=rw;free(records[n].name);records[n].name=strdup(file);break;}return rw;}
typedef struct {int allocated;uint8_t*buffer;uint32_t length;uint8_t volume;uint32_t id;} Chunk;
static uint32_t next_id=1;
static Chunk*channels[64];
static int volumes[64],paused[64],music_volume=128,music_playing,music_paused;
static uint32_t music_id;
static void channel_ended(unsigned channel){if(channel<64)channels[channel]=0;else if(channel==65535)music_playing=0;}
int Mix_OpenAudio(int frequency,uint16_t format,int channels_,int chunksize){for(int n=0;n<64;n++)volumes[n]=128;return 0;}
void Mix_CloseAudio(void){emit(13,0,0);}
int Mix_AllocateChannels(int count){return count<0?32:count;}
void*Mix_LoadMUS(const char*name){uint32_t*music=malloc(4);*music=next_id++;named(10,*music,name);return music;}
Chunk*Mix_LoadWAV_RW(void*rw,int freesrc){const char*name=0;for(int n=0;n<1024;n++)if(records[n].rw==rw){name=records[n].name;break;}if(!name)return 0;Chunk*c=calloc(1,sizeof(*c));c->id=next_id++;c->volume=128;named(10,c->id,name);if(freesrc&&rw){int(*close)(void*)=((int(**)(void*))rw)[3];close(rw);}return c;}
int Mix_PlayMusic(void*m,int loops){if(!m)return-1;music_id=*(uint32_t*)m;music_playing=1;music_paused=0;int32_t data[]={music_id,-2,loops,music_volume};emit(11,data,sizeof(data));return 0;}
int Mix_PlayChannelTimed(int channel,Chunk*c,int loops,int ticks){if(!c)return-1;if(channel<0){for(channel=0;channel<64&&channels[channel];channel++){}if(channel==64)channel=0;}if(channel>=64)return-1;channels[channel]=c;int32_t data[]={c->id,channel,loops,volumes[channel]};emit(11,data,sizeof(data));return channel;}
int Mix_HaltMusic(void){music_playing=0;int32_t channel=-2;emit(12,&channel,4);return 0;}
int Mix_HaltChannel(int channel){if(channel<0){memset(channels,0,sizeof(channels));}else if(channel<64)channels[channel]=0;emit(12,&channel,4);return 0;}
int Mix_Volume(int channel,int volume){int old=channel>=0&&channel<64?volumes[channel]:128;if(volume>=0){if(channel<0)for(int n=0;n<64;n++)volumes[n]=volume;else if(channel<64)volumes[channel]=volume;int data[]={channel,volume};emit(15,data,8);}return old;}
int Mix_VolumeMusic(int volume){int old=music_volume;if(volume>=0){music_volume=volume;int data[]={-2,volume};emit(15,data,8);}return old;}
int Mix_PlayingMusic(void){return music_playing;}
int Mix_Playing(int channel){return channel>=0&&channel<64&&channels[channel]?1:0;}
int Mix_Paused(int channel){if(channel>=0&&channel<64)return paused[channel];int count=0;for(int n=0;n<64;n++)count+=paused[n];return count;}
void Mix_Pause(int channel){if(channel<0)for(int n=0;n<64;n++)paused[n]=1;else if(channel<64)paused[channel]=1;int data[]={channel,1};emit(14,data,8);}
void Mix_Resume(int channel){if(channel<0)memset(paused,0,sizeof(paused));else if(channel<64)paused[channel]=0;int data[]={channel,0};emit(14,data,8);}
void Mix_PauseMusic(void){music_paused=1;int data[]={-2,1};emit(14,data,8);}
void Mix_ResumeMusic(void){music_paused=0;int data[]={-2,0};emit(14,data,8);}
void Mix_FreeChunk(Chunk*c){free(c);}
void Mix_FreeMusic(void*m){free(m);}
Chunk*Mix_GetChunk(int channel){return channel>=0&&channel<64?channels[channel]:0;}
void Mix_ChannelFinished(void*callback){}
void Mix_HookMusicFinished(void*callback){}
static size_t export_bytes;
static int export_directory(const char*path){DIR*dir=opendir(path);if(!dir)return 0;struct dirent*entry;while((entry=readdir(dir))){if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;char full[1024];snprintf(full,sizeof(full),"%s/%s",path,entry->d_name);struct stat info;if(lstat(full,&info))continue;if(S_ISDIR(info.st_mode)){int result=export_directory(full);if(result){closedir(dir);return result;}}else if(S_ISREG(info.st_mode)){export_bytes+=info.st_size;if(export_bytes>32*1024*1024){closedir(dir);return 9;}const char*relative=full+6;uint32_t length=strlen(relative);uint8_t*data=malloc(4+length+info.st_size);memcpy(data,&length,4);memcpy(data+4,relative,length);FILE*file=fopen(full,"rb");if(!file){free(data);continue;}size_t size=fread(data+4+length,1,info.st_size,file);fclose(file);emit(21,data,4+length+size);free(data);}}closedir(dir);return 0;}
static void export_saves(void){export_bytes=0;int status=export_directory("/home");emit(22,&status,4);}

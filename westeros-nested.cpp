#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>

#include <map>

#include "westeros-nested.h"
#include "simpleshell-client-protocol.h"

#ifdef ENABLE_SBPROTOCOL
#include "simplebuffer-client-protocol.h"
#endif

#define WST_UNUSED(x) ((void)(x))

typedef struct _WstNestedConnection
{
   WstCompositor *ctx;
   struct wl_display *display;
   struct wl_registry *registry;
   struct wl_compositor *compositor;
   struct wl_shm *shm;
   struct wl_simple_shell *simpleShell;
   #ifdef ENABLE_SBPROTOCOL
   struct wl_sb *sb;
   #endif
   struct wl_surface *surface;
   struct wl_seat *seat;
   struct wl_keyboard *keyboard;
   struct wl_pointer *pointer;
   struct wl_touch *touch;
   int nestedWidth;
   int nestedHeight;
   void *nestedListenerUserData;
   WstNestedConnectionListener *nestedListener;
   bool started;
   bool stopRequested;
   pthread_t nestedThreadId;
   uint32_t pointerEnterSerial;
   std::map<struct wl_surface*, int32_t> surfaceMap;
} WstNestedConnection;


static void keyboardHandleKeymap( void *data, struct wl_keyboard *keyboard, 
                                  uint32_t format, int fd, uint32_t size )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->nestedListener->keyboardHandleKeyMap( nc->nestedListenerUserData,
                                                format, fd, size );
   }
}

static void keyboardHandleEnter( void *data, struct wl_keyboard *keyboard,
                                 uint32_t serial, struct wl_surface *surface, 
                                 struct wl_array *keys )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->nestedListener->keyboardHandleEnter( nc->nestedListenerUserData,
                                               keys );
   }
}

static void keyboardHandleLeave( void *data, struct wl_keyboard *keyboard,
                                 uint32_t serial, struct wl_surface *surface )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->nestedListener->keyboardHandleLeave( nc->nestedListenerUserData );
   }
}

static void keyboardHandleKey( void *data, struct wl_keyboard *keyboard,
                               uint32_t serial, uint32_t time, uint32_t key,
                               uint32_t state)
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->nestedListener->keyboardHandleKey( nc->nestedListenerUserData,
                                             time, key, state );
   }
}

static void keyboardHandleModifiers( void *data, struct wl_keyboard *keyboard,
                                     uint32_t serial, uint32_t mods_depressed,
                                     uint32_t mods_latched, uint32_t mods_locked,
                                     uint32_t group )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->nestedListener->keyboardHandleModifiers( nc->nestedListenerUserData,
                                                         mods_depressed, mods_latched,
                                                         mods_locked, group );
   }
}

static void keyboardHandleRepeatInfo( void *data, struct wl_keyboard *keyboard,
                                      int32_t rate, int32_t delay )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->nestedListener->keyboardHandleRepeatInfo( nc->nestedListenerUserData,
                                                    rate, delay );
   }
}

static const struct wl_keyboard_listener keyboardListener= {
   keyboardHandleKeymap,
   keyboardHandleEnter,
   keyboardHandleLeave,
   keyboardHandleKey,
   keyboardHandleModifiers,
   keyboardHandleRepeatInfo
};

static void pointerHandleEnter( void *data, struct wl_pointer *pointer,
                                uint32_t serial, struct wl_surface *surface,
                                wl_fixed_t sx, wl_fixed_t sy )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->pointerEnterSerial= serial;
      nc->nestedListener->pointerHandleEnter( nc->nestedListenerUserData,
                                              surface, sx, sy );
   }
}

static void pointerHandleLeave( void *data, struct wl_pointer *pointer,
                                uint32_t serial, struct wl_surface *surface )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->nestedListener->pointerHandleLeave( nc->nestedListenerUserData, surface );
   }
}

static void pointerHandleMotion( void *data, struct wl_pointer *pointer, 
                                 uint32_t time, wl_fixed_t sx, wl_fixed_t sy )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->nestedListener->pointerHandleMotion( nc->nestedListenerUserData,
                                               time, sx, sy );
   }
}

static void pointerHandleButton( void *data, struct wl_pointer *pointer,
                                 uint32_t serial, uint32_t time, 
                                 uint32_t button, uint32_t state )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->nestedListener->pointerHandleButton( nc->nestedListenerUserData,
                                               time, button, state );
   }
}

static void pointerHandleAxis( void *data, struct wl_pointer *pointer,
                               uint32_t time, uint32_t axis, wl_fixed_t value )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   if ( nc->nestedListener )
   {
      nc->nestedListener->pointerHandleAxis( nc->nestedListenerUserData,
                                             time, axis, value );
   }
}

static const struct wl_pointer_listener pointerListener= {
   pointerHandleEnter,
   pointerHandleLeave,
   pointerHandleMotion,
   pointerHandleButton,
   pointerHandleAxis
};

static void seatCapabilities( void *data, struct wl_seat *seat, uint32_t capabilities )
{
	WstNestedConnection *nc = (WstNestedConnection*)data;

   if ( capabilities & WL_SEAT_CAPABILITY_KEYBOARD )
   {
      nc->keyboard= wl_seat_get_keyboard( nc->seat );
      wl_keyboard_add_listener( nc->keyboard, &keyboardListener, nc );
   }
   if ( capabilities & WL_SEAT_CAPABILITY_POINTER )
   {
      nc->pointer= wl_seat_get_pointer( nc->seat );
      wl_pointer_add_listener( nc->pointer, &pointerListener, nc );
   }
   if ( capabilities & WL_SEAT_CAPABILITY_TOUCH )
   {
      nc->touch= wl_seat_get_touch( nc->seat );
   }   
   wl_display_roundtrip( nc->display );
}

static void seatName( void *data, struct wl_seat *seat, const char *name )
{
   WST_UNUSED(data);
   WST_UNUSED(seat);
   WST_UNUSED(name);
}

static const struct wl_seat_listener seatListener = {
   seatCapabilities,
   seatName 
};

static void shmFormat( void *data, struct wl_shm *shm, uint32_t format )
{
	WstNestedConnection *nc = (WstNestedConnection*)data;
	
   if ( nc->nestedListener )
   {
      switch ( format )
      {
         case WL_SHM_FORMAT_ARGB8888:
         case WL_SHM_FORMAT_XRGB8888:
            // Nothing to do for required formats
            break;
         default:
            nc->nestedListener->shmFormat( nc->nestedListenerUserData, 
                                           format );
            break;
      }
   }
}

static const struct wl_shm_listener shmListener = {
   shmFormat
};

#ifdef ENABLE_SBPROTOCOL
static void sbFormat(void *data, struct wl_sb *wl_sb, uint32_t format)
{
   WST_UNUSED(data);
   WST_UNUSED(wl_sb);
   WST_UNUSED(format);
}

struct wl_sb_listener sbListener = {
	sbFormat
};
#endif

static void simpleShellSurfaceId(void *data,
                                 struct wl_simple_shell *wl_simple_shell,
                                 struct wl_surface *surface,
                                 uint32_t surfaceId)
{
   WST_UNUSED(wl_simple_shell);
	WstNestedConnection *nc = (WstNestedConnection*)data;

   nc->surfaceMap.insert( std::pair<struct wl_surface*,int32_t>( surface, surfaceId ) );     
}
                           
static void simpleShellSurfaceCreated(void *data,
                                      struct wl_simple_shell *wl_simple_shell,
                                      uint32_t surfaceId,
                                      const char *name)
{
   WST_UNUSED(data);
   WST_UNUSED(wl_simple_shell);
   WST_UNUSED(surfaceId);
   WST_UNUSED(name);
}

static void simpleShellSurfaceDestroyed(void *data,
                                        struct wl_simple_shell *wl_simple_shell,
                                        uint32_t surfaceId,
                                        const char *name)
{
   WST_UNUSED(data);
   WST_UNUSED(wl_simple_shell);
   WST_UNUSED(surfaceId);
   WST_UNUSED(name);
}
                                  
static void simpleShellSurfaceStatus(void *data,
                                     struct wl_simple_shell *wl_simple_shell,
                                     uint32_t surfaceId,
                                     const char *name,
                                     uint32_t visible,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height,
                                     wl_fixed_t opacity,
                                     wl_fixed_t zorder)
{
   WST_UNUSED(data);
   WST_UNUSED(wl_simple_shell);
   WST_UNUSED(surfaceId);
   WST_UNUSED(name);
   WST_UNUSED(visible);
   WST_UNUSED(x);
   WST_UNUSED(y);
   WST_UNUSED(width);
   WST_UNUSED(height);
   WST_UNUSED(opacity);
   WST_UNUSED(zorder);
}                               

static const struct wl_simple_shell_listener simpleShellListener = 
{
   simpleShellSurfaceId,
   simpleShellSurfaceCreated,
   simpleShellSurfaceDestroyed,
   simpleShellSurfaceStatus
};

static void registryHandleGlobal(void *data, 
                                 struct wl_registry *registry, uint32_t id,
		                           const char *interface, uint32_t version)
{
	WstNestedConnection *nc = (WstNestedConnection*)data;
   int len;
  
   len= strlen(interface);
   if ( ((len==13) && !strncmp(interface, "wl_compositor", len) ) ) {
      nc->compositor= (struct wl_compositor*)wl_registry_bind(registry, id, &wl_compositor_interface, 1);
   }
   else if ( (len==7) && !strncmp(interface, "wl_seat", len) ) {
      nc->seat= (struct wl_seat*)wl_registry_bind(registry, id, &wl_seat_interface, 4);
		wl_seat_add_listener(nc->seat, &seatListener, nc);
		wl_display_roundtrip( nc->display );
   } 
   else if ( (len==6) && !strncmp(interface, "wl_shm", len) ) {
      nc->shm= (struct wl_shm*)wl_registry_bind(registry, id, &wl_shm_interface, 1);
      wl_shm_add_listener(nc->shm, &shmListener, nc);
   }
   else if ( (len==15) && !strncmp(interface, "wl_simple_shell", len) ) {
      nc->simpleShell= (struct wl_simple_shell*)wl_registry_bind(registry, id, &wl_simple_shell_interface, 1);      
      wl_simple_shell_add_listener(nc->simpleShell, &simpleShellListener, nc);
   }
   #ifdef ENABLE_SBPROTOCOL
   else if ( (len==5) && !strncmp(interface, "wl_sb", len) ) {
      nc->sb= (struct wl_sb*)wl_registry_bind(registry, id, &wl_sb_interface, 1);
      wl_sb_add_listener(nc->sb, &sbListener, nc);
   }
   #endif
}

static void registryHandleGlobalRemove(void *data, 
                                       struct wl_registry *registry,
			                              uint32_t name)
{
   WST_UNUSED(data);
   WST_UNUSED(registry);
   WST_UNUSED(name);
}

static const struct wl_registry_listener registryListener = 
{
	registryHandleGlobal,
	registryHandleGlobalRemove
};

static void* wstNestedThread( void *data )
{
   WstNestedConnection *nc= (WstNestedConnection*)data;
   
   nc->started= true;
   
   while ( !nc->stopRequested )
   {
      if ( wl_display_dispatch( nc->display ) == -1 )
      {
         break;
      }
      
      usleep( 50000 );
   }
 
   nc->started= false;
   if ( !nc->stopRequested )
   {
      nc->nestedListener->connectionEnded( nc->nestedListenerUserData );
   }
}

WstNestedConnection* WstNestedConnectionCreate( WstCompositor *wctx, 
                                                const char *displayName, 
                                                int width, 
                                                int height,
                                                WstNestedConnectionListener *listener,
                                                void *userData )
{
   WstNestedConnection *nc= 0;
   bool error= false;
   int rc;
   
   nc= (WstNestedConnection*)calloc( 1, sizeof(WstNestedConnection) );
   if ( nc )
   {
      nc->ctx= wctx;
      nc->nestedListenerUserData= userData;
      nc->nestedListener= listener;
      
      nc->surfaceMap= std::map<struct wl_surface*, int32_t>();

      nc->display= wl_display_connect( displayName );
      if ( !nc->display )
      {
         printf("WstNestedConnectionCreate: failed to connect to wayland display: %s\n", displayName );
         error= true;
         goto exit;
      }
      
      nc->registry= wl_display_get_registry(nc->display);
      if ( !nc->registry )
      {
         printf("WstNestedConnectionCreate: failed to obtain registry from wayland display: %s\n", displayName );
         error= true;
         goto exit;
      }

      wl_registry_add_listener(nc->registry, &registryListener, nc);   
      wl_display_roundtrip(nc->display);
      
      if ( !nc->compositor )
      {
         printf("WstNestedConnectionCreate: failed to obtain compositor from wayland display: %s\n", displayName );
         error= true;
         goto exit;
      }
    
      if ( (width != 0) && (height != 0) )
      {
         nc->surface= wl_compositor_create_surface(nc->compositor);
         if ( !nc->surface )
         {
            printf("WstNestedConnectionCreate: failed to create compositor surface from wayland display: %s\n", displayName );
            error= true;
            goto exit;
         }
         wl_display_roundtrip(nc->display);
      }
      
      nc->started= false;
      nc->stopRequested= false;
      rc= pthread_create( &nc->nestedThreadId, NULL, wstNestedThread, nc );
      if ( rc )
      {
         printf("WstNestedConnectionCreate: failed to start thread for nested compositor\n" );
         error= true;
         goto exit;
      }
   
      nc->nestedWidth= width;
      nc->nestedHeight= height;
   }

exit:

   if ( error )
   {
      if ( nc )
      {
         WstNestedConnectionDestroy( nc );
         nc= 0;
      }
   }
   
   return nc;   
}

void WstNestedConnectionDisconnect( WstNestedConnection *nc )
{
   if ( nc )
   {
      if ( nc->started )
      {
         nc->stopRequested= true;
         wl_display_flush( nc->display );
         wl_display_roundtrip( nc->display );
         pthread_join( nc->nestedThreadId, NULL );
      }
   }
}

void WstNestedConnectionDestroy( WstNestedConnection *nc )
{
   if ( nc )
   {
      bool threadStarted= nc->started;
      if ( threadStarted )
      {
         nc->stopRequested= true;
         wl_display_flush( nc->display );
         wl_display_roundtrip( nc->display );
         pthread_join( nc->nestedThreadId, NULL );
      }
      if ( nc->touch )
      {
         wl_touch_destroy( nc->touch );
         nc->touch= 0;
      }
      if ( nc->pointer )
      {
         wl_pointer_destroy( nc->pointer );
         nc->pointer= 0;
      }
      if ( nc->keyboard )
      {
         wl_keyboard_destroy( nc->keyboard );
         nc->keyboard= 0;
      }
      if ( nc->surface )
      {
         wl_surface_destroy( nc->surface );
         nc->surface= 0;
      }
      if ( nc->shm )
      {
         wl_shm_destroy( nc->shm );
         nc->shm= 0;
      }
      #ifdef ENABLE_SBPROTOCOL
      if ( nc->sb )
      {
         wl_sb_destroy( nc->sb );
         nc->sb= 0;
      }
      #endif
      if ( nc->compositor )
      {
         wl_compositor_destroy( nc->compositor );
         nc->compositor= 0;
      }
      if ( nc->registry )
      {
         wl_registry_destroy( nc->registry );
         nc->registry= 0;
      }
      if ( nc->display )
      {
         wl_display_flush( nc->display );
         wl_display_roundtrip( nc->display );
         wl_display_disconnect( nc->display );
         nc->display= 0;
      }
      nc->surfaceMap.clear();
      free( nc );
   }
}

wl_display* WstNestedConnectionGetDisplay( WstNestedConnection *nc )
{
   wl_display *display= 0;
   
   if ( nc )
   {
      display= nc->display;
   }
   
   return display;
}

wl_surface* WstNestedConnectionGetCompositionSurface( WstNestedConnection *nc )
{
   wl_surface *surface= 0;
   
   if ( nc )
   {
      surface= nc->surface;
   }
   
   return surface;
}

struct wl_surface* WstNestedConnectionCreateSurface( WstNestedConnection *nc )
{
   wl_surface *surface= 0;
   
   if ( nc && nc->compositor )
   {
      surface= wl_compositor_create_surface(nc->compositor);
      wl_display_flush( nc->display );      
   }
   
   return surface;
}

void WstNestedConnectionDestroySurface( WstNestedConnection *nc, struct wl_surface *surface )
{
   if ( surface )
   {
      for( std::map<struct wl_surface*,int32_t>::iterator it= nc->surfaceMap.begin(); it != nc->surfaceMap.end(); ++it )
      {
         if ( it->first == surface )
         {
            nc->surfaceMap.erase(it);
            break;
         }
      }
      wl_surface_destroy( surface );
      wl_display_flush( nc->display );      
   }
}

void WstNestedConnectionSurfaceSetVisible( WstNestedConnection *nc, 
                                           struct wl_surface *surface,
                                           bool visible )
{
   if ( surface )
   {
      std::map<struct wl_surface*,int32_t>::iterator it= nc->surfaceMap.find( surface );
      if ( it != nc->surfaceMap.end() )
      {
         int32_t surfaceId= it->second;
         
         wl_simple_shell_set_visible( nc->simpleShell, surfaceId, visible );
      }
   }
}

void WstNestedConnectionSurfaceSetGeometry( WstNestedConnection *nc, 
                                            struct wl_surface *surface,
                                            int x,
                                            int y,
                                            int width, 
                                            int height )
{
   if ( surface )
   {
      std::map<struct wl_surface*,int32_t>::iterator it= nc->surfaceMap.find( surface );
      if ( it != nc->surfaceMap.end() )
      {
         int32_t surfaceId= it->second;
         
         wl_simple_shell_set_geometry( nc->simpleShell, surfaceId, x, y, width, height );
      }
   }
}

void WstNestedConnectionSurfaceSetZOrder( WstNestedConnection *nc, 
                                          struct wl_surface *surface,
                                          float zorder )
{
   if ( surface )
   {
      std::map<struct wl_surface*,int32_t>::iterator it= nc->surfaceMap.find( surface );
      if ( it != nc->surfaceMap.end() )
      {
         int32_t surfaceId= it->second;
         wl_fixed_t z= wl_fixed_from_double(zorder);
         
         wl_simple_shell_set_zorder( nc->simpleShell, surfaceId, z );
      }
   }
}

void WstNestedConnectionSurfaceSetOpacity( WstNestedConnection *nc, 
                                           struct wl_surface *surface,
                                           float opacity )
{
   if ( surface )
   {
      std::map<struct wl_surface*,int32_t>::iterator it= nc->surfaceMap.find( surface );
      if ( it != nc->surfaceMap.end() )
      {
         int32_t surfaceId= it->second;
         wl_fixed_t op= wl_fixed_from_double(opacity);
         
         wl_simple_shell_set_opacity( nc->simpleShell, surfaceId, op );
      }
   }
}

void WstNestedConnectionAttachAndCommit( WstNestedConnection *nc,
                                          struct wl_surface *surface,
                                          struct wl_buffer *buffer,
                                          int x,
                                          int y,
                                          int width,
                                          int height )
{
   if ( nc )
   {
      wl_surface_attach( surface, buffer, 0, 0 );
      wl_surface_damage( surface, x, y, width, height);
      wl_surface_commit( surface );
      wl_display_flush( nc->display );      
   }
}                                          

void WstNestedConnectionAttachAndCommitDevice( WstNestedConnection *nc,
                                               struct wl_surface *surface,
                                               void *deviceBuffer,
                                               uint32_t format,
                                               int32_t stride,
                                               int x,
                                               int y,
                                               int width,
                                               int height )
{
   if ( nc )
   {
      #ifdef ENABLE_SBPROTOCOL
      struct wl_buffer *buffer;
      
      buffer= wl_sb_create_buffer( nc->sb, 
                                   (uint32_t)deviceBuffer, 
                                   width, 
                                   height, 
                                   stride,
                                   format );
      if ( buffer )
      {
         wl_surface_attach( surface, buffer, 0, 0 );
         wl_surface_damage( surface, x, y, width, height);
         wl_surface_commit( surface );
         wl_display_flush( nc->display );
         wl_buffer_destroy( buffer );
      }
      #endif
   }
}                                               

void WstNestedConnectionPointerSetCursor( WstNestedConnection *nc, 
                                          struct wl_surface *surface, 
                                          int hotspotX, 
                                          int hotspotY )
{
   if ( nc )
   {
      wl_pointer_set_cursor( nc->pointer,
                             nc->pointerEnterSerial,
                             surface,
                             hotspotX,
                             hotspotY );
      wl_display_flush( nc->display );      
   }
}                                          

struct wl_shm_pool* WstNestedConnnectionShmCreatePool( WstNestedConnection *nc, int fd, int size )
{
   WST_UNUSED(nc);
   struct wl_shm_pool *pool= 0;
   
   if ( nc && nc->shm )
   {
      pool= wl_shm_create_pool( nc->shm, fd, size );
      wl_display_flush( nc->display );      
   }
   
   return pool;
}

void WstNestedConnectionShmDestroyPool( WstNestedConnection *nc, struct wl_shm_pool *pool )
{
   WST_UNUSED(nc);
   if ( pool )
   {
      wl_shm_pool_destroy( pool );
      wl_display_flush( nc->display );      
   }
}

void WstNestedConnectionShmPoolResize( WstNestedConnection *nc, struct wl_shm_pool *pool, int size )
{
   WST_UNUSED(nc);
   if ( pool )
   {
      wl_shm_pool_resize( pool, size );
      wl_display_flush( nc->display );      
   }
}

struct wl_buffer* WstNestedConnectionShmPoolCreateBuffer( WstNestedConnection *nc,
                                                          struct wl_shm_pool *pool,
                                                          int32_t offset,
                                                          int32_t width, 
                                                          int32_t height,
                                                          int32_t stride, 
                                                          uint32_t format)
{
   WST_UNUSED(nc);
   struct wl_buffer *buffer= 0;
   
   if ( pool )
   {
      buffer= wl_shm_pool_create_buffer( pool, offset, width, height, stride, format );
      wl_display_flush( nc->display );      
   }
   
   return buffer;
}                                                          

void WstNestedConnectionShmBufferPoolDestroy( WstNestedConnection *nc,
                                              struct wl_shm_pool *pool,
                                              struct wl_buffer *buffer )
{
   WST_UNUSED(nc);
   WST_UNUSED(pool);
   
   if ( buffer )
   {
      wl_buffer_destroy( buffer );
      wl_display_flush( nc->display );      
   }
}

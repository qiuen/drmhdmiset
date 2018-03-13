#include "drmresources.h"
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "hdmiset.h"
#include "string.h"
#include <fcntl.h>
using namespace android;

static HdmiInfo_t hdmi_info[50];
static int mode_count = 0;
static DrmResources *_drm = NULL;

int init_hdmi_set(int fd) {
    int r = 0;
    int display = 0;
    _drm = new DrmResources();
    r = _drm->Init(fd);
    DrmConnector *c = _drm->GetConnectorFromType(display);
    int i = 0;
    memset(&hdmi_info, 0, sizeof(HdmiInfo_t));
    for (const DrmMode &conn_mode : c->modes()) {
       	    hdmi_info[i].refresh = conn_mode.v_refresh();
            hdmi_info[i].interlaced = conn_mode.interlaced();
            hdmi_info[i].xres = conn_mode.h_display();
            hdmi_info[i].yres = conn_mode.v_display();
            hdmi_info[i].reserved[0] = conn_mode.clock();
            printf("#################init_hdmi_set interlaced=%x,refresh=%d,xres=%d,yres=%d\n",hdmi_info[i].interlaced,hdmi_info[i].refresh,hdmi_info[i].xres,hdmi_info[i].yres);
            i++;
    }
    mode_count = i;
    return r;
}

int deInit_hdmi_set() {
    
    delete _drm;
    _drm = NULL;
    return 0;
}

int set_plane(int fb_id, int crtc_id) {
   int ret = _drm->commitFrame(fb_id, crtc_id);
   return ret;
}

int hdmi_set_resolution(int xres, int yres, int refresh, bool interlaced) {
   // printf("+++++hdmi_set_resolution begin\n");
    int display = 0;
      int count = 0;
        DrmConnector *c = _drm->GetConnectorFromType(display);
	for (const DrmMode &conn_mode : c->modes()) {
            count++;
          if (conn_mode.equal(xres, yres, refresh, interlaced)) {
            c->set_best_mode(conn_mode);
            c->set_current_mode(conn_mode);	
            _drm->UpdateDisplayRoute();
            usleep(1000000);
        //  _drm->UpdateDisplaySize(0,0,1920,1080,0,0,xres,yres);
          break;
        }
    }
    printf("@@@@@@@@@@@conn mode count=%d\n",count);
   // printf("+++++hdmi_set_resolution end\n");
    return 0;
}

int UpdateDisplaySize(int src_xpos, int src_ypos, int src_w, int src_h, int dst_xpos, int dst_ypos, int dst_w, int dst_h) {
  _drm->UpdateDisplaySize(src_xpos,src_ypos,src_w,src_h,dst_xpos,dst_ypos,dst_w,dst_h);
}


int hdmi_get_resolution(Hdmi_Info_list_t *list) {
    printf("++++--------------hdmi_get_resolution begin \n");
    int display = 0;
    if (_drm == NULL) {
        return 0;
    }
    _drm->UpdatePrimary();
    DrmConnector *c = _drm->GetConnectorFromType(display);
    int i = 0;
    memset(&hdmi_info, 0, sizeof(HdmiInfo_t));
	for (const DrmMode &conn_mode : c->modes()) {
       hdmi_info[i].refresh = conn_mode.v_refresh();
       hdmi_info[i].interlaced = conn_mode.interlaced();
       hdmi_info[i].xres = conn_mode.h_display();
       hdmi_info[i].yres = conn_mode.v_display();
       hdmi_info[i].reserved[0] = conn_mode.clock();
       printf("++++hdmi_get_resolution refresh=%d,interlaced=%d,xres=%d,yres=%d\n",hdmi_info[i].refresh,hdmi_info[i].interlaced,hdmi_info[i].xres,hdmi_info[i].yres);
       i++;
    }
    list->hdmimode = &hdmi_info[0];
    list->numHdmiMode = i;
    mode_count = i;
    printf("++++hdmi_get_resolution end \n");
    return 0;
}

int hdmi_check_mode(int xres, int yres, int refresh, int flag, int clock) {
  
  for (int i=0; i<mode_count; i++) {
     if (xres==hdmi_info[i].xres && yres==hdmi_info[i].yres && refresh==hdmi_info[i].refresh && flag==hdmi_info[i].interlaced && clock==hdmi_info[i].reserved[0]) {
       return 1;
     }

  }

  return -1;
}


int hdmi_get_current_mode(int *xres, int *yres) {

    int fd = open("/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/mode", O_RDONLY, 0);
    char  value[50];
    if (fd > 0) {
       int ret = read(fd, value, 50);
       if (ret > 0) {
         sscanf(value, "%dx%d", xres, yres);
         printf("+++++hdmi_get_current_mode:xres=%d, yres=%d\n", *xres, *yres);
       }     
   }
   if (fd > 0) {
     close(fd);
   }

}

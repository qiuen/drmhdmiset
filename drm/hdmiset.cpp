#include "drmresources.h"
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "hdmiset.h"
#include "string.h"
#include <fcntl.h>
using namespace android;

#define BUFFER_LENGTH    256
#define AUTO_BIT_RESET 0x00
#define RESOLUTION_AUTO 1<<0
#define COLOR_AUTO (1<<1)
#define HDCP1X_EN (1<<2)
#define RESOLUTION_WHITE_EN (1<<3)
#define BASE_OFFSET 8*1024
#define DEFAULT_BRIGHTNESS  50
#define DEFAULT_CONTRAST  50
#define DEFAULT_SATURATION  50
#define DEFAULT_HUE  50
#define DEFAULT_OVERSCAN_VALUE 100
#define BASE_PARAMER_PATH "/dev/mmcblk2p7"
struct lut_data{
    uint16_t size;
    uint16_t lred[1024];
    uint16_t lgreen[1024];
    uint16_t lblue[1024];
};

struct lut_info{
    struct lut_data main;
    struct lut_data aux;
};

struct drm_display_mode {
    /* Proposed mode values */
    int clock;		/* in kHz */
    int hdisplay;
    int hsync_start;
    int hsync_end;
    int htotal;
    int vdisplay;
    int vsync_start;
    int vsync_end;
    int vtotal;
    int vrefresh;
    int vscan;
    unsigned int flags;
    int picture_aspect_ratio;
};

enum output_format {
    output_rgb=0,
    output_ycbcr444=1,
    output_ycbcr422=2,
    output_ycbcr420=3,
    output_ycbcr_high_subsampling=4,  // (YCbCr444 > YCbCr422 > YCbCr420 > RGB)
    output_ycbcr_low_subsampling=5	, // (RGB > YCbCr420 > YCbCr422 > YCbCr444)
    invalid_output=6,
};

enum  output_depth{
    Automatic=0,
    depth_24bit=8,
    depth_30bit=10,
};

struct overscan {
    unsigned int maxvalue;
    unsigned short leftscale;
    unsigned short rightscale;
    unsigned short topscale;
    unsigned short bottomscale;
};

struct hwc_inital_info{
    char device[128];
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    float fps;
};

struct bcsh_info {
    unsigned short brightness;
    unsigned short contrast;
    unsigned short saturation;
    unsigned short hue;
};

struct screen_info {
    int type;
    struct drm_display_mode resolution;// 52 bytes
    enum output_format  format; // 4 bytes
    enum output_depth depthc; // 4 bytes
    unsigned int feature;//4 //4 bytes
};

struct disp_info {
    struct screen_info screen_list[5];
    struct overscan scan;//12 bytes
    struct hwc_inital_info hwc_info; //140 bytes
    struct bcsh_info bcsh;
    unsigned int reserve[128];
    struct lut_data mlutdata;/*6k+4*/
};

struct file_base_paramer
{
    struct disp_info main;
    struct disp_info aux;
};

static char const *const device_template[] =
{
    "/dev/block/platform/1021c000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/30020000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/fe330000.sdhci/by-name/baseparameter",
    "/dev/block/platform/ff520000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/ff0f0000.dwmmc/by-name/baseparameter",
    "/dev/block/rknand_baseparameter",
    NULL
};

static HdmiInfo_t hdmi_info[200];
static int mode_count = 0;
static HdmiInfo_t best_mode;
static DrmResources *_drm = NULL;
static  void saveModeConfig(const DrmMode *mode, int conn_type);
static bool getBaseParameterInfo(struct file_base_paramer* base_paramer);
static int findSuitableInfoSlot(struct disp_info* info, int type) ;
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

void set_best_hdmi_mode(int xres, int yres, int refresh, bool interlaced) {

	best_mode.xres = xres;
	best_mode.yres = yres;
	best_mode.refresh = refresh;
	best_mode.interlaced = interlaced;
}

int hdmi_get_last_resolution(int* xres, int* yres, int* refresh, bool* interlaced, int type) {
	
	int slot = 0;
	int find_mode = 0;
	int display = 0;
	file_base_paramer base_paramer;
	getBaseParameterInfo(&base_paramer);
    
	slot = findSuitableInfoSlot(&base_paramer.main, type);
	printf("================================hdmi_get_last_resolution:slot=%d\n",slot);	
	*xres = base_paramer.main.screen_list[slot].resolution.hdisplay;
	*yres = base_paramer.main.screen_list[slot].resolution.vdisplay;		
	*refresh = base_paramer.main.screen_list[slot].resolution.vrefresh;
	*interlaced = (base_paramer.main.screen_list[slot].resolution.flags&DRM_MODE_FLAG_INTERLACE)>0?1:0;
    DrmConnector *c = _drm->GetConnectorFromType(display);
    for (const DrmMode &conn_mode : c->modes()) {
		if (*refresh == conn_mode.v_refresh() \
			&& *xres==conn_mode.h_display() \
			&& *yres==conn_mode.v_display() \
			&& *interlaced==conn_mode.interlaced()) {
			find_mode = 1;
            break;
		}
	}
	if (find_mode == 0) {
		*xres = best_mode.xres;
		*yres = best_mode.yres;
		*refresh = best_mode.refresh;
		*interlaced = best_mode.interlaced;
	}
	
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
			saveModeConfig(&conn_mode, c->type());
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



const char* GetBaseparameterFile(void)
{
    int i = 0;
    char *path = "/dev/mmcblk2p7";
	return path;
    while (device_template[i]) {
        if (!access(device_template[i], R_OK | W_OK))
            return device_template[i];
        printf("temp[%d]=%s access=%d(%s)", i,device_template[i], errno, strerror(errno));
        i++;
    }
    return NULL;
}

static int setGamma(int fd, uint32_t crtc_id, uint32_t size,
        uint16_t *red, uint16_t *green, uint16_t *blue)
{
    int ret = drmModeCrtcSetGamma(fd, crtc_id, size, red, green, blue);
    if (ret < 0)
        printf("fail to SetGamma %d(%s)", ret, strerror(errno));
    return ret;
}

static void freeLutInfo(){

}

static int findSuitableInfoSlot(struct disp_info* info, int type) 
{
    int found=-1;
    for (int i=0;i<5;i++) {
		printf("+++++++++++++++++i=%d,type=%d,%d\n",i, type,info->screen_list[i].type);
        if (info->screen_list[i].type !=0 && info->screen_list[i].type == type) {
            found = i;
            break;
        } 
    }
    if (found == -1) {
       for (int i=0;i<5;i++) {
	//	printf("+++++++++++++++++i=%d,type=%d,%d\n",i, type,info->screen_list[i].type);
        if (info->screen_list[i].type ==0) {
            found = i;
            break;
        } 
      }

    }
    printf("findSuitableInfoSlot: %d type=%d", found, type);
    return found;
}

static bool getBaseParameterInfo(struct file_base_paramer* base_paramer)
{
    int file;
    const char *baseparameterfile = GetBaseparameterFile();
    if (baseparameterfile) {
        file = open(baseparameterfile, O_RDWR);
        if (file > 0) {
            unsigned int length = lseek(file, 0L, SEEK_END);
            lseek(file, 0L, SEEK_SET);
            printf("getBaseParameterInfo size=%d\n", (int)sizeof(*base_paramer));
            if (length >  sizeof(*base_paramer)) {
                read(file, (void*)&(base_paramer->main), sizeof(base_paramer->main));
				printf("==================getBaseParameterInfo hdisplay=%d\n", base_paramer->main.screen_list[1].resolution.hdisplay);
                lseek(file, BASE_OFFSET, SEEK_SET);
                read(file, (void*)&(base_paramer->aux), sizeof(base_paramer->aux));
                return true;
            }
        }
    }
    return false;
}


static void saveModeConfig(const DrmMode *mode, int conn_type) {
	
	int file;
    const char *baseparameterfile = GetBaseparameterFile();
	char buf[BUFFER_LENGTH];
    bool isMainHdmiConnected=false;
    bool isAuxHdmiConnected = false;
	int w=0,h=0,hsync_start=0,hsync_end=0,htotal=0;
	int vsync_start=0,vsync_end=0,vtotal=0,flags=0;
	int left=0,top=0,right=0,bottom=0;
	float vfresh=0;
	int slot = 0;
    file_base_paramer base_paramer;
    if (!baseparameterfile) {
        sync();
        return;
    }
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        printf("base paramter file can not be opened\n");
        sync();
        return;
    }
    // caculate file's size and read it
    unsigned int length = lseek(file, 0L, SEEK_END);
    lseek(file, 0L, SEEK_SET);
    if(length < sizeof(base_paramer)) {
        printf("BASEPARAME data's length is error\n");
        sync();
        close(file);
        return;
    }
	read(file, (void*)&(base_paramer.main), sizeof(base_paramer.main));
    lseek(file, BASE_OFFSET, SEEK_SET);
    read(file, (void*)&(base_paramer.aux), sizeof(base_paramer.aux));
	
	//if (conn_type == DRM_MODE_CONNECTOR_TV) {
	slot = findSuitableInfoSlot(&base_paramer.main, conn_type);	
	printf("++++++++++++++++++++++++++++++saveModeConfig slot=%d,conn_type=%d\n",slot,conn_type);
	base_paramer.main.screen_list[slot].type = conn_type;
	base_paramer.main.screen_list[slot].resolution.hdisplay = mode->h_display();
	base_paramer.main.screen_list[slot].resolution.vdisplay = mode->v_display();
	base_paramer.main.screen_list[slot].resolution.hsync_start = mode->h_sync_start();
	base_paramer.main.screen_list[slot].resolution.hsync_end = mode->h_sync_end();
	base_paramer.main.screen_list[slot].resolution.clock = mode->clock();
	base_paramer.main.screen_list[slot].resolution.htotal = mode->h_total();
	base_paramer.main.screen_list[slot].resolution.vsync_start = mode->v_sync_start();
	base_paramer.main.screen_list[slot].resolution.vsync_end = mode->v_sync_end();
	base_paramer.main.screen_list[slot].resolution.vtotal = mode->v_total();
	base_paramer.main.screen_list[slot].resolution.flags = mode->flags();
	base_paramer.main.screen_list[slot].resolution.vrefresh = mode->v_refresh();
	/*}

    if (conn_type == DRM_MODE_CONNECTOR_HDMIA) {
		slot = findSuitableInfoSlot(&base_paramer.aux, conn_type);	
		base_paramer.main.screen_list[slot].type = conn_type;
		base_paramer.main.screen_list[slot].resolution.hdisplay = mode->h_display();
		base_paramer.main.screen_list[slot].resolution.vdisplay = mode->v_display();
		base_paramer.main.screen_list[slot].resolution.hsync_start = mode->h_sync_start();
		base_paramer.main.screen_list[slot].resolution.hsync_end = mode->h_sync_end();
		base_paramer.main.screen_list[slot].resolution.clock = mode->clock();
		base_paramer.main.screen_list[slot].resolution.htotal = mode->h_total();
		base_paramer.main.screen_list[slot].resolution.vsync_start = mode->v_sync_start();
		base_paramer.main.screen_list[slot].resolution.vsync_end = mode->v_sync_end();
		base_paramer.main.screen_list[slot].resolution.vtotal = mode->v_total();
		base_paramer.main.screen_list[slot].resolution.flags = mode->flags();
		base_paramer.main.screen_list[slot].resolution.vrefresh = mode->v_refresh();
	}*/

	
	lseek(file, 0L, SEEK_SET);
	write(file, (char*)(&base_paramer.main), sizeof(base_paramer.main));
	lseek(file, BASE_OFFSET, SEEK_SET);
	write(file, (char*)(&base_paramer.aux), sizeof(base_paramer.aux));
	close(file);
	sync();
	
    printf("*********************************write hdmi mode succ,slot=%d.\n",slot);
	
}
#if 0
static void nativeSaveConfig() {
    char buf[BUFFER_LENGTH];
    bool isMainHdmiConnected=false;
    bool isAuxHdmiConnected = false;
    int foundMainIdx=-1,foundAuxIdx=-1;
    struct file_base_paramer base_paramer;
    int   display = 0;
    if (primary != NULL) {
		 DrmConnector *c = _drm->GetConnectorFromType(display);
        std::vector<DrmMode> mModes =  c->modes();
        char resolution[200];
        unsigned int w=0,h=0,hsync_start=0,hsync_end=0,htotal=0;
        unsigned int vsync_start=0,vsync_end=0,vtotal=0,flags=0;
        float vfresh=0.0000;

       /* property_get("persist.sys.resolution.main", resolution, "0x0@0.00-0-0-0-0-0-0-0");
        if (strncmp(resolution, "Auto", 4) != 0 && strncmp(resolution, "0x0p0-0", 7) !=0)
            sscanf(resolution,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x", &w, &h, &vfresh, &hsync_start,&hsync_end,
                    &htotal,&vsync_start,&vsync_end, &vtotal, &flags);*/
        for (size_t c = 0; c < mModes.size(); ++c){
            const DrmMode& info = mModes[c];
            char curDrmModeRefresh[16];
            char curRefresh[16];
            float mModeRefresh;
            if (info.flags() & DRM_MODE_FLAG_INTERLACE)
                mModeRefresh = info.clock()*2 / (float)(info.v_total()* info.h_total()) * 1000.0f;
            else
                mModeRefresh = info.clock()/ (float)(info.v_total()* info.h_total()) * 1000.0f;
            sprintf(curDrmModeRefresh, "%.2f", mModeRefresh);
            sprintf(curRefresh, "%.2f", vfresh);
            if (info.h_display() == w &&
                    info.v_display() == h &&
                    info.h_sync_start() == hsync_start &&
                    info.h_sync_end() == hsync_end &&
                    info.h_total() == htotal &&
                    info.v_sync_start() == vsync_start &&
                    info.v_sync_end() == vsync_end &&
                    info.v_total()==vtotal &&
                    atof(curDrmModeRefresh)==atof(curRefresh)) {
                ALOGD("***********************found main idx %d ****************", (int)c);
                foundMainIdx = c;
                sprintf(buf, "display=%d,iface=%d,enable=%d,mode=%s\n",
                        primary->display(), primary->get_type(), primary->state(), resolution);
                break;
            }
        }
    }

    if (extend != NULL) {
        std::vector<DrmMode> mModes = extend->modes();
        char resolution[PROPERTY_VALUE_MAX];
        unsigned int w=0,h=0,hsync_start=0,hsync_end=0,htotal=0;
        unsigned int vsync_start=0,vsync_end=0,vtotal=0,flags;
        float vfresh=0;

        property_get("persist.sys.resolution.aux", resolution, "0x0@0.00-0-0-0-0-0-0-0");
        if (strncmp(resolution, "Auto", 4) != 0 && strncmp(resolution, "0x0p0-0", 7) !=0)
            sscanf(resolution,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x", &w, &h, &vfresh, &hsync_start,&hsync_end,&htotal,&vsync_start,&vsync_end,
                    &vtotal, &flags);
        for (size_t c = 0; c < mModes.size(); ++c){
            const DrmMode& info = mModes[c];
            char curDrmModeRefresh[16];
            char curRefresh[16];
            float mModeRefresh;
            if (info.flags() & DRM_MODE_FLAG_INTERLACE)
                mModeRefresh = info.clock()*2 / (float)(info.v_total()* info.h_total()) * 1000.0f;
            else
                mModeRefresh = info.clock()/ (float)(info.v_total()* info.h_total()) * 1000.0f;
            sprintf(curDrmModeRefresh, "%.2f", mModeRefresh);
            sprintf(curRefresh, "%.2f", vfresh);
            if (info.h_display() == w &&
                    info.v_display() == h &&
                    info.h_sync_start() == hsync_start &&
                    info.h_sync_end() == hsync_end &&
                    info.h_total() == htotal &&
                    info.v_sync_start() == vsync_start &&
                    info.v_sync_end() == vsync_end &&
                    info.v_total()==vtotal &&
                    atof(curDrmModeRefresh)==atoi(curRefresh)) {
                ALOGD("***********************found aux idx %d ****************", (int)c);
                foundAuxIdx = c;
                break;
            }
        }
    }

    int file;
    const char *baseparameterfile = GetBaseparameterFile();
    if (!baseparameterfile) {
        sync();
        return;
    }
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        ALOGW("base paramter file can not be opened");
        sync();
        return;
    }
    // caculate file's size and read it
    unsigned int length = lseek(file, 0L, SEEK_END);
    lseek(file, 0L, SEEK_SET);
    if(length < sizeof(base_paramer)) {
        ALOGE("BASEPARAME data's length is error\n");
        sync();
        close(file);
        return;
    }

    read(file, (void*)&(base_paramer.main), sizeof(base_paramer.main));
    lseek(file, BASE_OFFSET, SEEK_SET);
    read(file, (void*)&(base_paramer.aux), sizeof(base_paramer.aux));

    for (auto &conn : drm_->connectors()) {
        if (conn->state() == DRM_MODE_CONNECTED 
                && (conn->get_type() == DRM_MODE_CONNECTOR_HDMIA)
                && (conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT))
            isMainHdmiConnected = true;
        else if(conn->state() == DRM_MODE_CONNECTED 
                && (conn->get_type() == DRM_MODE_CONNECTOR_HDMIA)
                && (conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT))
            isAuxHdmiConnected = true;
    }
    ALOGD("nativeSaveConfig: size=%d isMainHdmiConnected=%d", (int)sizeof(base_paramer.main), isMainHdmiConnected);
    for (auto &conn : drm_->connectors()) {
        if (conn->state() == DRM_MODE_CONNECTED 
                && (conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT)) {
            char property[PROPERTY_VALUE_MAX];
            int w=0,h=0,hsync_start=0,hsync_end=0,htotal=0;
            int vsync_start=0,vsync_end=0,vtotal=0,flags=0;
            int left=0,top=0,right=0,bottom=0;
            float vfresh=0;
            int slot = findSuitableInfoSlot(&base_paramer.main, conn->get_type());
            if (isMainHdmiConnected && conn->get_type() == DRM_MODE_CONNECTOR_TV)
                continue;

            base_paramer.main.screen_list[slot].type = conn->get_type();
            base_paramer.main.screen_list[slot].feature &= AUTO_BIT_RESET;
            property_get("persist.sys.resolution.main", property, "0x0@0.00-0-0-0-0-0-0-0");
            if (strncmp(property, "Auto", 4) != 0 && strncmp(property, "0x0p0-0", 7) !=0) {
                ALOGD("saveConfig resolution = %s", property);
                std::vector<DrmMode> mModes = primary->modes();
                sscanf(property,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x", &w, &h, &vfresh, &hsync_start,&hsync_end,&htotal,&vsync_start,&vsync_end,
                        &vtotal, &flags);

                ALOGD("last base_paramer.main.resolution.hdisplay = %d,  vdisplay=%d(%s@%f)",
                        base_paramer.main.screen_list[slot].resolution.hdisplay,
                        base_paramer.main.screen_list[slot].resolution.vdisplay,
                        base_paramer.main.hwc_info.device,  base_paramer.main.hwc_info.fps);
                base_paramer.main.screen_list[slot].resolution.hdisplay = w;
                base_paramer.main.screen_list[slot].resolution.vdisplay = h;
                base_paramer.main.screen_list[slot].resolution.hsync_start = hsync_start;
                base_paramer.main.screen_list[slot].resolution.hsync_end = hsync_end;
                if (foundMainIdx != -1)
                    base_paramer.main.screen_list[slot].resolution.clock = mModes[foundMainIdx].clock();
                else if (flags & DRM_MODE_FLAG_INTERLACE)
                    base_paramer.main.screen_list[slot].resolution.clock = (htotal*vtotal*vfresh/2)/1000.0f;
                else
                    base_paramer.main.screen_list[slot].resolution.clock = (htotal*vtotal*vfresh)/1000.0f;
                base_paramer.main.screen_list[slot].resolution.htotal = htotal;
                base_paramer.main.screen_list[slot].resolution.vsync_start = vsync_start;
                base_paramer.main.screen_list[slot].resolution.vsync_end = vsync_end;
                base_paramer.main.screen_list[slot].resolution.vtotal = vtotal;
                base_paramer.main.screen_list[slot].resolution.flags = flags;
                ALOGD("saveBaseParameter foundMainIdx=%d clock=%d", foundMainIdx, base_paramer.main.screen_list[slot].resolution.clock);
            } else {
                base_paramer.main.screen_list[slot].feature|= RESOLUTION_AUTO;
                memset(&base_paramer.main.screen_list[slot].resolution, 0, sizeof(base_paramer.main.screen_list[slot].resolution));
            }

            memset(property,0,sizeof(property));
            property_get("persist.sys.overscan.main", property, "overscan 100,100,100,100");
            sscanf(property, "overscan %d,%d,%d,%d",
                    &left,
                    &top,
                    &right,
                    &bottom);
            base_paramer.main.scan.leftscale = (unsigned short)left;
            base_paramer.main.scan.topscale = (unsigned short)top;
            base_paramer.main.scan.rightscale = (unsigned short)right;
            base_paramer.main.scan.bottomscale = (unsigned short)bottom;

            memset(property,0,sizeof(property));
            property_get("persist.sys.color.main", property, "Auto");
            if (strncmp(property, "Auto", 4) != 0){
                if (strstr(property, "RGB") != 0)
                    base_paramer.main.screen_list[slot].format = output_rgb;
                else if (strstr(property, "YCBCR444") != 0)
                    base_paramer.main.screen_list[slot].format = output_ycbcr444;
                else if (strstr(property, "YCBCR422") != 0)
                    base_paramer.main.screen_list[slot].format = output_ycbcr422;
                else if (strstr(property, "YCBCR420") != 0)
                    base_paramer.main.screen_list[slot].format = output_ycbcr420;
                else {
                    base_paramer.main.screen_list[slot].feature |= COLOR_AUTO;
                    base_paramer.main.screen_list[slot].format = output_ycbcr_high_subsampling;
                }

                if (strstr(property, "8bit") != NULL)
                    base_paramer.main.screen_list[slot].depthc = depth_24bit;
                else if (strstr(property, "10bit") != NULL)
                    base_paramer.main.screen_list[slot].depthc = depth_30bit;
                else
                    base_paramer.main.screen_list[slot].depthc = Automatic;
                ALOGD("saveConfig: color=%d-%d", base_paramer.main.screen_list[slot].format, base_paramer.main.screen_list[slot].depthc);
            } else {
                base_paramer.main.screen_list[slot].depthc = Automatic;
                base_paramer.main.screen_list[slot].format = output_ycbcr_high_subsampling;
                base_paramer.main.screen_list[slot].feature |= COLOR_AUTO;
            }

            memset(property,0,sizeof(property));
            property_get("persist.sys.hdcp1x.main", property, "0");
            if (atoi(property) > 0)
                base_paramer.main.screen_list[slot].feature |= HDCP1X_EN;

            memset(property,0,sizeof(property));
            property_get("persist.sys.resolution_white.main", property, "0");
            if (atoi(property) > 0)
                base_paramer.main.screen_list[slot].feature |= RESOLUTION_WHITE_EN;
            saveBcshConfig(&base_paramer, HWC_DISPLAY_PRIMARY_BIT);
#ifdef TEST_BASE_PARMARTER
            /*save aux fb & device*/
            saveHwcInitalInfo(&base_paramer, HWC_DISPLAY_PRIMARY_BIT);
#endif
        } else if(conn->state() == DRM_MODE_CONNECTED 
                && (conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT) 
                && (conn->encoder() != NULL)) {
            char property[PROPERTY_VALUE_MAX];
            int w=0,h=0,hsync_start=0,hsync_end=0,htotal=0;
            int vsync_start=0,vsync_end=0,vtotal=0,flags=0;
            float vfresh=0;
            int left=0,top=0,right=0,bottom=0;
            int slot = findSuitableInfoSlot(&base_paramer.aux, conn->get_type());

            if (isAuxHdmiConnected && conn->get_type() == DRM_MODE_CONNECTOR_TV)
                continue;

            base_paramer.aux.screen_list[slot].type = conn->get_type();
            base_paramer.aux.screen_list[slot].feature &= AUTO_BIT_RESET;
            property_get("persist.sys.resolution.aux", property, "0x0p0-0");
            if (strncmp(property, "Auto", 4) != 0 && strncmp(property, "0x0p0-0", 7) !=0) {
                std::vector<DrmMode> mModes = extend->modes();
                sscanf(property,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x", &w, &h, &vfresh, &hsync_start,&hsync_end,&htotal,&vsync_start,&vsync_end,
                        &vtotal, &flags);
                base_paramer.aux.screen_list[slot].resolution.hdisplay = w;
                base_paramer.aux.screen_list[slot].resolution.vdisplay = h;
                if (foundMainIdx != -1)
                    base_paramer.aux.screen_list[slot].resolution.clock = mModes[foundMainIdx].clock();
                else if (flags & DRM_MODE_FLAG_INTERLACE)
                    base_paramer.aux.screen_list[slot].resolution.clock = (htotal*vtotal*vfresh/2) / 1000.0f;
                else
                    base_paramer.aux.screen_list[slot].resolution.clock = (htotal*vtotal*vfresh) / 1000.0f;
                base_paramer.aux.screen_list[slot].resolution.hsync_start = hsync_start;
                base_paramer.aux.screen_list[slot].resolution.hsync_end = hsync_end;
                base_paramer.aux.screen_list[slot].resolution.htotal = htotal;
                base_paramer.aux.screen_list[slot].resolution.vsync_start = vsync_start;
                base_paramer.aux.screen_list[slot].resolution.vsync_end = vsync_end;
                base_paramer.aux.screen_list[slot].resolution.vtotal = vtotal;
                base_paramer.aux.screen_list[slot].resolution.flags = flags;
            } else {
                base_paramer.aux.screen_list[slot].feature |= RESOLUTION_AUTO;
                memset(&base_paramer.aux.screen_list[slot].resolution, 0, sizeof(base_paramer.aux.screen_list[slot].resolution));
            }

            memset(property,0,sizeof(property));
            property_get("persist.sys.overscan.aux", property, "overscan 100,100,100,100");
            sscanf(property, "overscan %d,%d,%d,%d",
                    &left,
                    &top,
                    &right,
                    &bottom);
            base_paramer.aux.scan.leftscale = (unsigned short)left;
            base_paramer.aux.scan.topscale = (unsigned short)top;
            base_paramer.aux.scan.rightscale = (unsigned short)right;
            base_paramer.aux.scan.bottomscale = (unsigned short)bottom;

            memset(property,0,sizeof(property));
            property_get("persist.sys.color.aux", property, "Auto");
            if (strncmp(property, "Auto", 4) != 0){
                char color[16];
                char depth[16];

                sscanf(property, "%s-%s", color, depth);
                if (strncmp(color, "RGB", 3) == 0)
                    base_paramer.aux.screen_list[slot].format = output_rgb;
                else if (strncmp(color, "YCBCR444", 8) == 0)
                    base_paramer.aux.screen_list[slot].format = output_ycbcr444;
                else if (strncmp(color, "YCBCR422", 8) == 0)
                    base_paramer.aux.screen_list[slot].format = output_ycbcr422;
                else if (strncmp(color, "YCBCR420", 8) == 0)
                    base_paramer.aux.screen_list[slot].format = output_ycbcr420;
                else {
                    base_paramer.aux.screen_list[slot].feature |= COLOR_AUTO;
                    base_paramer.aux.screen_list[slot].format = output_ycbcr_high_subsampling;
                }

                if (strncmp(depth, "8bit", 4) == 0)
                    base_paramer.aux.screen_list[slot].depthc = depth_24bit;
                else if (strncmp(depth, "10bit", 5) == 0)
                    base_paramer.aux.screen_list[slot].depthc = depth_30bit;
                else
                    base_paramer.aux.screen_list[slot].depthc = Automatic;
            } else {
                base_paramer.aux.screen_list[slot].feature |= COLOR_AUTO;
                base_paramer.aux.screen_list[slot].depthc = Automatic;
                base_paramer.aux.screen_list[slot].format = output_ycbcr_high_subsampling;
            }

            memset(property,0,sizeof(property));
            property_get("persist.sys.hdcp1x.aux", property, "0");
            if (atoi(property) > 0)
                base_paramer.aux.screen_list[slot].feature |= HDCP1X_EN;

            memset(property,0,sizeof(property));
            property_get("persist.sys.resolution_white.aux", property, "0");
            if (atoi(property) > 0)
                base_paramer.aux.screen_list[slot].feature |= RESOLUTION_WHITE_EN;
            /*add for BCSH*/
            saveBcshConfig(&base_paramer, HWC_DISPLAY_EXTERNAL_BIT);
#ifdef TEST_BASE_PARMARTER
            /*save aux fb & device*/
            saveHwcInitalInfo(&base_paramer, HWC_DISPLAY_EXTERNAL_BIT);
#endif
        }
    }

    if (mlut != NULL) {
        int mainLutSize = mlut->main.size*sizeof(uint16_t);
        int auxLutSize = mlut->aux.size*sizeof(uint16_t);
        if (mainLutSize) {
            base_paramer.main.mlutdata.size = mlut->main.size;
            memcpy(base_paramer.main.mlutdata.lred, mlut->main.lred, mainLutSize);
            memcpy(base_paramer.main.mlutdata.lgreen, mlut->main.lred, mainLutSize);
            memcpy(base_paramer.main.mlutdata.lblue, mlut->main.lred, mainLutSize);
        }

        if (auxLutSize) {
            base_paramer.aux.mlutdata.size = mlut->aux.size;
            memcpy(base_paramer.aux.mlutdata.lred, mlut->aux.lred, mainLutSize);
            memcpy(base_paramer.aux.mlutdata.lgreen, mlut->aux.lred, mainLutSize);
            memcpy(base_paramer.aux.mlutdata.lblue, mlut->aux.lred, mainLutSize);
        }
    }
    freeLutInfo();
    lseek(file, 0L, SEEK_SET);
    write(file, (char*)(&base_paramer.main), sizeof(base_paramer.main));
    lseek(file, BASE_OFFSET, SEEK_SET);
    write(file, (char*)(&base_paramer.aux), sizeof(base_paramer.aux));
    close(file);
    sync();
    /*
       ALOGD("[%s] hdmi:%d,%d,%d,%d,%d,%d foundMainIdx %d\n", __FUNCTION__,
       base_paramer.main.resolution.hdisplay,
       base_paramer.main.resolution.vdisplay,
       base_paramer.main.resolution.hsync_start,
       base_paramer.main.resolution.hsync_end,
       base_paramer.main.resolution.htotal,
       base_paramer.main.resolution.flags,
       foundMainIdx);

       ALOGD("[%s] tve:%d,%d,%d,%d,%d,%d foundAuxIdx %d\n", __FUNCTION__,
       base_paramer.aux.resolution.hdisplay,
       base_paramer.aux.resolution.vdisplay,
       base_paramer.aux.resolution.hsync_start,
       base_paramer.aux.resolution.hsync_end,
       base_paramer.aux.resolution.htotal,
       base_paramer.aux.resolution.flags,
       foundAuxIdx);
     */
}

#endif



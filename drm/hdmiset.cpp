#include "drmresources.h"
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "hdmiset.h"
#include "string.h"
#include <fcntl.h>
#include <tinyxml2.h>
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

typedef struct {
	
	uint32_t clock;
	uint32_t hdisplay;
	uint32_t hsync_start;
	uint32_t hsync_end;
	uint32_t hskew;
	uint32_t vdisplay;
	uint32_t vsync_start;
	uint32_t vsync_end;
	uint32_t vscan;
	uint32_t vrefresh;
	uint32_t flags;
	uint32_t htotal;
	
} drm_mode_info_t;

typedef struct {
	int count;
	drm_mode_info_t drm_mode_info[35];
} drm_mode_info_list_t;

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
static drm_mode_info_list_t  drm_mode_info_list;
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
   // printf("+++++hdmi_set_resolution end\n");
    return 0;
}

int UpdateDisplaySize(int src_xpos, int src_ypos, int src_w, int src_h, int dst_xpos, int dst_ypos, int dst_w, int dst_h) {
  _drm->UpdateDisplaySize(src_xpos,src_ypos,src_w,src_h,dst_xpos,dst_ypos,dst_w,dst_h);
}


int hdmi_get_resolution(Hdmi_Info_list_t *list) {
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
        if (info->screen_list[i].type !=0 && info->screen_list[i].type == type) {
            found = i;
            break;
        } 
    }
    if (found == -1) {
       for (int i=0;i<5;i++) {
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
}

int parse_white_mode() {
	
	tinyxml2::XMLDocument doc;
	int i = 0;
	doc.LoadFile("/usr/share/resolution_white.xml");

	tinyxml2::XMLElement* root=doc.RootElement();
	if (!root)
	return -1;

	tinyxml2::XMLElement* resolution = root->FirstChildElement("resolution");

	while (resolution) {
		drmModeModeInfo m;

		#define PARSE(x) \
		tinyxml2::XMLElement* _##x = resolution->FirstChildElement(#x); \
		if (!_##x) { \
		printf("------> failed to parse %s\n", #x); \
		resolution = resolution->NextSiblingElement(); \
		continue; \
		} \
		m.x = atoi(_##x->GetText())
		#define PARSE_HEX(x) \
		tinyxml2::XMLElement* _##x = resolution->FirstChildElement(#x); \
		if (!_##x) { \
		printf("------> failed to parse %s\n", #x); \
		resolution = resolution->NextSiblingElement(); \
		continue; \
		} \
		sscanf(_##x->GetText(), "%x", &m.x);
		PARSE(clock);
		PARSE(hdisplay);
		PARSE(hsync_start);
		PARSE(hsync_end);
		PARSE(hskew);
		PARSE(vdisplay);
		PARSE(vsync_start);
		PARSE(vsync_end);
		PARSE(vscan);
		PARSE(vrefresh);
		PARSE_HEX(flags);
		PARSE(htotal);
		drm_mode_info_list.drm_mode_info[i].clock = m.clock;
		drm_mode_info_list.drm_mode_info[i].hdisplay = m.hdisplay;
		drm_mode_info_list.drm_mode_info[i].hsync_start = m.hsync_start;
		drm_mode_info_list.drm_mode_info[i].hsync_end = m.hsync_end;
		drm_mode_info_list.drm_mode_info[i].hskew = m.hskew;
		drm_mode_info_list.drm_mode_info[i].vdisplay = m.vdisplay;
		drm_mode_info_list.drm_mode_info[i].vsync_start = m.vsync_start;
		drm_mode_info_list.drm_mode_info[i].vsync_end = m.vsync_end;
		drm_mode_info_list.drm_mode_info[i].vscan = m.vscan;
		drm_mode_info_list.drm_mode_info[i].vrefresh = m.vrefresh;
		drm_mode_info_list.drm_mode_info[i].flags = m.flags;
		drm_mode_info_list.drm_mode_info[i].htotal = m.htotal;
		i++;
		/* add modes in "resolution.xml" to white list */
		resolution = resolution->NextSiblingElement();
	}
	drm_mode_info_list.count = i;
	return 0;
}

bool check_mode(void* mode_info) {
	
	if (mode_info == NULL) {
		return false;
	}
	drmModeModeInfo *m = (drmModeModeInfo*)mode_info;

	for (int i=0; i<drm_mode_info_list.count; i++) {
		if (drm_mode_info_list.drm_mode_info[i].clock == m->clock && \
			drm_mode_info_list.drm_mode_info[i].hdisplay == m->hdisplay && \
			drm_mode_info_list.drm_mode_info[i].hsync_start == m->hsync_start && \
			drm_mode_info_list.drm_mode_info[i].hsync_end == m->hsync_end && \
			drm_mode_info_list.drm_mode_info[i].vdisplay == m->vdisplay&& \
			drm_mode_info_list.drm_mode_info[i].vsync_start == m->vsync_start&& \
			drm_mode_info_list.drm_mode_info[i].vsync_end == m->vsync_end&& \
			drm_mode_info_list.drm_mode_info[i].vrefresh == m->vrefresh&& \
			drm_mode_info_list.drm_mode_info[i].flags == m->flags&& \
			drm_mode_info_list.drm_mode_info[i].htotal == m->htotal
			) {
			printf("check_mode is true:clock=%d,hdisplay=%d,hsync_start=%d,hsync_end=%d,\
					vdisplay=%d,vsync_start=%d,vsync_end=%d,vrefresh=%d,flags=0x%x,htotal=%d\n",
					m->clock,m->hdisplay,m->hsync_start,m->hsync_end,m->vdisplay,m->vsync_start,\
					m->vsync_end,m->vrefresh,m->flags,m->htotal);
			return true;
		}
		
	}
    
	return false;
}








#include "hdmiset.h"
#include "stdio.h"
//using namespace android;


#if 0
void read_hdmi_modes() {
	
	//hdmoModes.insert(pair<int,int>(channel, sid));
     
	
	
}

int main() {
    //ShowHelp();
	int display = 0;
	DrmResources *drm = new DrmResources();
	drm->Init();
	drmModeModeInfo m;
	m.clock = 27000;
	m.hdisplay = 720;
	m.hsync_start = 732;
	m.hsync_end = 796;
	m.hskew = 0;
	m.vdisplay = 576;
	m.vsync_start = 581;
	m.vsync_end = 586;
	m.vscan = 0;
	m.vrefresh = 50;
	m.flags = 0x8001A;
	m.htotal = 864;
	m.vtotal = 625;
	
    //DrmMode mode(&m);
	DrmConnector *c = drm->GetConnectorFromType(display);

	for (const DrmMode &conn_mode : c->modes()) {
        if (conn_mode.equal(m.hdisplay ,m.vdisplay , m.vrefresh, m.hsync_start, m.hsync_end,
                            m.htotal, m.vsync_start, m.vsync_end, m.vtotal, m.flags)) {
          c->set_best_mode(conn_mode);
		  c->set_current_mode(conn_mode);	
		  printf("++++++set_current_mode\n");
         // return 0;
        }
    }
/*
    if (c != NULL) {
	      c->set_best_mode(mode);
	      c->set_current_mode(mode);	
		  c->UpdateModes();
	} else {
		printf("DrmConnector object is null.\n");
		return 0;
	} 
*/
	
    while(true){
        printf("   please input: ");
        char t = getchar();
        if(t != 10){    //filter retrun char
		  switch(t) {
			case '1':
			  drm->UpdateDisplayRoute();
			  break;
			case '2':
			  break;
			case '3':
			  break;
			case '4':
			  break;
			case '5':
			  break;
			case '6':
			  break;
			case '7':
			  break;
			default:
			  break; 
			 
		  }

		
		}
	}
  return 0;

}

#endif


int main() {

   init_hdmi_set(0);
   Hdmi_Info_list_t list;
   int mode = -1;
   int r = hdmi_get_resolution(&list);

   if (r!=0) {
      return -1;
   }

   if (list.numHdmiMode == 0) {
      printf("hdmi mode num is 0\n");
      return -1;
   }
   printf("-------------------------------------------------------------------\n");
   for (int i=0; i<list.numHdmiMode; i++) {
      HdmiInfo_t *hdmimode = &list.hdmimode[i]; 
      printf("mode[%d], xres=%d, yres=%d, refresh=%d, interlace=%d\n",\
              i, hdmimode->xres, hdmimode->yres, hdmimode->refresh, hdmimode->interlaced);

   }
   
    while(true){
        printf("   please input: ");
        char t = getchar();
        if(t != 10){    //filter retrun char 
		  switch(t) {
            case '0':
              mode = 0;
              break;
			case '1':
              mode = 1;
			  break;
			case '2':
              mode = 2;
			  break;
			case '3':
              mode = 3;
			  break;
			case '4':
              mode = 4;
			  break;
			case '5':
              mode = 5;
			  break;
			case '6':
              mode = 6;
			  break;
			case '7':
              mode = 7;
			  break;
            case '8':
                mode = 8;
                break;
            case '9':
                mode = 9;
                break;
            case 'a':
                mode = 10;
                break;
            case 'b':
                mode = 11;
                break;
            case 'c':
                mode = 12;
                break;
            case 'd':
                mode = 13;
                break;
            case 'e':
                mode = 14;
                break;
            case 'f':
                mode = 15;
                break;
            case 'g':
                mode = 16;
                break;
            case 'h':
                mode = 17;
                break;
            case 'i':
                mode = 18;
                break;
            case 'j':
                mode = 19;
                break;
            case 'k':
                mode = 20;
                break;
            case 'l':
                mode = 21;
                break;
			default:
			  break; 
			 
		  }

		  
		}
        if (mode >= 0) {
            printf("###########mode=%d\n",mode);
            HdmiInfo_t *hdmimode = &list.hdmimode[mode]; 
            
            hdmi_set_resolution(hdmimode->xres, hdmimode->yres, hdmimode->refresh, hdmimode->interlaced);
             // deInit_hdmi_set();
        }
	}
 
   return 0;
}



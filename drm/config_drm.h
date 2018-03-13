enum {
    HWC_DISPLAY_PRIMARY     = 0,
    HWC_DISPLAY_EXTERNAL    = 1,    // HDMI, DP, etc.
    HWC_DISPLAY_VIRTUAL     = 2,

    HWC_NUM_PHYSICAL_DISPLAY_TYPES = 1,
    HWC_NUM_DISPLAY_TYPES          = 3,
};

enum {
    HWC_DISPLAY_PRIMARY_BIT     = 1 << HWC_DISPLAY_PRIMARY,
    HWC_DISPLAY_EXTERNAL_BIT    = 1 << HWC_DISPLAY_EXTERNAL,
    HWC_DISPLAY_VIRTUAL_BIT     = 1 << HWC_DISPLAY_VIRTUAL,
};


bool log_level(int log_level)
{
    return log_level;
}


#ifndef GEFFECTCOMMON_H
#define GEFFECTCOMMON_H

enum G_EFFECT_ID
{
    EFX_ID_MOTION = 0,
    EFX_ID_STILL,
    EFX_ID_MULTIPLE,
    EFX_ID_TRAIL,
    EFX_ID_LOOP,
    EFX_ID_TRASH,
    EFX_ID_INPAINT,
    EFX_ID_BLACK,
    G_EFX_NUMBER
};

static int G_EFX_PRIORITY[] = {
    3, // motion
    1, // still
    5, // multi
    2, // trail
    2, // loop
    0, // trash
    0, // inpaint
    6 // border
};

static const int G_EFX_PRIORITY_STEP = 10;

static char G_EFFECT_STR[][20] = {
    "Motion",
    "Still",
    "MultiStill",
    "Trail",
    "Loop",
    "Trash",
    "Inpaint",
    "Black"
};

static const float G_TRANS_LEVEL[] = {
    1.f,
    0.8f,
    0.5f,
    0.2f,
    0.f
};

static const int FadeDuration[] = {
    0,
    5,
    10,
    15,
    20,
};

#endif // GEFFECTCOMMON_H

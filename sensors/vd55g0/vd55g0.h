/**
  ******************************************************************************
  * @file    vd55g0.h
  * @author  MDG Application Team
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#ifndef VD55G0_H
#define VD55G0_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define VD55G0_MAX_WIDTH  644U
#define VD55G0_MAX_HEIGHT 604U

#define VD55G0_ANALOG_GAIN_MIN  0
#define VD55G0_ANALOG_GAIN_MAX  24
#define VD55G0_DIGITAL_GAIN_MIN 0x0100U
#define VD55G0_DIGITAL_GAIN_MAX 0x0800U

#define VD55G0_MIN_EXPOSURE     19U
#define VD55G0_MAX_EXPOSURE     32744U

#define VD55G0_MIN_FPS          2
#define VD55G0_MAX_FPS          152

/* VD55G0 Sensor Context Structure */
typedef struct VD55G0_Ctx {
  /* I2C Interface */
  int (*ReadReg)(uint16_t Addr, uint16_t Reg, uint8_t *pData, uint16_t Size);
  int (*WriteReg)(uint16_t Addr, uint16_t Reg, const uint8_t *pData, uint16_t Size);
  uint16_t Address;

  /* Power Control */
  int (*ShutdownPin)(uint8_t State);

  /* Timing */
  uint32_t (*GetTick)(void);
  void (*Delay)(uint32_t DelayMs);

  /* Sensor State */
  uint8_t IsInitialized;
  uint8_t CurrentContext;
  uint16_t LineLength;
  uint32_t CurrentWidth;
  uint32_t CurrentHeight;
  uint32_t PixelDepth;
} VD55G0_Ctx;

/* VD55G0 Public Functions */

/**
  * @brief  Initialize VD55G0 sensor
  * @param  ctx     Pointer to VD55G0 context
  * @param  width   Image width
  * @param  height  Image height
  * @param  fps     Frame rate
  * @return 0 if successful, error code otherwise
  */
int VD55G0_Init(VD55G0_Ctx *ctx, uint32_t width, uint32_t height, uint32_t fps);

/**
  * @brief  De-initialize VD55G0 sensor
  * @param  ctx     Pointer to VD55G0 context
  * @return 0 if successful
  */
int VD55G0_DeInit(VD55G0_Ctx *ctx);

/**
  * @brief  Start streaming from VD55G0
  * @param  ctx     Pointer to VD55G0 context
  * @return 0 if successful
  */
int VD55G0_Start(VD55G0_Ctx *ctx);

/**
  * @brief  Stop streaming from VD55G0
  * @param  ctx     Pointer to VD55G0 context
  * @return 0 if successful
  */
int VD55G0_Stop(VD55G0_Ctx *ctx);

/**
  * @brief  Set sensor gain
  * @param  ctx     Pointer to VD55G0 context
  * @param  gain    Gain value in millidecibels (mdB)
  * @return 0 if successful
  */
int VD55G0_SetGain(VD55G0_Ctx *ctx, int32_t gain);

/**
  * @brief  Set sensor exposure time
  * @param  ctx         Pointer to VD55G0 context
  * @param  exposure    Exposure time in coarse integration lines
  * @return 0 if successful
  */
int VD55G0_SetExposure(VD55G0_Ctx *ctx, int32_t exposure);

/**
  * @brief  Set exposure mode (Manual/Auto/Freeze)
  * @param  ctx     Pointer to VD55G0 context
  * @param  mode    Exposure mode (0=Auto, 1=Freeze, 2=Manual)
  * @return 0 if successful
  */
int VD55G0_SetExposureMode(VD55G0_Ctx *ctx, uint8_t mode);

/**
  * @brief  Set test pattern
  * @param  ctx      Pointer to VD55G0 context
  * @param  pattern  Pattern type
  * @return 0 if successful
  */
int VD55G0_SetTestPattern(VD55G0_Ctx *ctx, uint16_t pattern);

/**
  * @brief  Set pixel format (RAW8 or RAW10)
  * @param  ctx      Pointer to VD55G0 context
  * @param  format   Format (0x08=RAW8, 0x0A=RAW10)
  * @return 0 if successful
  */
int VD55G0_SetPixelFormat(VD55G0_Ctx *ctx, uint8_t format);

/**
  * @brief  Get sensor information (min/max exposure, gain ranges)
  * @param  ctx      Pointer to VD55G0 context
  * @param  out_min_exp   Pointer to store minimum exposure
  * @param  out_max_exp   Pointer to store maximum exposure
  * @param  out_again_max Pointer to store max analog gain (mdB)
  * @param  out_dgain_max Pointer to store max digital gain (mdB)
  * @return 0 if successful
  */
int VD55G0_GetSensorInfo(VD55G0_Ctx *ctx, uint32_t *out_min_exp, uint32_t *out_max_exp,
                         uint32_t *out_again_max, uint32_t *out_dgain_max);

/**
  * @brief  Get default PHY bitrate for VD55G0
  * @return PHY bitrate in bps (804000000 for 804 Mbps)
  */
uint32_t VD55G0_GetDefaultPHYBitrate(void);

#ifdef __cplusplus
}
#endif

#endif /* VD55G0_H */

/**
  ******************************************************************************
  * @file    cmw_vd55g0.h
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

#ifndef CMW_VD55G0
#define CMW_VD55G0

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "cmw_sensors_if.h"
#include "cmw_errno.h"
#include "cmw_camera.h"

#define VD55G0_NAME                 "VD55G0"
#define VD55G0_MAX_WIDTH            644U
#define VD55G0_MAX_HEIGHT           604U
#define VD55G0_DEFAULT_DATARATE     804000000U

typedef struct
{
  uint16_t Address;
  uint8_t IsInitialized;
  uint8_t CurrentContext;
  uint16_t LineLength;
  uint32_t CurrentWidth;
  uint32_t CurrentHeight;
  uint32_t PixelDepth;
  int32_t (*Init)(void);
  int32_t (*DeInit)(void);
  int32_t (*WriteReg)(uint16_t, uint16_t, uint8_t *, uint16_t);
  int32_t (*ReadReg)(uint16_t, uint16_t, uint8_t *, uint16_t);
  int32_t (*GetTick)(void);
  void (*Delay)(uint32_t delay_in_ms);
  void (*ShutdownPin)(int value);
  void (*EnablePin)(int value);
} CMW_VD55G0_t;

int CMW_VD55G0_Probe(CMW_VD55G0_t *io_ctx, CMW_Sensor_if_t *vd55g0_if);
void CMW_VD55G0_SetDefaultSensorValues(CMW_VD55G0_config_t *vd55g0_config);

#ifdef __cplusplus
}
#endif

#endif

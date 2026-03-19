/**
  ******************************************************************************
  * @file    cmw_vd55g0.c
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

#include "cmw_vd55g0.h"
#include "vd55g0/vd55g0.h"
#include "cmw_io.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#define VD55G0_CHIP_ID 0x53354730UL
#define container_of(ptr, type, member) (type *) ((unsigned char *)ptr - offsetof(type,member))
#define MDECIBEL_TO_LINEAR(mdB)      (pow(10.0, ((double)(mdB) / 1000.0) / 20.0))
#define LINEAR_TO_MDECIBEL(linearValue) (1000.0 * (20.0 * log10(linearValue)))
#define FP88_TO_FLOAT(fp)            (((fp) >> 8) + (((fp) & 0xFFU) / 256.0))

#define VD55G0_NAME "VD55G0"
#define VD55G0_DEVICE_MODEL_ID_ADD 0x0000U
#define VD55G0_DEFAULT_DATARATE    804000000U

/* Middleware callback adapters that bridge CMW I2C to VD55G0 driver context */
static int CMW_VD55G0_ReadRef_Bridge(uint16_t Addr, uint16_t Reg, uint8_t *pData, uint16_t Size)
{
  return CMW_I2C_READREG16(Addr, Reg, pData, Size);
}

static int CMW_VD55G0_WriteReg_Bridge(uint16_t Addr, uint16_t Reg, const uint8_t *pData, uint16_t Size)
{
  return CMW_I2C_WRITEREG16(Addr, Reg, pData, Size);
}

static uint32_t CMW_VD55G0_GetTick_Bridge(void)
{
  return HAL_GetTick();
}

static int CMW_VD55G0_ReadID(uint16_t address, uint32_t *id)
{
  uint8_t data[4];
  int ret;

  if (id == NULL)
    return CMW_ERROR_WRONG_PARAM;

  ret = CMW_VD55G0_ReadRef_Bridge(address, VD55G0_DEVICE_MODEL_ID_ADD, data, 4);
  if (ret != CMW_ERROR_NONE)
    return ret;

  *id = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
  return CMW_ERROR_NONE;
}

static void CMW_VD55G0_PowerOn(void)
{
  CMW_CAMERA_ShutdownPin(0);
  HAL_Delay(200);
  CMW_CAMERA_ShutdownPin(1);
  HAL_Delay(20);
}

/* Middleware-level Init function */
static int32_t CMW_VD55G0_Init(void *io_ctx, CMW_Sensor_Init_t *initSensor)
{
  CMW_VD55G0_t *cmw_ctx = (CMW_VD55G0_t *)io_ctx;
  VD55G0_Ctx vd_ctx = {0};
  CMW_VD55G0_config_t *sensor_config;
  uint8_t pixel_format = 0x0A; /* RAW10 default */
  int ret;

  if ((cmw_ctx == NULL) || (initSensor == NULL))
    return CMW_ERROR_WRONG_PARAM;

  sensor_config = (CMW_VD55G0_config_t *)initSensor->sensor_config;
  if (sensor_config == NULL)
    return CMW_ERROR_WRONG_PARAM;

  if (cmw_ctx->IsInitialized != 0U)
    return CMW_ERROR_NONE;

  if ((initSensor->width > VD55G0_MAX_WIDTH) || (initSensor->height > VD55G0_MAX_HEIGHT))
    return CMW_ERROR_WRONG_PARAM;

  if ((initSensor->width == 0U) || (initSensor->height == 0U)) {
    initSensor->width = VD55G0_MAX_WIDTH;
    initSensor->height = VD55G0_MAX_HEIGHT;
  }

  /* Map pixel format */
  switch (sensor_config->pixel_format) {
    case CMW_PIXEL_FORMAT_RAW8:
      pixel_format = 0x08U;
      cmw_ctx->PixelDepth = 8U;
      break;
    case CMW_PIXEL_FORMAT_DEFAULT:
    case CMW_PIXEL_FORMAT_RAW10:
      pixel_format = 0x0AU;
      cmw_ctx->PixelDepth = 10U;
      break;
    default:
      return CMW_ERROR_WRONG_PARAM;
  }

  /* Setup VD55G0 driver context */
  vd_ctx.Address = CAMERA_VD55G0_ADDRESS;
  vd_ctx.ReadReg = CMW_VD55G0_ReadRef_Bridge;
  vd_ctx.WriteReg = CMW_VD55G0_WriteReg_Bridge;
  vd_ctx.GetTick = CMW_VD55G0_GetTick_Bridge;
  vd_ctx.Delay = HAL_Delay;
  vd_ctx.ShutdownPin = CMW_CAMERA_ShutdownPin;
  vd_ctx.IsInitialized = 0;

  /* Initialize the VD55G0 sensor */
  ret = VD55G0_Init(&vd_ctx, initSensor->width, initSensor->height, initSensor->fps);
  if (ret != 0)
    return CMW_ERROR_COMPONENT_FAILURE;

  /* Set pixel format */
  ret = VD55G0_SetPixelFormat(&vd_ctx, pixel_format);
  if (ret != 0)
    return CMW_ERROR_COMPONENT_FAILURE;

  /* Save state to CMW context */
  cmw_ctx->IsInitialized = 1U;
  cmw_ctx->CurrentWidth = initSensor->width;
  cmw_ctx->CurrentHeight = initSensor->height;

  return CMW_ERROR_NONE;
}

/* Middleware-level Start function */
static int32_t CMW_VD55G0_Start(void *io_ctx)
{
  CMW_VD55G0_t *cmw_ctx = (CMW_VD55G0_t *)io_ctx;
  VD55G0_Ctx vd_ctx = {0};
  int ret;

  if ((cmw_ctx == NULL) || (cmw_ctx->IsInitialized == 0U))
    return CMW_ERROR_COMPONENT_FAILURE;

  /* Rebuild VD55G0 context from CMW context */
  vd_ctx.Address = CAMERA_VD55G0_ADDRESS;
  vd_ctx.ReadReg = CMW_VD55G0_ReadRef_Bridge;
  vd_ctx.WriteReg = CMW_VD55G0_WriteReg_Bridge;
  vd_ctx.GetTick = CMW_VD55G0_GetTick_Bridge;
  vd_ctx.Delay = HAL_Delay;
  vd_ctx.IsInitialized = 1;

  ret = VD55G0_Start(&vd_ctx);
  return (ret == 0) ? CMW_ERROR_NONE : CMW_ERROR_COMPONENT_FAILURE;
}

/* Middleware-level Stop function */
static int32_t CMW_VD55G0_Stop(void *io_ctx)
{
  CMW_VD55G0_t *cmw_ctx = (CMW_VD55G0_t *)io_ctx;
  VD55G0_Ctx vd_ctx = {0};
  int ret;

  if (cmw_ctx == NULL)
    return CMW_ERROR_COMPONENT_FAILURE;

  /* Rebuild VD55G0 context */
  vd_ctx.Address = CAMERA_VD55G0_ADDRESS;
  vd_ctx.ReadReg = CMW_VD55G0_ReadRef_Bridge;
  vd_ctx.WriteReg = CMW_VD55G0_WriteReg_Bridge;
  vd_ctx.GetTick = CMW_VD55G0_GetTick_Bridge;
  vd_ctx.Delay = HAL_Delay;
  vd_ctx.IsInitialized = 1;

  ret = VD55G0_Stop(&vd_ctx);
  return (ret == 0) ? CMW_ERROR_NONE : CMW_ERROR_COMPONENT_FAILURE;
}

/* Middleware-level DeInit function */
static int32_t CMW_VD55G0_DeInit(void *io_ctx)
{
  CMW_VD55G0_t *cmw_ctx = (CMW_VD55G0_t *)io_ctx;
  VD55G0_Ctx vd_ctx = {0};

  if (cmw_ctx == NULL)
    return CMW_ERROR_COMPONENT_FAILURE;

  /* Rebuild VD55G0 context */
  vd_ctx.Address = CAMERA_VD55G0_ADDRESS;
  vd_ctx.ReadReg = CMW_VD55G0_ReadRef_Bridge;
  vd_ctx.WriteReg = CMW_VD55G0_WriteReg_Bridge;
  vd_ctx.GetTick = CMW_VD55G0_GetTick_Bridge;
  vd_ctx.Delay = HAL_Delay;
  vd_ctx.IsInitialized = 1;

  VD55G0_DeInit(&vd_ctx);
  CMW_CAMERA_ShutdownPin(0);
  cmw_ctx->IsInitialized = 0U;

  return CMW_ERROR_NONE;
}

/* Middleware-level SetGain function */
static int32_t CMW_VD55G0_SetGain(void *io_ctx, int32_t gain)
{
  CMW_VD55G0_t *cmw_ctx = (CMW_VD55G0_t *)io_ctx;
  VD55G0_Ctx vd_ctx = {0};
  int ret;

  if ((cmw_ctx == NULL) || (cmw_ctx->IsInitialized == 0U))
    return CMW_ERROR_WRONG_PARAM;

  /* Rebuild VD55G0 context */
  vd_ctx.Address = CAMERA_VD55G0_ADDRESS;
  vd_ctx.ReadReg = CMW_VD55G0_ReadRef_Bridge;
  vd_ctx.WriteReg = CMW_VD55G0_WriteReg_Bridge;
  vd_ctx.GetTick = CMW_VD55G0_GetTick_Bridge;
  vd_ctx.Delay = HAL_Delay;
  vd_ctx.IsInitialized = 1;

  ret = VD55G0_SetGain(&vd_ctx, gain);
  return (ret == 0) ? CMW_ERROR_NONE : CMW_ERROR_COMPONENT_FAILURE;
}

/* Middleware-level SetExposure function */
static int32_t CMW_VD55G0_SetExposure(void *io_ctx, int32_t exposure)
{
  CMW_VD55G0_t *cmw_ctx = (CMW_VD55G0_t *)io_ctx;
  VD55G0_Ctx vd_ctx = {0};
  int ret;

  if ((cmw_ctx == NULL) || (cmw_ctx->IsInitialized == 0U))
    return CMW_ERROR_WRONG_PARAM;

  /* Rebuild VD55G0 context */
  vd_ctx.Address = CAMERA_VD55G0_ADDRESS;
  vd_ctx.ReadReg = CMW_VD55G0_ReadRef_Bridge;
  vd_ctx.WriteReg = CMW_VD55G0_WriteReg_Bridge;
  vd_ctx.GetTick = CMW_VD55G0_GetTick_Bridge;
  vd_ctx.Delay = HAL_Delay;
  vd_ctx.IsInitialized = 1;

  ret = VD55G0_SetExposure(&vd_ctx, exposure);
  return (ret == 0) ? CMW_ERROR_NONE : CMW_ERROR_COMPONENT_FAILURE;
}

/* Middleware-level SetExposureMode function */
static int32_t CMW_VD55G0_SetExposureMode(void *io_ctx, int32_t mode)
{
  CMW_VD55G0_t *cmw_ctx = (CMW_VD55G0_t *)io_ctx;
  VD55G0_Ctx vd_ctx = {0};
  uint8_t sensor_mode = 0x02; /* Default to manual */
  int ret;

  if (cmw_ctx == NULL)
    return CMW_ERROR_WRONG_PARAM;

  switch (mode) {
    case CMW_EXPOSUREMODE_AUTO:
      sensor_mode = 0x00U;
      break;
    case CMW_EXPOSUREMODE_AUTOFREEZE:
      sensor_mode = 0x01U;
      break;
    case CMW_EXPOSUREMODE_MANUAL:
      sensor_mode = 0x02U;
      break;
    default:
      return CMW_ERROR_WRONG_PARAM;
  }

  /* Rebuild VD55G0 context */
  vd_ctx.Address = CAMERA_VD55G0_ADDRESS;
  vd_ctx.ReadReg = CMW_VD55G0_ReadRef_Bridge;
  vd_ctx.WriteReg = CMW_VD55G0_WriteReg_Bridge;
  vd_ctx.GetTick = CMW_VD55G0_GetTick_Bridge;
  vd_ctx.Delay = HAL_Delay;
  vd_ctx.IsInitialized = 1;

  ret = VD55G0_SetExposureMode(&vd_ctx, sensor_mode);
  return (ret == 0) ? CMW_ERROR_NONE : CMW_ERROR_COMPONENT_FAILURE;
}

/* Middleware-level GetSensorInfo function */
static int32_t CMW_VD55G0_GetSensorInfo(void *io_ctx, ISP_SensorInfoTypeDef *info)
{
  CMW_VD55G0_t *cmw_ctx = (CMW_VD55G0_t *)io_ctx;
  VD55G0_Ctx vd_ctx = {0};
  uint32_t exp_min, exp_max, again_max, dgain_max;
  int ret;

  if ((cmw_ctx == NULL) || (info == NULL))
    return CMW_ERROR_WRONG_PARAM;

  if (sizeof(info->name) < (strlen(VD55G0_NAME) + 1U))
    return CMW_ERROR_WRONG_PARAM;

  strcpy(info->name, VD55G0_NAME);
  info->bayer_pattern = ISP_DEMOS_TYPE_MONO;
  info->color_depth = cmw_ctx->PixelDepth;
  info->width = cmw_ctx->CurrentWidth;
  info->height = cmw_ctx->CurrentHeight;

  /* Rebuild VD55G0 context */
  vd_ctx.Address = CAMERA_VD55G0_ADDRESS;
  vd_ctx.ReadReg = CMW_VD55G0_ReadRef_Bridge;
  vd_ctx.WriteReg = CMW_VD55G0_WriteReg_Bridge;
  vd_ctx.GetTick = CMW_VD55G0_GetTick_Bridge;
  vd_ctx.Delay = HAL_Delay;
  vd_ctx.IsInitialized = 1;

  ret = VD55G0_GetSensorInfo(&vd_ctx, &exp_min, &exp_max, &again_max, &dgain_max);
  if (ret != 0)
    return CMW_ERROR_COMPONENT_FAILURE;

  info->gain_min = 0;
  info->again_max = (int32_t)again_max;
  info->gain_max = (int32_t)(again_max + dgain_max);
  info->exposure_min = exp_min;
  info->exposure_max = exp_max;

  return CMW_ERROR_NONE;
}

/* Middleware-level SetTestPattern function */
static int32_t CMW_VD55G0_SetTestPattern(void *io_ctx, int32_t mode)
{
  CMW_VD55G0_t *cmw_ctx = (CMW_VD55G0_t *)io_ctx;
  VD55G0_Ctx vd_ctx = {0};
  uint16_t reg_value = 0;
  int ret;

  if (cmw_ctx == NULL)
    return CMW_ERROR_WRONG_PARAM;

  switch (mode) {
    case 0:
      reg_value = 0x0000U;
      break;
    case 1:
      reg_value = 0x0201U;
      break;
    case 2:
      reg_value = 0x0211U;
      break;
    case 3:
      reg_value = 0x0221U;
      break;
    case 4:
      reg_value = 0x0281U;
      break;
    default:
      return CMW_ERROR_WRONG_PARAM;
  }

  /* Rebuild VD55G0 context */
  vd_ctx.Address = CAMERA_VD55G0_ADDRESS;
  vd_ctx.ReadReg = CMW_VD55G0_ReadRef_Bridge;
  vd_ctx.WriteReg = CMW_VD55G0_WriteReg_Bridge;
  vd_ctx.GetTick = CMW_VD55G0_GetTick_Bridge;
  vd_ctx.Delay = HAL_Delay;
  vd_ctx.IsInitialized = 1;

  ret = VD55G0_SetTestPattern(&vd_ctx, reg_value);
  return (ret == 0) ? CMW_ERROR_NONE : CMW_ERROR_COMPONENT_FAILURE;
}

/* Middleware-level GetDefaultPHYBitrate function */
static int32_t CMW_VD55G0_GetDefaultPHYBitrate(void *io_ctx, int32_t *bitrate)
{
  UNUSED(io_ctx);

  if (bitrate == NULL)
    return CMW_ERROR_WRONG_PARAM;

  *bitrate = (int32_t)VD55G0_GetDefaultPHYBitrate();
  return CMW_ERROR_NONE;
}

/* Probe function - detect sensor and populate interface */
int CMW_VD55G0_Probe(CMW_VD55G0_t *io_ctx, CMW_Sensor_if_t *vd55g0_if)
{
  uint32_t id = 0;
  int ret;

  if ((io_ctx == NULL) || (vd55g0_if == NULL) || (io_ctx->Init == NULL) ||
      (io_ctx->ReadReg == NULL) || (io_ctx->WriteReg == NULL)) {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  CMW_VD55G0_PowerOn();
  ret = io_ctx->Init();
  if (ret != CMW_ERROR_NONE)
    return CMW_ERROR_COMPONENT_FAILURE;

  ret = CMW_VD55G0_ReadID(CAMERA_VD55G0_ADDRESS, &id);
  if (ret != CMW_ERROR_NONE)
    return CMW_ERROR_COMPONENT_FAILURE;

  if ((id == 0UL) || (id == 0xFFFFFFFFUL))
    return CMW_ERROR_COMPONENT_FAILURE;

  if ((id != VD55G0_CHIP_ID) && (id != 0x53354731UL))
    return CMW_ERROR_COMPONENT_FAILURE;

  /* Populate sensor interface */
  memset(vd55g0_if, 0, sizeof(*vd55g0_if));
  vd55g0_if->Init = CMW_VD55G0_Init;
  vd55g0_if->DeInit = CMW_VD55G0_DeInit;
  vd55g0_if->Start = CMW_VD55G0_Start;
  vd55g0_if->Stop = CMW_VD55G0_Stop;
  vd55g0_if->SetGain = CMW_VD55G0_SetGain;
  vd55g0_if->SetExposure = CMW_VD55G0_SetExposure;
  vd55g0_if->SetExposureMode = CMW_VD55G0_SetExposureMode;
  vd55g0_if->GetSensorInfo = CMW_VD55G0_GetSensorInfo;
  vd55g0_if->SetTestPattern = CMW_VD55G0_SetTestPattern;
  vd55g0_if->GetDefaultPHYBitrate = CMW_VD55G0_GetDefaultPHYBitrate;

  return CMW_ERROR_NONE;
}

/* Set default sensor configuration */
void CMW_VD55G0_SetDefaultSensorValues(CMW_VD55G0_config_t *vd55g0_config)
{
  assert(vd55g0_config != NULL);
  vd55g0_config->pixel_format = CMW_PIXEL_FORMAT_RAW10;
  vd55g0_config->CSI_PHYBitrate = VD55G0_DEFAULT_DATARATE;
}

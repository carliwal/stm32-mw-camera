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
#include "cmw_io.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#define VD55G0_CHIP_ID                            0x53354730UL

#define VD55G0_STATE_READY_TO_BOOT                0x01U
#define VD55G0_STATE_SW_STDBY                     0x02U
#define VD55G0_STATE_STREAM_ON                    0x03U
#define VD55G0_STATE_ERROR                        0xFFU

#define VD55G0_CMD_ACK                            0x00U
#define VD55G0_CMD_BOOT                           0x01U
#define VD55G0_CMD_PATCH_SETUP                    0x02U

#define VD55G0_BOOT_REG_ADD                       0x0200U
#define VD55G0_SW_STANDBY_REG                     0x0201U
#define VD55G0_STREAMING_REG                      0x0202U
#define VD55G0_EXT_CLOCK_ADD                      0x0220U
#define VD55G0_FORMAT_CTRL_ADD                    0x030AU
#define VD55G0_LINE_LENGTH_ADD                    0x0300U
#define VD55G0_PATGEN_CTRL                        0x0400U
#define VD55G0_EXP_MODE_ADD(ctx)                  (0x044CU + (48U * (uint16_t)(ctx)))
#define VD55G0_MANUAL_ANALOG_GAIN_ADD(ctx)        (0x044DU + (48U * (uint16_t)(ctx)))
#define VD55G0_MANUAL_COARSE_EXPOSURE_ADD(ctx)    (0x044EU + (48U * (uint16_t)(ctx)))
#define VD55G0_MANUAL_DIGITAL_GAIN_ADD(ctx)       (0x0450U + (48U * (uint16_t)(ctx)))
#define VD55G0_FRAME_LENGTH_ADD(ctx)              (0x0458U + (48U * (uint16_t)(ctx)))
#define VD55G0_GPIO_0_CTRL_ADD(ctx)               (0x0467U + (48U * (uint16_t)(ctx)))
#define VD55G0_STROBE_START_DELAY(ctx)            (0x046DU + (48U * (uint16_t)(ctx)))
#define VD55G0_CURRENT_CONTEXT_ADD                0x0056U
#define VD55G0_SYSTEM_FSM_ADD                     0x002CU
#define VD55G0_DEVICE_MODEL_ID_ADD                0x0000U
#define VD55G0_MIN_EXPOSURE_IN_CURRENT_CONFIG_ADD 0x012EU
#define VD55G0_MAX_EXPOSURE_IN_CURRENT_CONFIG_ADD 0x0138U

#define VD55G0_AE_MODE_MANUAL                     0x02U
#define VD55G0_START_STREAM                       0x01U
#define VD55G0_STOP_STREAM                        0x01U
#define VD55G0_STROBE_MODE                        0x02U
#define VD55G0_RAW8                               0x08U
#define VD55G0_RAW10                              0x0AU

#define VD55G0_PATCH_START_ADDR                   0x2000U

#define VD55G0_MIN_EXPOSURE                       19U
#define VD55G0_MAX_EXPOSURE                       32744U
#define VD55G0_MIN_LINE_LENGTH                    1200U
#define VD55G0_DEFAULT_LINE_LENGTH                1200U
#define VD55G0_MAX_FRAME_LENGTH                   65535U
#define VD55G0_MAX_FRAME_RATE                     152.0f
#define VD55G0_PIXEL_CLOCK                        124000000.0f
#define VD55G0_V_SIZE_MAX                         604.0f

#define VD55G0_ANALOG_GAIN_MIN                    0
#define VD55G0_ANALOG_GAIN_MAX                    24
#define VD55G0_DIGITAL_GAIN_MIN                   0x0100U
#define VD55G0_DIGITAL_GAIN_MAX                   0x0800U

#define MDECIBEL_TO_LINEAR(mdB)                   (pow(10.0, ((double)(mdB) / 1000.0) / 20.0))
#define LINEAR_TO_MDECIBEL(linearValue)           (1000.0 * (20.0 * log10(linearValue)))
#define FP88_TO_FLOAT(fp)                         (((fp) >> 8) + (((fp) & 0xFFU) / 256.0))
#define FLOAT_TO_FP88(x)                          ((((uint16_t)(x)) << 8) | ((uint16_t)(((x) - (uint16_t)(x)) * 256.0f) & 0xFFU))

#define VD55G0_WAIT_TIMEOUT_MS                    5000U

extern const uint8_t vd55g0_patch[];
extern const uint32_t vd55g0_patch_len;

static int CMW_VD55G0_Read8(CMW_VD55G0_t *ctx, uint16_t addr, uint8_t *value)
{
  return ctx->ReadReg(ctx->Address, addr, value, 1);
}

static int CMW_VD55G0_Read16(CMW_VD55G0_t *ctx, uint16_t addr, uint16_t *value)
{
  uint8_t data[2];
  int ret;

  ret = ctx->ReadReg(ctx->Address, addr, data, 2);
  if (ret != 0)
  {
    return ret;
  }

  *value = ((uint16_t)data[1] << 8) | data[0];
  return CMW_ERROR_NONE;
}

static int CMW_VD55G0_Read32(CMW_VD55G0_t *ctx, uint16_t addr, uint32_t *value)
{
  uint8_t data[4];
  int ret;

  ret = ctx->ReadReg(ctx->Address, addr, data, 4);
  if (ret != 0)
  {
    return ret;
  }

  *value = ((uint32_t)data[3] << 24) | ((uint32_t)data[2] << 16) | ((uint32_t)data[1] << 8) | data[0];
  return CMW_ERROR_NONE;
}

static int CMW_VD55G0_Write8(CMW_VD55G0_t *ctx, uint16_t addr, uint8_t value)
{
  return ctx->WriteReg(ctx->Address, addr, &value, 1);
}

static int CMW_VD55G0_Write16(CMW_VD55G0_t *ctx, uint16_t addr, uint16_t value)
{
  return ctx->WriteReg(ctx->Address, addr, (uint8_t *)&value, 2);
}

static int CMW_VD55G0_WriteArray(CMW_VD55G0_t *ctx, uint16_t addr, const uint8_t *data, uint32_t data_len)
{
  const uint16_t chunk_size = 128U;
  uint16_t chunk;
  int ret;

  while (data_len > 0U)
  {
    chunk = (data_len > chunk_size) ? chunk_size : (uint16_t)data_len;
    ret = ctx->WriteReg(ctx->Address, addr, (uint8_t *)data, chunk);
    if (ret != 0)
    {
      return ret;
    }
    addr = (uint16_t)(addr + chunk);
    data += chunk;
    data_len -= chunk;
  }

  return CMW_ERROR_NONE;
}

static uint32_t CMW_VD55G0_GetTick(CMW_VD55G0_t *ctx)
{
  if (ctx->GetTick != NULL)
  {
    return (uint32_t)ctx->GetTick();
  }
  return HAL_GetTick();
}

static int CMW_VD55G0_WaitReg8(CMW_VD55G0_t *ctx, uint16_t reg, uint8_t expected)
{
  uint8_t val = 0;
  uint32_t start = CMW_VD55G0_GetTick(ctx);
  int ret;

  do
  {
    ret = CMW_VD55G0_Read8(ctx, reg, &val);
    if ((ret == 0) && (val == expected))
    {
      return CMW_ERROR_NONE;
    }
  } while ((CMW_VD55G0_GetTick(ctx) - start) < VD55G0_WAIT_TIMEOUT_MS);

  return CMW_ERROR_COMPONENT_FAILURE;
}

static int CMW_VD55G0_ComputeAndSetFrameLength(CMW_VD55G0_t *ctx, float frame_rate)
{
  float line_length_ratio;
  float line_time;
  float frame_length;
  uint16_t read_offset;
  uint16_t frame_length_offset;
  uint16_t minimum_frame_length;

  if ((frame_rate <= 0.0f) || (frame_rate > VD55G0_MAX_FRAME_RATE))
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  line_length_ratio = (float)VD55G0_MIN_LINE_LENGTH / (float)ctx->LineLength;
  read_offset = (uint16_t)ceilf(25.0f * line_length_ratio);
  frame_length_offset = (uint16_t)(31U + read_offset + (uint16_t)ceilf(20.0f * line_length_ratio));
  minimum_frame_length = (uint16_t)(VD55G0_V_SIZE_MAX + frame_length_offset);

  line_time = (float)ctx->LineLength / VD55G0_PIXEL_CLOCK;
  frame_length = ceilf(1.0f / (line_time * frame_rate));

  if ((frame_length < (float)minimum_frame_length) || (frame_length > VD55G0_MAX_FRAME_LENGTH))
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  return CMW_VD55G0_Write16(ctx, VD55G0_FRAME_LENGTH_ADD(ctx->CurrentContext), (uint16_t)frame_length);
}

static int32_t CMW_VD55G0_GetSensorInfo(void *io_ctx, ISP_SensorInfoTypeDef *info)
{
  CMW_VD55G0_t *ctx = (CMW_VD55G0_t *)io_ctx;
  uint16_t exposure_min = VD55G0_MIN_EXPOSURE;
  uint16_t exposure_max = VD55G0_MAX_EXPOSURE;
  uint32_t again_max_mdB;
  uint32_t dgain_max_mdB;

  if ((ctx == NULL) || (info == NULL))
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  if (sizeof(info->name) < (strlen(VD55G0_NAME) + 1U))
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  strcpy(info->name, VD55G0_NAME);
  info->bayer_pattern = ISP_DEMOS_TYPE_MONO;
  info->color_depth = ctx->PixelDepth;
  info->width = ctx->CurrentWidth;
  info->height = ctx->CurrentHeight;

  (void)CMW_VD55G0_Read16(ctx, VD55G0_MIN_EXPOSURE_IN_CURRENT_CONFIG_ADD, &exposure_min);
  (void)CMW_VD55G0_Read16(ctx, VD55G0_MAX_EXPOSURE_IN_CURRENT_CONFIG_ADD, &exposure_max);

  again_max_mdB = (uint32_t)(LINEAR_TO_MDECIBEL(32.0 / (32.0 - (double)VD55G0_ANALOG_GAIN_MAX)) + 0.5);
  dgain_max_mdB = (uint32_t)(LINEAR_TO_MDECIBEL(FP88_TO_FLOAT(VD55G0_DIGITAL_GAIN_MAX)) + 0.5);

  info->gain_min = 0;
  info->again_max = again_max_mdB;
  info->gain_max = (int32_t)(again_max_mdB + dgain_max_mdB);
  info->exposure_min = exposure_min;
  info->exposure_max = exposure_max;

  return CMW_ERROR_NONE;
}

static int32_t CMW_VD55G0_Init(void *io_ctx, CMW_Sensor_Init_t *initSensor)
{
  CMW_VD55G0_t *ctx = (CMW_VD55G0_t *)io_ctx;
  CMW_VD55G0_config_t *sensor_config;
  uint8_t pixel_format;
  uint8_t current_context = 0;
  int ret;

  if ((ctx == NULL) || (initSensor == NULL))
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  sensor_config = (CMW_VD55G0_config_t *)initSensor->sensor_config;
  if (sensor_config == NULL)
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  if (ctx->IsInitialized != 0U)
  {
    return CMW_ERROR_NONE;
  }

  if ((initSensor->width > VD55G0_MAX_WIDTH) || (initSensor->height > VD55G0_MAX_HEIGHT))
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  if ((initSensor->width == 0U) || (initSensor->height == 0U))
  {
    initSensor->width = VD55G0_MAX_WIDTH;
    initSensor->height = VD55G0_MAX_HEIGHT;
  }

  switch (sensor_config->pixel_format)
  {
    case CMW_PIXEL_FORMAT_RAW8:
      pixel_format = VD55G0_RAW8;
      ctx->PixelDepth = 8U;
      break;
    case CMW_PIXEL_FORMAT_DEFAULT:
    case CMW_PIXEL_FORMAT_RAW10:
      pixel_format = VD55G0_RAW10;
      ctx->PixelDepth = 10U;
      break;
    default:
      return CMW_ERROR_WRONG_PARAM;
  }

  ctx->LineLength = VD55G0_DEFAULT_LINE_LENGTH;
  ctx->CurrentWidth = initSensor->width;
  ctx->CurrentHeight = initSensor->height;

  ret = CMW_VD55G0_WaitReg8(ctx, VD55G0_SYSTEM_FSM_ADD, VD55G0_STATE_READY_TO_BOOT);
  if (ret != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  ret = CMW_VD55G0_WriteArray(ctx, VD55G0_PATCH_START_ADDR, vd55g0_patch, vd55g0_patch_len);
  if (ret != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  ret = CMW_VD55G0_Write8(ctx, VD55G0_BOOT_REG_ADD, VD55G0_CMD_PATCH_SETUP);
  ret |= CMW_VD55G0_WaitReg8(ctx, VD55G0_BOOT_REG_ADD, VD55G0_CMD_ACK);
  ret |= CMW_VD55G0_Write8(ctx, VD55G0_BOOT_REG_ADD, VD55G0_CMD_BOOT);
  ret |= CMW_VD55G0_WaitReg8(ctx, VD55G0_BOOT_REG_ADD, VD55G0_CMD_ACK);
  ret |= CMW_VD55G0_WaitReg8(ctx, VD55G0_SYSTEM_FSM_ADD, VD55G0_STATE_SW_STDBY);
  if (ret != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  ret = CMW_VD55G0_Read8(ctx, VD55G0_CURRENT_CONTEXT_ADD, &current_context);
  if (ret != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  ctx->CurrentContext = current_context;

  ret = CMW_VD55G0_Write8(ctx, VD55G0_EXP_MODE_ADD(ctx->CurrentContext), VD55G0_AE_MODE_MANUAL);
  ret |= CMW_VD55G0_Write8(ctx, VD55G0_FORMAT_CTRL_ADD, pixel_format);
  ret |= CMW_VD55G0_Write16(ctx, VD55G0_EXT_CLOCK_ADD, (uint16_t)(CAMERA_VD55G0_FREQ_IN_HZ / 1000000U));
  ret |= CMW_VD55G0_Write16(ctx, VD55G0_LINE_LENGTH_ADD, ctx->LineLength);
  ret |= CMW_VD55G0_ComputeAndSetFrameLength(ctx, (float)initSensor->fps);
  ret |= CMW_VD55G0_Write16(ctx, VD55G0_MANUAL_COARSE_EXPOSURE_ADD(ctx->CurrentContext), VD55G0_MIN_EXPOSURE);
  ret |= CMW_VD55G0_Write8(ctx, VD55G0_MANUAL_ANALOG_GAIN_ADD(ctx->CurrentContext), VD55G0_ANALOG_GAIN_MIN);
  ret |= CMW_VD55G0_Write16(ctx, VD55G0_MANUAL_DIGITAL_GAIN_ADD(ctx->CurrentContext), VD55G0_DIGITAL_GAIN_MIN);
  ret |= CMW_VD55G0_Write8(ctx, VD55G0_GPIO_0_CTRL_ADD(ctx->CurrentContext), VD55G0_STROBE_MODE);
  ret |= CMW_VD55G0_Write8(ctx, VD55G0_STROBE_START_DELAY(ctx->CurrentContext), 0x80U);
  if (ret != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  ctx->IsInitialized = 1U;
  return CMW_ERROR_NONE;
}

static int32_t CMW_VD55G0_Start(void *io_ctx)
{
  CMW_VD55G0_t *ctx = (CMW_VD55G0_t *)io_ctx;
  int ret;

  if ((ctx == NULL) || (ctx->IsInitialized == 0U))
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  ret = CMW_VD55G0_WaitReg8(ctx, VD55G0_SYSTEM_FSM_ADD, VD55G0_STATE_SW_STDBY);
  ret |= CMW_VD55G0_Write8(ctx, VD55G0_SW_STANDBY_REG, VD55G0_START_STREAM);
  ret |= CMW_VD55G0_WaitReg8(ctx, VD55G0_SW_STANDBY_REG, VD55G0_CMD_ACK);
  ret |= CMW_VD55G0_WaitReg8(ctx, VD55G0_SYSTEM_FSM_ADD, VD55G0_STATE_STREAM_ON);

  return (ret == CMW_ERROR_NONE) ? CMW_ERROR_NONE : CMW_ERROR_COMPONENT_FAILURE;
}

static int32_t CMW_VD55G0_Stop(void *io_ctx)
{
  CMW_VD55G0_t *ctx = (CMW_VD55G0_t *)io_ctx;
  uint8_t current_state = 0;
  int ret;

  if (ctx == NULL)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  ret = CMW_VD55G0_Read8(ctx, VD55G0_SYSTEM_FSM_ADD, &current_state);
  if (ret != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  if (current_state == VD55G0_STATE_STREAM_ON)
  {
    ret = CMW_VD55G0_Write8(ctx, VD55G0_STREAMING_REG, VD55G0_STOP_STREAM);
    ret |= CMW_VD55G0_WaitReg8(ctx, VD55G0_STREAMING_REG, VD55G0_CMD_ACK);
    ret |= CMW_VD55G0_WaitReg8(ctx, VD55G0_SYSTEM_FSM_ADD, VD55G0_STATE_SW_STDBY);
  }

  if (current_state == VD55G0_STATE_ERROR)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  return (ret == CMW_ERROR_NONE) ? CMW_ERROR_NONE : CMW_ERROR_COMPONENT_FAILURE;
}

static int32_t CMW_VD55G0_DeInit(void *io_ctx)
{
  CMW_VD55G0_t *ctx = (CMW_VD55G0_t *)io_ctx;

  if (ctx == NULL)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  (void)CMW_VD55G0_Stop(ctx);
  ctx->ShutdownPin(0);
  ctx->IsInitialized = 0U;

  return CMW_ERROR_NONE;
}

static int32_t CMW_VD55G0_SetGain(void *io_ctx, int32_t gain)
{
  CMW_VD55G0_t *ctx = (CMW_VD55G0_t *)io_ctx;
  uint32_t again_max_mdB;
  uint32_t dgain_max_mdB;
  double analog_linear_gain;
  double digital_linear_gain;
  int analog_reg;
  uint16_t digital_reg;
  int ret;

  again_max_mdB = (uint32_t)(LINEAR_TO_MDECIBEL(32.0 / (32.0 - (double)VD55G0_ANALOG_GAIN_MAX)) + 0.5);
  dgain_max_mdB = (uint32_t)(LINEAR_TO_MDECIBEL(FP88_TO_FLOAT(VD55G0_DIGITAL_GAIN_MAX)) + 0.5);

  if ((ctx == NULL) || (gain < 0) || (gain > (int32_t)(again_max_mdB + dgain_max_mdB)))
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  if (gain <= (int32_t)again_max_mdB)
  {
    analog_linear_gain = MDECIBEL_TO_LINEAR((double)gain);
    digital_linear_gain = 1.0;
  }
  else
  {
    analog_linear_gain = MDECIBEL_TO_LINEAR((double)again_max_mdB);
    digital_linear_gain = MDECIBEL_TO_LINEAR((double)(gain - (int32_t)again_max_mdB));
  }

  if (analog_linear_gain < 1.0)
  {
    analog_linear_gain = 1.0;
  }

  analog_reg = (int)(32.0 - (32.0 / analog_linear_gain) + 0.5);
  if (analog_reg < VD55G0_ANALOG_GAIN_MIN)
  {
    analog_reg = VD55G0_ANALOG_GAIN_MIN;
  }
  if (analog_reg > VD55G0_ANALOG_GAIN_MAX)
  {
    analog_reg = VD55G0_ANALOG_GAIN_MAX;
  }

  digital_reg = FLOAT_TO_FP88(digital_linear_gain);
  if (digital_reg < VD55G0_DIGITAL_GAIN_MIN)
  {
    digital_reg = VD55G0_DIGITAL_GAIN_MIN;
  }
  if (digital_reg > VD55G0_DIGITAL_GAIN_MAX)
  {
    digital_reg = VD55G0_DIGITAL_GAIN_MAX;
  }

  ret = CMW_VD55G0_Write8(ctx, VD55G0_MANUAL_ANALOG_GAIN_ADD(ctx->CurrentContext), (uint8_t)analog_reg);
  ret |= CMW_VD55G0_Write16(ctx, VD55G0_MANUAL_DIGITAL_GAIN_ADD(ctx->CurrentContext), digital_reg);

  return (ret == CMW_ERROR_NONE) ? CMW_ERROR_NONE : CMW_ERROR_COMPONENT_FAILURE;
}

static int32_t CMW_VD55G0_SetExposure(void *io_ctx, int32_t exposure)
{
  CMW_VD55G0_t *ctx = (CMW_VD55G0_t *)io_ctx;

  if ((ctx == NULL) || (exposure < (int32_t)VD55G0_MIN_EXPOSURE) || (exposure > (int32_t)VD55G0_MAX_EXPOSURE))
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  if (CMW_VD55G0_Write16(ctx, VD55G0_MANUAL_COARSE_EXPOSURE_ADD(ctx->CurrentContext), (uint16_t)exposure) != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  return CMW_ERROR_NONE;
}

static int32_t CMW_VD55G0_SetExposureMode(void *io_ctx, int32_t mode)
{
  CMW_VD55G0_t *ctx = (CMW_VD55G0_t *)io_ctx;
  uint8_t sensor_mode;

  if (ctx == NULL)
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  switch (mode)
  {
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

  if (CMW_VD55G0_Write8(ctx, VD55G0_EXP_MODE_ADD(ctx->CurrentContext), sensor_mode) != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  return CMW_ERROR_NONE;
}

static int32_t CMW_VD55G0_SetTestPattern(void *io_ctx, int32_t mode)
{
  CMW_VD55G0_t *ctx = (CMW_VD55G0_t *)io_ctx;
  uint16_t reg_value = 0;

  if (ctx == NULL)
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  switch (mode)
  {
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

  if (CMW_VD55G0_Write16(ctx, VD55G0_PATGEN_CTRL, reg_value) != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  return CMW_ERROR_NONE;
}

static int32_t CMW_VD55G0_GetDefaultPHYBitrate(void *io_ctx, int32_t *bitrate)
{
  UNUSED(io_ctx);
  if (bitrate == NULL)
  {
    return CMW_ERROR_WRONG_PARAM;
  }
  *bitrate = (int32_t)VD55G0_DEFAULT_DATARATE;
  return CMW_ERROR_NONE;
}

static int32_t VD55G0_ReadID(CMW_VD55G0_t *io_ctx, uint32_t *id)
{
  if ((io_ctx == NULL) || (id == NULL))
  {
    return CMW_ERROR_WRONG_PARAM;
  }

  return CMW_VD55G0_Read32(io_ctx, VD55G0_DEVICE_MODEL_ID_ADD, id);
}

static void CMW_VD55G0_PowerOn(CMW_VD55G0_t *io_ctx)
{
  io_ctx->ShutdownPin(0);
  io_ctx->Delay(200);
  io_ctx->ShutdownPin(1);
  io_ctx->Delay(20);
}

void CMW_VD55G0_SetDefaultSensorValues(CMW_VD55G0_config_t *vd55g0_config)
{
  assert(vd55g0_config != NULL);
  vd55g0_config->pixel_format = CMW_PIXEL_FORMAT_RAW10;
  vd55g0_config->CSI_PHYBitrate = VD55G0_DEFAULT_DATARATE;
}

int CMW_VD55G0_Probe(CMW_VD55G0_t *io_ctx, CMW_Sensor_if_t *vd55g0_if)
{
  uint32_t id = 0;
  int ret;

  if ((io_ctx == NULL) || (vd55g0_if == NULL) || (io_ctx->Init == NULL) || (io_ctx->ReadReg == NULL) || (io_ctx->WriteReg == NULL))
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  CMW_VD55G0_PowerOn(io_ctx);

  ret = io_ctx->Init();
  if (ret != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  ret = VD55G0_ReadID(io_ctx, &id);
  if (ret != CMW_ERROR_NONE)
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  if ((id == 0UL) || (id == 0xFFFFFFFFUL))
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

  if ((id != VD55G0_CHIP_ID) && (id != 0x53354731UL))
  {
    return CMW_ERROR_COMPONENT_FAILURE;
  }

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

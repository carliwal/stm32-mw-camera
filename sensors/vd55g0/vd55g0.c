/**
  ******************************************************************************
  * @file    vd55g0.c
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

#include "vd55g0.h"
#include <assert.h>
#include <math.h>
#include <string.h>

/* Register Definitions */
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

#define VD55G0_DEFAULT_LINE_LENGTH                1200U
#define VD55G0_MAX_FRAME_LENGTH                   65535U
#define VD55G0_MAX_FRAME_RATE                     152.0f
#define VD55G0_PIXEL_CLOCK                        124000000.0f
#define VD55G0_V_SIZE_MAX                         604.0f

#define MDECIBEL_TO_LINEAR(mdB)                   (pow(10.0, ((double)(mdB) / 1000.0) / 20.0))
#define LINEAR_TO_MDECIBEL(linearValue)           (1000.0 * (20.0 * log10(linearValue)))
#define FP88_TO_FLOAT(fp)                         (((fp) >> 8) + (((fp) & 0xFFU) / 256.0))
#define FLOAT_TO_FP88(x)                          ((((uint16_t)(x)) << 8) | ((uint16_t)(((x) - (uint16_t)(x)) * 256.0f) & 0xFFU))

#define VD55G0_WAIT_TIMEOUT_MS                    5000U

extern const uint8_t vd55g0_patch[];
extern const uint32_t vd55g0_patch_len;

/* Private function prototypes -----------------------------------------------*/
static int VD55G0_Read8(VD55G0_Ctx *ctx, uint16_t addr, uint8_t *value);
static int VD55G0_Read16(VD55G0_Ctx *ctx, uint16_t addr, uint16_t *value);
static int VD55G0_Read32(VD55G0_Ctx *ctx, uint16_t addr, uint32_t *value);
static int VD55G0_Write8(VD55G0_Ctx *ctx, uint16_t addr, uint8_t value);
static int VD55G0_Write16(VD55G0_Ctx *ctx, uint16_t addr, uint16_t value);
static int VD55G0_WriteArray(VD55G0_Ctx *ctx, uint16_t addr, const uint8_t *data, uint32_t data_len);
static int VD55G0_WaitReg8(VD55G0_Ctx *ctx, uint16_t reg, uint8_t expected);
static int VD55G0_ComputeAndSetFrameLength(VD55G0_Ctx *ctx, float frame_rate);

/* Public Functions ----------------------------------------------------------*/

/**
  * @brief  Read 8-bit register
  */
static int VD55G0_Read8(VD55G0_Ctx *ctx, uint16_t addr, uint8_t *value)
{
  return ctx->ReadReg(ctx->Address, addr, value, 1);
}

/**
  * @brief  Read 16-bit register (little-endian)
  */
static int VD55G0_Read16(VD55G0_Ctx *ctx, uint16_t addr, uint16_t *value)
{
  uint8_t data[2];
  int ret;

  ret = ctx->ReadReg(ctx->Address, addr, data, 2);
  if (ret != 0)
    return ret;

  *value = (data[1] << 8) | data[0];
  return 0;
}

/**
  * @brief  Read 32-bit register (little-endian)
  */
static int VD55G0_Read32(VD55G0_Ctx *ctx, uint16_t addr, uint32_t *value)
{
  uint8_t data[4];
  int ret;

  ret = ctx->ReadReg(ctx->Address, addr, data, 4);
  if (ret != 0)
    return ret;

  *value = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
  return 0;
}

/**
  * @brief  Write 8-bit register
  */
static int VD55G0_Write8(VD55G0_Ctx *ctx, uint16_t addr, uint8_t value)
{
  return ctx->WriteReg(ctx->Address, addr, &value, 1);
}

/**
  * @brief  Write 16-bit register
  */
static int VD55G0_Write16(VD55G0_Ctx *ctx, uint16_t addr, uint16_t value)
{
  uint8_t data[2];
  data[0] = value & 0xFF;
  data[1] = (value >> 8) & 0xFF;
  return ctx->WriteReg(ctx->Address, addr, data, 2);
}

/**
  * @brief  Write array data (chunked I2C transfers)
  */
static int VD55G0_WriteArray(VD55G0_Ctx *ctx, uint16_t addr, const uint8_t *data, uint32_t data_len)
{
  const uint32_t chunk_size = 128;
  uint32_t remaining = data_len;
  uint16_t current_addr = addr;
  int ret;

  while (remaining > 0) {
    uint32_t chunk = (remaining > chunk_size) ? chunk_size : remaining;
    ret = ctx->WriteReg(ctx->Address, current_addr, (uint8_t *)data, chunk);
    if (ret != 0)
      return ret;

    remaining -= chunk;
    current_addr += chunk;
    data += chunk;
  }

  return 0;
}

/**
  * @brief  Wait for register to reach expected value with timeout
  */
static int VD55G0_WaitReg8(VD55G0_Ctx *ctx, uint16_t reg, uint8_t expected)
{
  uint8_t value = 0;
  uint32_t timeout = ctx->GetTick() + VD55G0_WAIT_TIMEOUT_MS;
  int ret;

  while (ctx->GetTick() < timeout) {
    ret = VD55G0_Read8(ctx, reg, &value);
    if (ret != 0)
      return ret;

    if (value == expected)
      return 0;

    ctx->Delay(10);
  }

  return -1; /* Timeout */
}

/**
  * @brief  Compute and set frame length based on desired frame rate
  */
static int VD55G0_ComputeAndSetFrameLength(VD55G0_Ctx *ctx, float frame_rate)
{
  uint16_t frame_length;
  float pixel_rate = VD55G0_PIXEL_CLOCK / (float)ctx->LineLength;
  float lines_per_frame = pixel_rate / frame_rate;

  if (lines_per_frame > VD55G0_MAX_FRAME_LENGTH)
    lines_per_frame = VD55G0_MAX_FRAME_LENGTH;

  frame_length = (uint16_t)lines_per_frame;

  return VD55G0_Write16(ctx, VD55G0_FRAME_LENGTH_ADD(ctx->CurrentContext), frame_length);
}

/**
  * @brief  Get sensor information (exposure and gain ranges)
  */
int VD55G0_GetSensorInfo(VD55G0_Ctx *ctx, uint32_t *out_min_exp, uint32_t *out_max_exp,
                         uint32_t *out_again_max, uint32_t *out_dgain_max)
{
  uint32_t again_max_mdB, dgain_max_mdB;

  if ((ctx == NULL) || (out_min_exp == NULL) || (out_max_exp == NULL))
    return -1;

  /* Read min/max exposure from sensor */
  VD55G0_Read16(ctx, VD55G0_MIN_EXPOSURE_IN_CURRENT_CONFIG_ADD, (uint16_t *)out_min_exp);
  VD55G0_Read16(ctx, VD55G0_MAX_EXPOSURE_IN_CURRENT_CONFIG_ADD, (uint16_t *)out_max_exp);

  /* Compute analog and digital gain max in millidecibels */
  again_max_mdB = (uint32_t)(LINEAR_TO_MDECIBEL(32.0 / (32.0 - (double)VD55G0_ANALOG_GAIN_MAX)) + 0.5);
  dgain_max_mdB = (uint32_t)(LINEAR_TO_MDECIBEL(FP88_TO_FLOAT(VD55G0_DIGITAL_GAIN_MAX)) + 0.5);

  if (out_again_max != NULL)
    *out_again_max = again_max_mdB;
  if (out_dgain_max != NULL)
    *out_dgain_max = dgain_max_mdB;

  return 0;
}

/**
  * @brief  Initialize VD55G0 sensor
  */
int VD55G0_Init(VD55G0_Ctx *ctx, uint32_t width, uint32_t height, uint32_t fps)
{
  uint8_t pixel_format = VD55G0_RAW10;
  uint8_t current_context = 0;
  int ret;

  if ((ctx == NULL) || (width == 0) || (height == 0))
    return -1;

  if ((width > VD55G0_MAX_WIDTH) || (height > VD55G0_MAX_HEIGHT))
    return -1;

  if (ctx->IsInitialized != 0U)
    return 0;

  ctx->LineLength = VD55G0_DEFAULT_LINE_LENGTH;
  ctx->CurrentWidth = width;
  ctx->CurrentHeight = height;

  /* Wait for sensor to reach READY_TO_BOOT state */
  ret = VD55G0_WaitReg8(ctx, VD55G0_SYSTEM_FSM_ADD, VD55G0_STATE_READY_TO_BOOT);
  if (ret != 0)
    return ret;

  /* Upload firmware patch */
  ret = VD55G0_WriteArray(ctx, VD55G0_PATCH_START_ADDR, vd55g0_patch, vd55g0_patch_len);
  if (ret != 0)
    return ret;

  /* Boot sensor with patch */
  ret = VD55G0_Write8(ctx, VD55G0_BOOT_REG_ADD, VD55G0_CMD_PATCH_SETUP);
  ret |= VD55G0_WaitReg8(ctx, VD55G0_BOOT_REG_ADD, VD55G0_CMD_ACK);
  ret |= VD55G0_Write8(ctx, VD55G0_BOOT_REG_ADD, VD55G0_CMD_BOOT);
  ret |= VD55G0_WaitReg8(ctx, VD55G0_BOOT_REG_ADD, VD55G0_CMD_ACK);
  ret |= VD55G0_WaitReg8(ctx, VD55G0_SYSTEM_FSM_ADD, VD55G0_STATE_SW_STDBY);
  if (ret != 0)
    return ret;

  /* Get current context */
  ret = VD55G0_Read8(ctx, VD55G0_CURRENT_CONTEXT_ADD, &current_context);
  if (ret != 0)
    return ret;

  ctx->CurrentContext = current_context;

  /* Configure sensor */
  ret = VD55G0_Write8(ctx, VD55G0_EXP_MODE_ADD(ctx->CurrentContext), VD55G0_AE_MODE_MANUAL);
  ret |= VD55G0_Write8(ctx, VD55G0_FORMAT_CTRL_ADD, pixel_format);
  ret |= VD55G0_Write16(ctx, VD55G0_EXT_CLOCK_ADD, 12); /* 12 MHz */
  ret |= VD55G0_Write16(ctx, VD55G0_LINE_LENGTH_ADD, ctx->LineLength);
  ret |= VD55G0_ComputeAndSetFrameLength(ctx, (float)fps);
  ret |= VD55G0_Write16(ctx, VD55G0_MANUAL_COARSE_EXPOSURE_ADD(ctx->CurrentContext), VD55G0_MIN_EXPOSURE);
  ret |= VD55G0_Write8(ctx, VD55G0_MANUAL_ANALOG_GAIN_ADD(ctx->CurrentContext), VD55G0_ANALOG_GAIN_MIN);
  ret |= VD55G0_Write16(ctx, VD55G0_MANUAL_DIGITAL_GAIN_ADD(ctx->CurrentContext), VD55G0_DIGITAL_GAIN_MIN);
  ret |= VD55G0_Write8(ctx, VD55G0_GPIO_0_CTRL_ADD(ctx->CurrentContext), VD55G0_STROBE_MODE);
  ret |= VD55G0_Write8(ctx, VD55G0_STROBE_START_DELAY(ctx->CurrentContext), 0x80U);
  if (ret != 0)
    return ret;

  ctx->IsInitialized = 1U;
  return 0;
}

/**
  * @brief  De-initialize VD55G0 sensor
  */
int VD55G0_DeInit(VD55G0_Ctx *ctx)
{
  if (ctx == NULL)
    return -1;

  VD55G0_Stop(ctx);
  ctx->IsInitialized = 0U;

  return 0;
}

/**
  * @brief  Start streaming
  */
int VD55G0_Start(VD55G0_Ctx *ctx)
{
  int ret;

  if ((ctx == NULL) || (ctx->IsInitialized == 0U))
    return -1;

  ret = VD55G0_WaitReg8(ctx, VD55G0_SYSTEM_FSM_ADD, VD55G0_STATE_SW_STDBY);
  ret |= VD55G0_Write8(ctx, VD55G0_SW_STANDBY_REG, VD55G0_START_STREAM);
  ret |= VD55G0_WaitReg8(ctx, VD55G0_SW_STANDBY_REG, VD55G0_CMD_ACK);
  ret |= VD55G0_WaitReg8(ctx, VD55G0_SYSTEM_FSM_ADD, VD55G0_STATE_STREAM_ON);

  return ret;
}

/**
  * @brief  Stop streaming
  */
int VD55G0_Stop(VD55G0_Ctx *ctx)
{
  uint8_t current_state = 0;
  int ret;

  if (ctx == NULL)
    return -1;

  ret = VD55G0_Read8(ctx, VD55G0_SYSTEM_FSM_ADD, &current_state);
  if (ret != 0)
    return ret;

  if (current_state == VD55G0_STATE_STREAM_ON) {
    ret = VD55G0_Write8(ctx, VD55G0_STREAMING_REG, VD55G0_STOP_STREAM);
    ret |= VD55G0_WaitReg8(ctx, VD55G0_SYSTEM_FSM_ADD, VD55G0_STATE_SW_STDBY);
    return ret;
  }

  if (current_state == VD55G0_STATE_ERROR)
    return -1;

  return 0;
}

/**
  * @brief  Set gain (analog + digital, converted from millidecibels)
  */
int VD55G0_SetGain(VD55G0_Ctx *ctx, int32_t gain)
{
  double linear_gain = MDECIBEL_TO_LINEAR(gain);
  double analog_linear = 0, digital_linear = 1.0;
  double max_analog_linear = 32.0 / (32.0 - (double)VD55G0_ANALOG_GAIN_MAX);
  uint8_t analog_reg;
  uint16_t digital_reg;
  int ret;

  if ((ctx == NULL) || (ctx->IsInitialized == 0U))
    return -1;

  /* Split into analog and digital gain */
  if (linear_gain <= max_analog_linear) {
    analog_linear = linear_gain;
  } else {
    analog_linear = max_analog_linear;
    digital_linear = linear_gain / max_analog_linear;
  }

  /* Clamp values */
  if (analog_linear < 1.0)
    analog_linear = 1.0;
  if (digital_linear < 1.0)
    digital_linear = 1.0;

  /* Convert analog gain: gain = 32 / (32 - reg) -> reg = 32 - 32/gain */
  analog_reg = (uint8_t)(32.0 - (32.0 / analog_linear) + 0.5);
  if (analog_reg > VD55G0_ANALOG_GAIN_MAX)
    analog_reg = VD55G0_ANALOG_GAIN_MAX;

  /* Convert digital gain to FP88 format */
  digital_reg = FLOAT_TO_FP88((float)digital_linear);

  ret = VD55G0_Write8(ctx, VD55G0_MANUAL_ANALOG_GAIN_ADD(ctx->CurrentContext), analog_reg);
  ret |= VD55G0_Write16(ctx, VD55G0_MANUAL_DIGITAL_GAIN_ADD(ctx->CurrentContext), digital_reg);

  return ret;
}

/**
  * @brief  Set exposure time (coarse integration lines)
  */
int VD55G0_SetExposure(VD55G0_Ctx *ctx, int32_t exposure)
{
  if ((ctx == NULL) || (ctx->IsInitialized == 0U))
    return -1;

  if (exposure < VD55G0_MIN_EXPOSURE)
    exposure = VD55G0_MIN_EXPOSURE;
  if (exposure > VD55G0_MAX_EXPOSURE)
    exposure = VD55G0_MAX_EXPOSURE;

  return VD55G0_Write16(ctx, VD55G0_MANUAL_COARSE_EXPOSURE_ADD(ctx->CurrentContext), (uint16_t)exposure);
}

/**
  * @brief  Set exposure mode
  */
int VD55G0_SetExposureMode(VD55G0_Ctx *ctx, uint8_t mode)
{
  if ((ctx == NULL) || (ctx->IsInitialized == 0U))
    return -1;

  return VD55G0_Write8(ctx, VD55G0_EXP_MODE_ADD(ctx->CurrentContext), mode);
}

/**
  * @brief  Set test pattern
  */
int VD55G0_SetTestPattern(VD55G0_Ctx *ctx, uint16_t pattern)
{
  if ((ctx == NULL) || (ctx->IsInitialized == 0U))
    return -1;

  return VD55G0_Write16(ctx, VD55G0_PATGEN_CTRL, pattern);
}

/**
  * @brief  Set pixel format
  */
int VD55G0_SetPixelFormat(VD55G0_Ctx *ctx, uint8_t format)
{
  if ((ctx == NULL) || (ctx->IsInitialized == 0U))
    return -1;

  if ((format != VD55G0_RAW8) && (format != VD55G0_RAW10))
    return -1;

  ctx->PixelDepth = (format == VD55G0_RAW10) ? 10 : 8;

  return VD55G0_Write8(ctx, VD55G0_FORMAT_CTRL_ADD, format);
}

/**
  * @brief  Get default PHY bitrate
  */
uint32_t VD55G0_GetDefaultPHYBitrate(void)
{
  return 804000000U; /* 804 Mbps */
}

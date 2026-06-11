/**
  ******************************************************************************
  * @file    vd55g0.c
  * @author  MDG Application Team
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include <stdlib.h>

/* STATUS registers (RO) - group base 0x0000 */
#define VD55G0_REG_MODEL_ID                           0x0000
  #define VD55G0_MODEL_ID_VD55G0                      0x53354730
#define VD55G0_REG_DEVICE_REVISION                    0x0004
#define VD55G0_REG_ROM_REVISION                       0x0018
#define VD55G0_REG_OPTICAL_REVISION                   0x001E
  #define VD55G0_OPTICAL_REV_MONO                     0x00
#define VD55G0_REG_FWPATCH_REVISION                   0x0022
#define VD55G0_REG_SYSTEM_FSM                         0x002C
  #define VD55G0_SYSTEM_FSM_HW_STBY                   0x00
  #define VD55G0_SYSTEM_FSM_READY_TO_BOOT             0x01
  #define VD55G0_SYSTEM_FSM_SW_STBY                   0x02
  #define VD55G0_SYSTEM_FSM_STREAMING                 0x03
  #define VD55G0_SYSTEM_FSM_ERROR                     0xFF
#define VD55G0_REG_PIXEL_CLK                          0x0040

/* COMMAND registers (R/W) - group base 0x0200 */
#define VD55G0_REG_BOOT                               0x0200
  #define VD55G0_CMD_ACK                              0
  #define VD55G0_BOOT_BOOT                            1
  #define VD55G0_BOOT_PATCH_SETUP                     2
#define VD55G0_REG_SW_STANDBY                         0x0201
  #define VD55G0_STBY_START_STREAM                    1
#define VD55G0_REG_STREAMING                          0x0202
  #define VD55G0_STREAMING_STOP_STREAM                1

/* SENSOR SETTINGS registers (R/W) - group base 0x0220 */
#define VD55G0_REG_EXT_CLOCK                          0x0220
#define VD55G0_REG_CLK_PLL_MIPI                       0x0224

/* STATIC registers (R/W) - group base 0x0300 */
#define VD55G0_REG_LINE_LENGTH                        0x0300
#define VD55G0_REG_ORIENTATION                        0x0302
#define VD55G0_REG_VT_CTRL                            0x0309
#define VD55G0_REG_FORMAT_CTRL                        0x030A
#define VD55G0_REG_OIF_CTRL                           0x030C
#define VD55G0_REG_OIF_IMG_CTRL                       0x030F
  #define VD55G0_RAW8_DATA_TYPE                       0x2A
  #define VD55G0_RAW10_DATA_TYPE                      0x2B
#define VD55G0_REG_DUSTER_CTRL                        0x0316
  #define VD55G0_DUSTER_DISABLE                       0
#define VD55G0_REG_DARKCAL_CTRL                       0x032C
  #define VD55G0_DARKCAL_BYPASS                       0
  #define VD55G0_DARKCAL_BYPASS_DARKAVG               2

/* DYNAMIC registers (R/W) - group base 0x0400 */
#define VD55G0_REG_PATGEN_CTRL                        0x0400
  #define VD55G0_PATGEN_CTRL_DISABLE                  0x0000
  #define VD55G0_PATGEN_CTRL_DIAG_GRAY                0x0221
  #define VD55G0_PATGEN_CTRL_PSN                      0x0281
#define VD55G0_REG_AE_COMPILER_CONTROL                0x0434
#define VD55G0_REG_AE_TARGET_PERCENTAGE               0x0440

/* CONTEXT 0 registers (R/W) - group base 0x044C */
#define VD55G0_REG_EXP_MODE                           0x044C
  #define VD55G0_EXP_MODE_AUTO                        0
  #define VD55G0_EXP_MODE_FREEZE                      1
  #define VD55G0_EXP_MODE_MANUAL                      2
#define VD55G0_REG_MANUAL_ANALOG_GAIN                 0x044D
#define VD55G0_REG_MANUAL_COARSE_EXPOSURE             0x044E
#define VD55G0_REG_MANUAL_DIGITAL_GAIN_CH0            0x0450
#define VD55G0_REG_MANUAL_DIGITAL_GAIN_CH1            0x0452
#define VD55G0_REG_MANUAL_DIGITAL_GAIN_CH2            0x0454
#define VD55G0_REG_MANUAL_DIGITAL_GAIN_CH3            0x0456
#define VD55G0_REG_FRAME_LENGTH                       0x0458
#define VD55G0_REG_Y_START                            0x045A
#define VD55G0_REG_Y_END                              0x045C
#define VD55G0_REG_OUT_ROI_X_START                    0x045E
#define VD55G0_REG_OUT_ROI_X_END                      0x0460
#define VD55G0_REG_OUT_ROI_Y_START                    0x0462
#define VD55G0_REG_OUT_ROI_Y_END                      0x0464
#define VD55G0_REG_GPIO_x(_i_)                        (0x0467 + _i_)
#define VD55G0_REG_READOUT_CTRL                       0x047A

/* MISCELLANEOUS registers (R/W) - group base 0x0900 */
#define VD55G0_REG_DPHYTX_CTRL                        0x093A
  #define DBG_CONT_MODE_DISABLED                      0x08
  #define DBG_CONT_MODE_ENABLED                       0x18
#define VD55G0_REG_MAX_DG                             0x0942
#define VD55G0_REG_MAX_AG_CODED                       0x0944
#define VD55G0_REG_MINIMUM_EXPOSURE_10BIT             0x095C
#define VD55G0_REG_MINIMUM_EXPOSURE_9BIT              0x095E
#define VD55G0_REG_MAX_EXPOSURE_LINES                 0x096C

#define VD55G0_REG_FWPATCH_START_ADDR                 0x2000

#define VD55G0_MIN_LINE_LEN_ADC_10                    1128
#define VD55G0_MIN_LINE_LEN_ADC_10_SLOW              1200
#define VD55G0_MIPI_MARGIN                            900
#define VD55G0_FRAME_LENGTH_OFFSET                    76

#define VD55G0_EXPOSURE_MIN_COARSE_LINES_ADC_10       19
#define VD55G0_EXP_COARSE_INTG_MARGIN_ADC_10          64

#define VD55G0_ANALOG_GAIN_DEFAULT                    0
#define VD55G0_DIGITAL_GAIN_DEFAULT                   0x100
#define VD55G0_EXPOSURE_COARSE_DEFAULT                50

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef CEIL
#define CEIL(num) ((num) == (int)(num) ? (int)(num) : (num) > 0 ? (int)((num) + 1) : (int)(num))
#endif

enum vd55g0_bin_mode {
  VD55G0_BIN_MODE_NORMAL,
  VD55G0_BIN_MODE_DIGITAL_X2,
  VD55G0_BIN_MODE_DIGITAL_X4,
};

enum {
  VD55G0_ST_IDLE,
  VD55G0_ST_STREAMING,
};

struct vd55g0_rect {
  int32_t left;
  int32_t top;
  uint32_t width;
  uint32_t height;
};

struct vd55g0_mode {
  uint32_t width;
  uint32_t height;
  enum vd55g0_bin_mode bin_mode;
  struct vd55g0_rect crop;
};

static const struct vd55g0_mode vd55g0_supported_modes[] = {
  {
    .width = VD55G0_MAX_WIDTH,
    .height = VD55G0_MAX_HEIGHT,
    .bin_mode = VD55G0_BIN_MODE_NORMAL,
    .crop = {
      .left = 0,
      .top = 0,
      .width = VD55G0_MAX_WIDTH,
      .height = VD55G0_MAX_HEIGHT,
    },
  },
  {
    .width = 640,
    .height = 480,
    .bin_mode = VD55G0_BIN_MODE_NORMAL,
    .crop = {
      .left = 2,
      .top = 62,
      .width = 640,
      .height = 480,
    },
  },
  {
    .width = 320,
    .height = 240,
    .bin_mode = VD55G0_BIN_MODE_DIGITAL_X2,
    .crop = {
      .left = 2,
      .top = 62,
      .width = 640,
      .height = 480,
    },
  },
};

#define VD55G0_TraceError(_ctx_,_ret_) do { \
  if (_ret_) VD55G0_error(_ctx_, "Error on %s:%d : %d\n", __func__, __LINE__, _ret_); \
  if (_ret_) display_error(_ctx_); \
  if (_ret_) return _ret_; \
} while(0)

static const struct vd55g0_mode *VD55G0_Resolution2Mode(VD55G0_Res_t resolution)
{
  switch (resolution) {
  case VD55G0_RES_QVGA_320_240:
    return &vd55g0_supported_modes[2];
    break;
  case VD55G0_RES_VGA_640_480:
    return &vd55g0_supported_modes[1];
    break;
  case VD55G0_RES_FULL_644_604:
    return &vd55g0_supported_modes[0];
    break;
  default:
    return NULL;
  }
}

static void VD55G0_log_impl(VD55G0_Ctx_t *ctx, int lvl, const char *format, ...)
{
  va_list ap;

  if (!ctx->log)
    return ;

  va_start(ap, format);
  ctx->log(ctx, lvl, format, ap);
  va_end(ap);
}

#define VD55G0_dbg(_ctx_, _lvl_, _fmt_, ...) do { \
  VD55G0_log_impl(_ctx_, VD55G0_LVL_DBG(_lvl_), "VD55G0_DG%d-%d : " _fmt_, _lvl_, __LINE__, ##__VA_ARGS__); \
} while(0)

#define VD55G0_notice(_ctx_, _fmt_, ...) do { \
  VD55G0_log_impl(_ctx_, VD55G0_LVL_NOTICE, "VD55G0_NOT-%d : " _fmt_, __LINE__, ##__VA_ARGS__); \
} while(0)

#define VD55G0_warn(_ctx_, _fmt_, ...) do { \
  VD55G0_log_impl(_ctx_, VD55G0_LVL_WARNING, "VD55G0_WRN-%d : " _fmt_, __LINE__, ##__VA_ARGS__); \
} while(0)

#define VD55G0_error(_ctx_, _fmt_, ...) do { \
  VD55G0_log_impl(_ctx_, VD55G0_LVL_ERROR, "VD55G0_ERR-%d : " _fmt_, __LINE__, ##__VA_ARGS__); \
} while(0)

static void display_error(VD55G0_Ctx_t *ctx)
{
  uint8_t reg8;
  int ret;

  ret = ctx->read8(ctx, VD55G0_REG_SYSTEM_FSM, &reg8);
  assert(ret == 0);
  VD55G0_error(ctx, "SYSTEM_FSM : 0x%02x\n", reg8);
}

static int VD55G0_PollReg8(VD55G0_Ctx_t *ctx, uint16_t addr, uint8_t poll_val)
{
  const unsigned int loop_delay_ms = 10;
  const unsigned int timeout_ms = 500;
  int loop_nb = timeout_ms / loop_delay_ms;
  uint8_t val;
  int ret;

  while (--loop_nb) {
    ret = ctx->read8(ctx, addr, &val);
    if (ret < 0)
      return ret;
    if (val == poll_val)
      return 0;
    ctx->delay(ctx, loop_delay_ms);
  }

  VD55G0_dbg(ctx, 0, "current state %d\n", val);

  return -1;
}

static int VD55G0_IsStreaming(VD55G0_Ctx_t *ctx)
{
  uint8_t state;
  int ret;

  ret = ctx->read8(ctx, VD55G0_REG_SYSTEM_FSM, &state);
  if (ret)
    return ret;

  return state == VD55G0_SYSTEM_FSM_STREAMING;
}

static int VD55G0_WaitState(VD55G0_Ctx_t *ctx, int state)
{
  int ret = VD55G0_PollReg8(ctx, VD55G0_REG_SYSTEM_FSM, state);

  if (ret)
    VD55G0_warn(ctx, "Unable to reach state %d\n", state);
  else
    VD55G0_dbg(ctx, 0, "reach state %d\n", state);

  return ret;
}

static int VD55G0_SetBayerType(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  uint16_t reg16;
  int ret;

  /* OPTICAL_REVISION reg is populated once sensor is booted  */
  ret = ctx->read16(ctx, VD55G0_REG_OPTICAL_REVISION, &reg16);
  VD55G0_TraceError(ctx, ret);

  ctx->ctx.is_mono = !(reg16 & 1);

  if (drv_ctx->is_mono) {
    ctx->bayer = VD55G0_BAYER_NONE;
    return 0;
  }

  switch (drv_ctx->config_save.flip_mirror_mode) {
  case VD55G0_MIRROR_FLIP_NONE:
    ctx->bayer = VD55G0_BAYER_RGGB;
    break;
  case VD55G0_FLIP:
    ctx->bayer = VD55G0_BAYER_GBRG;
    break;
  case VD55G0_MIRROR:
    ctx->bayer = VD55G0_BAYER_GRBG;
    break;
  case VD55G0_MIRROR_FLIP:
    ctx->bayer = VD55G0_BAYER_BGGR;
    break;
  default:
    assert(0);
  }

  return 0;
}

static int VD55G0_CheckModelId(VD55G0_Ctx_t *ctx)
{
  uint32_t reg32;
  uint16_t reg16;
  int ret;

  ret = ctx->read32(ctx, VD55G0_REG_MODEL_ID, &reg32);
  VD55G0_TraceError(ctx, ret);
  VD55G0_dbg(ctx, 0, "model_id = 0x%08x\n", reg32);
  if (reg32 != VD55G0_MODEL_ID_VD55G0) {
    VD55G0_error(ctx, "Bad model id got 0x%08x\n", reg32);
    return -1;
  }

  ret = ctx->read16(ctx, VD55G0_REG_DEVICE_REVISION, &reg16);
  VD55G0_TraceError(ctx, ret);
  VD55G0_dbg(ctx, 0, "revision = 0x%04x\n", reg16);

  ret = ctx->read32(ctx, VD55G0_REG_ROM_REVISION, &reg32);
  VD55G0_TraceError(ctx, ret);
  VD55G0_dbg(ctx, 0, "rom = 0x%08x\n", reg32);

  return 0;
}

static int VD55G0_BootMcu(VD55G0_Ctx_t *ctx)
{
  int ret;

  ret = ctx->write8(ctx, VD55G0_REG_BOOT, VD55G0_BOOT_BOOT);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_PollReg8(ctx, VD55G0_REG_BOOT, VD55G0_CMD_ACK);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_WaitState(ctx, VD55G0_SYSTEM_FSM_SW_STBY);
  VD55G0_TraceError(ctx, ret);

  VD55G0_notice(ctx, "sensor boot successfully\n");

  return 0;
}

static int VD55G0_Gpios(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int ret;
  int i;

  for (i = 0 ; i < VD55G0_GPIO_NB; i++)
  {
    ret = ctx->write8(ctx, VD55G0_REG_GPIO_x(i), drv_ctx->config_save.gpio_ctrl[i]);
    VD55G0_TraceError(ctx, ret);
  }

  return 0;
}

static int VD55G0_Boot(VD55G0_Ctx_t *ctx)
{
  int ret;

  ret = VD55G0_WaitState(ctx, VD55G0_SYSTEM_FSM_READY_TO_BOOT);
  if (ret)
    return ret;

  ret = VD55G0_CheckModelId(ctx);
  if (ret)
    return ret;

  /* No mandatory firmware patch on the CSI-2 path, boot directly */
  ret = VD55G0_BootMcu(ctx);
  if (ret)
    return ret;

  ret = VD55G0_SetBayerType(ctx);
  if (ret)
    return ret;

  ret = VD55G0_Gpios(ctx);
  if (ret)
    return ret;

  return 0;
}

static uint32_t VD55G0_GetSystemClock(VD55G0_Ctx_t *ctx)
{
  uint32_t mipi_data_rate;
  int ret;

  ret = ctx->read32(ctx, VD55G0_REG_CLK_PLL_MIPI, &mipi_data_rate);
  if (ret)
    return 0;

  if (mipi_data_rate <= 1200000000 && mipi_data_rate > 600000000)
    return mipi_data_rate;
  else if (mipi_data_rate <= 600000000 && mipi_data_rate > 300000000)
    return mipi_data_rate * 2;
  else if (mipi_data_rate <= 300000000 && mipi_data_rate >= 250000000)
    return mipi_data_rate * 4;

  return 0;
}

static uint32_t VD55G0_GetPixelClock(VD55G0_Ctx_t *ctx)
{
  uint32_t system_clk;

  system_clk = VD55G0_GetSystemClock(ctx);
  if (!system_clk)
    return 0;

  if (system_clk <= 1200000000 && system_clk > 900000000)
    return system_clk / 8;
  else if (system_clk <= 900000000 && system_clk > 780000000)
    return system_clk / 6;
  else if (system_clk <= 780000000 && system_clk >= 600000000)
    return system_clk / 5;

  return 0;
}

static int VD55G0_SetupClocks(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int ret;

  if (drv_ctx->config_save.out_itf.data_rate_in_mps < VD55G0_MIN_DATARATE ||
      drv_ctx->config_save.out_itf.data_rate_in_mps > VD55G0_MAX_DATARATE)
    return -1;

  ret = ctx->write32(ctx, VD55G0_REG_EXT_CLOCK, drv_ctx->config_save.ext_clock_freq_in_hz);
  VD55G0_TraceError(ctx, ret);

  ret = ctx->write32(ctx, VD55G0_REG_CLK_PLL_MIPI, drv_ctx->config_save.out_itf.data_rate_in_mps);
  VD55G0_TraceError(ctx, ret);

  drv_ctx->pclk = VD55G0_GetPixelClock(ctx);
  if (!drv_ctx->pclk)
    return -1;

  return 0;
}

static int VD55G0_GetLineTimeInUs(VD55G0_Ctx_t *ctx, uint32_t *line_time_in_us)
{
  uint16_t line_len;
  uint32_t pixel_clock;
  int ret;

  ret = ctx->read16(ctx, VD55G0_REG_LINE_LENGTH, &line_len);
  VD55G0_TraceError(ctx, ret);

  /* compute line_time_in_us */
  pixel_clock = VD55G0_GetPixelClock(ctx);
  if (!pixel_clock)
    return -1;

  /* Round up line time to the next integer */
  *line_time_in_us = ((uint64_t)line_len * 1000000) / pixel_clock + 1;

  return 0;
}

static int VD55G0_SetupOutput(VD55G0_Ctx_t *ctx)
{
  VD55G0_OutItf_Config_t *out_itf = &ctx->ctx.config_save.out_itf;
  uint8_t pixel_depth = ctx->ctx.config_save.pixel_depth;
  uint8_t data_type;
  uint16_t oif_ctrl;
  int ret;

  /* Be sure we got value 0 or 1 */
  out_itf->clock_lane_swap_enable = !!out_itf->clock_lane_swap_enable;
  out_itf->data_lane_swap_enable = !!out_itf->data_lane_swap_enable;

  if (pixel_depth == 10)
    data_type = VD55G0_RAW10_DATA_TYPE;
  else
    data_type = VD55G0_RAW8_DATA_TYPE;

  ret = ctx->write8(ctx, VD55G0_REG_FORMAT_CTRL, pixel_depth);
  VD55G0_TraceError(ctx, ret);

  /* csi lanes */
  oif_ctrl = out_itf->data_lane_swap_enable << 6 |
             out_itf->clock_lane_swap_enable << 3;
  ret = ctx->write16(ctx, VD55G0_REG_OIF_CTRL, oif_ctrl);
  VD55G0_TraceError(ctx, ret);

  /* data type */
  ret = ctx->write8(ctx, VD55G0_REG_OIF_IMG_CTRL, data_type);
  VD55G0_TraceError(ctx, ret);

  return 0;
}

static int VD55G0_SetupSize(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  const struct vd55g0_mode *mode;
  int ret;

  mode = VD55G0_Resolution2Mode(drv_ctx->config_save.resolution);
  if (!mode)
    return -1;

  ret = ctx->write8(ctx, VD55G0_REG_READOUT_CTRL, mode->bin_mode);
  VD55G0_TraceError(ctx, ret);

  /* VT crop window (pixel array readout) */
  ret = ctx->write16(ctx, VD55G0_REG_Y_START, mode->crop.top);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_Y_END, mode->crop.top + mode->crop.height - 1);
  VD55G0_TraceError(ctx, ret);

  /* Image output ROI, referenced to the VT crop output top-left corner */
  ret = ctx->write16(ctx, VD55G0_REG_OUT_ROI_X_START, mode->crop.left);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_OUT_ROI_X_END, mode->crop.left + mode->crop.width - 1);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_OUT_ROI_Y_START, 0);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_OUT_ROI_Y_END, mode->crop.height - 1);
  VD55G0_TraceError(ctx, ret);

  return 0;
}

static int VD55G0_SetupLineLen(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  const struct vd55g0_mode *mode;
  int min_line_len_mipi;
  int min_line_len;
  uint8_t bit_per_pixel = drv_ctx->config_save.pixel_depth;
  uint16_t line_len;
  int ret;

  mode = VD55G0_Resolution2Mode(drv_ctx->config_save.resolution);
  if (!mode)
    return -1;

  min_line_len = (drv_ctx->config_save.out_itf.data_rate_in_mps > 900000000) ?
                 VD55G0_MIN_LINE_LEN_ADC_10 : VD55G0_MIN_LINE_LEN_ADC_10_SLOW;

  min_line_len_mipi = ((mode->crop.width * bit_per_pixel + VD55G0_MIPI_MARGIN) * (uint64_t)drv_ctx->pclk)
                      / drv_ctx->config_save.out_itf.data_rate_in_mps;
  line_len = MAX(min_line_len, min_line_len_mipi);

  ret = ctx->write16(ctx, VD55G0_REG_LINE_LENGTH, line_len);
  VD55G0_TraceError(ctx, ret);
  VD55G0_dbg(ctx, 1, "line_length = %d\n", line_len);

  return 0;
}

static int VD55G0_ComputeFrameLength(VD55G0_Ctx_t *ctx, int fps, uint16_t *frame_length)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  const struct vd55g0_mode *mode;
  int min_frame_length;
  int req_frame_length;
  uint16_t line_length;
  uint32_t pixel_clock;
  int ret;

  mode = VD55G0_Resolution2Mode(drv_ctx->config_save.resolution);
  if (!mode)
    return -1;

  ret = ctx->read16(ctx, VD55G0_REG_LINE_LENGTH, &line_length);
  VD55G0_TraceError(ctx, ret);

  min_frame_length = mode->height + VD55G0_FRAME_LENGTH_OFFSET;
  pixel_clock = VD55G0_GetPixelClock(ctx);
  if (!pixel_clock)
    return -1;
  req_frame_length = pixel_clock / (line_length * fps);
  *frame_length = MIN(MAX(min_frame_length, req_frame_length), 65535);

  VD55G0_dbg(ctx, 1, "frame_length to MAX(%d, %d) = %d to reach %d fps\n", min_frame_length, req_frame_length,
             *frame_length, fps);

  return 0;
}

static int VD55G0_SetupFrameRate(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  uint16_t frame_length;
  int ret;

  ret = VD55G0_SetupLineLen(ctx);
  if (ret)
    return ret;

  ret = VD55G0_ComputeFrameLength(ctx, drv_ctx->config_save.frame_rate, &frame_length);
  if (ret)
    return ret;

  VD55G0_dbg(ctx, 1, "Set frame_length to %d to reach %d fps\n", frame_length, drv_ctx->config_save.frame_rate);
  ret = ctx->write16(ctx, VD55G0_REG_FRAME_LENGTH, frame_length);
  VD55G0_TraceError(ctx, ret);

  return 0;
}

static int VD55G0_SetManualExpoGains(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int ret;

  ret = ctx->write16(ctx, VD55G0_REG_MANUAL_COARSE_EXPOSURE, drv_ctx->manual_coarse_integration);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write8(ctx, VD55G0_REG_MANUAL_ANALOG_GAIN, drv_ctx->manual_analog_gain);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_MANUAL_DIGITAL_GAIN_CH0, drv_ctx->manual_digital_gain);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_MANUAL_DIGITAL_GAIN_CH1, drv_ctx->manual_digital_gain);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_MANUAL_DIGITAL_GAIN_CH2, drv_ctx->manual_digital_gain);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_MANUAL_DIGITAL_GAIN_CH3, drv_ctx->manual_digital_gain);
  VD55G0_TraceError(ctx, ret);

  return 0;
}

static int VD55G0_SetupExposure(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  uint8_t reg;
  int ret;

  /* turn on auto exposure except when patgen is active */
  reg = drv_ctx->exposure_mode;
  if (drv_ctx->config_save.patgen != VD55G0_PATGEN_DISABLE)
    reg = VD55G0_EXP_MODE_MANUAL;

  ret = ctx->write8(ctx, VD55G0_REG_EXP_MODE, reg);
  VD55G0_TraceError(ctx, ret);

  if (reg == VD55G0_EXP_MODE_MANUAL)
  {
    ret = VD55G0_SetManualExpoGains(ctx);
    if (ret)
      return ret;
  }

  return 0;
}

static int VD55G0_SetupMirrorFlip(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  uint8_t mode;
  int ret;

  switch (drv_ctx->config_save.flip_mirror_mode) {
  case VD55G0_MIRROR_FLIP_NONE:
    mode = 0;
    break;
  case VD55G0_FLIP:
    mode = 2;
    break;
  case VD55G0_MIRROR:
    mode = 1;
    break;
  case VD55G0_MIRROR_FLIP:
    mode = 3;
    break;
  default:
    return -1;
  }

  ret = ctx->write8(ctx, VD55G0_REG_ORIENTATION, mode);
  VD55G0_TraceError(ctx, ret);

  return 0;
}

static int VD55G0_SetupPatGen(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  uint16_t value = VD55G0_PATGEN_CTRL_DISABLE;
  int ret;

  switch (drv_ctx->config_save.patgen) {
  case VD55G0_PATGEN_DISABLE:
    value = VD55G0_PATGEN_CTRL_DISABLE;
    break;
  case VD55G0_PATGEN_DIAGONAL_GRAYSCALE:
    value = VD55G0_PATGEN_CTRL_DIAG_GRAY;
    break;
  case VD55G0_PATGEN_PSEUDO_RANDOM:
    value = VD55G0_PATGEN_CTRL_PSN;
    break;
  default:
    return -1;
  }

  if (drv_ctx->config_save.patgen != VD55G0_PATGEN_DISABLE)
  {
    ret = ctx->write8(ctx, VD55G0_REG_DUSTER_CTRL, VD55G0_DUSTER_DISABLE);
    VD55G0_TraceError(ctx, ret);
    ret = ctx->write8(ctx, VD55G0_REG_DARKCAL_CTRL, VD55G0_DARKCAL_BYPASS_DARKAVG);
    VD55G0_TraceError(ctx, ret);
  }

  ret = ctx->write16(ctx, VD55G0_REG_PATGEN_CTRL, value);
  VD55G0_TraceError(ctx, ret);

  return 0;
}

static int VD55G0_SetFlicker(VD55G0_Ctx_t *ctx, VD55G0_Flicker_t flicker)
{
  uint16_t mode;
  int ret;

  switch (flicker) {
  case VD55G0_FLICKER_FREE_NONE:
    mode = 0;
    break;
  case VD55G0_FLICKER_FREE_50HZ:
    mode = 1;
    break;
  case VD55G0_FLICKER_FREE_60HZ:
    mode = 3;
    break;
  default:
    return -1;
  }

  ret = ctx->write16(ctx, VD55G0_REG_AE_COMPILER_CONTROL, mode);
  VD55G0_TraceError(ctx, ret);

  return 0;
}

static int VD55G0_Flicker(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;

  return VD55G0_SetFlicker(ctx, drv_ctx->config_save.flicker);
}

static int VD55G0_Setup(VD55G0_Ctx_t *ctx)
{
  int ret;

  ret = VD55G0_SetupClocks(ctx);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_SetupOutput(ctx);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_SetupSize(ctx);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_SetupFrameRate(ctx);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_SetupExposure(ctx);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_SetupMirrorFlip(ctx);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_SetupPatGen(ctx);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_Flicker(ctx);
  VD55G0_TraceError(ctx, ret);

  return 0;
}

static int VD55G0_StartStreaming(VD55G0_Ctx_t *ctx)
{
  int ret;

  ret = ctx->write8(ctx, VD55G0_REG_SW_STANDBY, VD55G0_STBY_START_STREAM);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_PollReg8(ctx, VD55G0_REG_SW_STANDBY, VD55G0_CMD_ACK);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_WaitState(ctx, VD55G0_SYSTEM_FSM_STREAMING);
  VD55G0_TraceError(ctx, ret);

  VD55G0_notice(ctx, "Streaming is on\n");

  return 0;
}

static int VD55G0_StopStreaming(VD55G0_Ctx_t *ctx)
{
  int ret;

  ret = ctx->write8(ctx, VD55G0_REG_STREAMING, VD55G0_STREAMING_STOP_STREAM);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_PollReg8(ctx, VD55G0_REG_STREAMING, VD55G0_CMD_ACK);
  VD55G0_TraceError(ctx, ret);

  ret = VD55G0_WaitState(ctx, VD55G0_SYSTEM_FSM_SW_STBY);
  VD55G0_TraceError(ctx, ret);

  VD55G0_notice(ctx, "Streaming is off\n");

  return 0;
}

int VD55G0_Init(VD55G0_Ctx_t *ctx, VD55G0_Config_t *config)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int ret;

  if (config->frame_rate < VD55G0_MIN_FPS)
    return -1;
  if (config->frame_rate > VD55G0_MAX_FPS)
    return -1;

  if ((config->resolution != VD55G0_RES_QVGA_320_240) &&
      (config->resolution != VD55G0_RES_VGA_640_480) &&
      (config->resolution != VD55G0_RES_FULL_644_604)) {
    return -1;
  }

  if ((config->pixel_depth != 8) && (config->pixel_depth != 10))
    return -1;

  drv_ctx->config_save = *config;
  drv_ctx->exposure_mode = VD55G0_EXPOSURE_MODE_AUTO;
  drv_ctx->manual_coarse_integration = VD55G0_EXPOSURE_COARSE_DEFAULT;
  drv_ctx->manual_analog_gain = VD55G0_ANALOG_GAIN_DEFAULT;
  drv_ctx->manual_digital_gain = VD55G0_DIGITAL_GAIN_DEFAULT;

  ctx->shutdown_pin(ctx, 0);
  ctx->delay(ctx, 10);
  ctx->shutdown_pin(ctx, 1);
  ctx->delay(ctx, 10);

  ret = VD55G0_Boot(ctx);
  if (ret)
    return ret;

  drv_ctx->state = VD55G0_ST_IDLE;

  return 0;
}

int VD55G0_DeInit(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;

  if (drv_ctx->state == VD55G0_ST_STREAMING)
    return -1;

  ctx->shutdown_pin(ctx, 0);
  ctx->delay(ctx, 10);

  return 0;
}

int VD55G0_Start(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int ret;

  ret = VD55G0_Setup(ctx);
  if (ret)
    return ret;

  ret = VD55G0_StartStreaming(ctx);
  if (ret)
    return ret;
  drv_ctx->state = VD55G0_ST_STREAMING;

  return 0;
}

int VD55G0_Stop(VD55G0_Ctx_t *ctx)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int ret;

  ret = VD55G0_StopStreaming(ctx);
  if (ret)
    return ret;
  drv_ctx->state = VD55G0_ST_IDLE;

  return 0;
}

int VD55G0_SetFlipMirrorMode(VD55G0_Ctx_t *ctx, VD55G0_MirrorFlip_t mode)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int is_streaming;
  int ret;

  is_streaming = VD55G0_IsStreaming(ctx);
  if (is_streaming < 0)
    return is_streaming;

  if (is_streaming) {
    ret = VD55G0_Stop(ctx);
    if (ret)
      return ret;
  }

  drv_ctx->config_save.flip_mirror_mode = mode;
  ret = VD55G0_SetBayerType(ctx);
  if (ret)
    return ret;

  if (is_streaming) {
    ret = VD55G0_Start(ctx);
    if (ret)
      return ret;
  }

  return 0;
}

int VD55G0_GetBrightnessLevel(VD55G0_Ctx_t *ctx, int *level)
{
  uint16_t value;
  int ret;

  ret = ctx->read16(ctx, VD55G0_REG_AE_TARGET_PERCENTAGE, &value);
  VD55G0_TraceError(ctx, ret);
  *level = value;

  return 0;
}

int VD55G0_SetBrightnessLevel(VD55G0_Ctx_t *ctx, int level)
{
  uint16_t value = level;
  int ret;

  if (level < VD55G0_MIN_BRIGHTNESS || level > VD55G0_MAX_BRIGHTNESS)
    return -1;

  ret = ctx->write16(ctx, VD55G0_REG_AE_TARGET_PERCENTAGE, value);
  VD55G0_TraceError(ctx, ret);

  return 0;
}

int VD55G0_SetFlickerMode(VD55G0_Ctx_t *ctx, VD55G0_Flicker_t mode)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int ret;

  ret = VD55G0_SetFlicker(ctx, mode);
  if (ret)
    return ret;

  drv_ctx->config_save.flicker = mode;

  return 0;
}

int VD55G0_SetExposureMode(VD55G0_Ctx_t *ctx, VD55G0_ExposureMode_t mode)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int ret;

  if ((mode != VD55G0_EXPOSURE_MODE_AUTO) &&
      (mode != VD55G0_EXPOSURE_MODE_FREEZE) &&
      (mode != VD55G0_EXPOSURE_MODE_MANUAL))
    return -1;

  ret = ctx->write8(ctx, VD55G0_REG_EXP_MODE, (uint8_t)mode);
  VD55G0_TraceError(ctx, ret);

  drv_ctx->exposure_mode = (uint8_t)mode;

  if (mode == VD55G0_EXPOSURE_MODE_MANUAL)
    return VD55G0_SetManualExpoGains(ctx);

  return 0;
}

int VD55G0_SetAnalogGain(VD55G0_Ctx_t *ctx, int gain)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int ret;

  if ((gain < VD55G0_ANALOG_GAIN_MIN) || (gain > VD55G0_ANALOG_GAIN_MAX))
    return -1;

  ret = ctx->write8(ctx, VD55G0_REG_MANUAL_ANALOG_GAIN, (uint8_t)gain);
  VD55G0_TraceError(ctx, ret);

  drv_ctx->manual_analog_gain = (uint8_t)gain;

  return 0;
}

int VD55G0_SetDigitalGain(VD55G0_Ctx_t *ctx, int gain)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int ret;

  if ((gain < (int)VD55G0_DIGITAL_GAIN_MIN) || (gain > (int)VD55G0_DIGITAL_GAIN_MAX))
    return -1;

  ret = ctx->write16(ctx, VD55G0_REG_MANUAL_DIGITAL_GAIN_CH0, (uint16_t)gain);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_MANUAL_DIGITAL_GAIN_CH1, (uint16_t)gain);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_MANUAL_DIGITAL_GAIN_CH2, (uint16_t)gain);
  VD55G0_TraceError(ctx, ret);
  ret = ctx->write16(ctx, VD55G0_REG_MANUAL_DIGITAL_GAIN_CH3, (uint16_t)gain);
  VD55G0_TraceError(ctx, ret);

  drv_ctx->manual_digital_gain = (uint16_t)gain;

  return 0;
}

int VD55G0_GetExposureRegRange(VD55G0_Ctx_t *ctx, uint32_t *min_us, uint32_t *max_us)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  uint32_t line_time_in_us;
  uint16_t frame_length;
  int ret;

  if ((min_us == NULL) || (max_us == NULL))
    return -1;

  ret = VD55G0_GetLineTimeInUs(ctx, &line_time_in_us);
  if (ret)
    return ret;

  *min_us = VD55G0_EXPOSURE_MIN_COARSE_LINES_ADC_10 * line_time_in_us;

  ret = VD55G0_ComputeFrameLength(ctx, drv_ctx->config_save.frame_rate, &frame_length);
  if (ret)
    return ret;

  *max_us = (frame_length - VD55G0_EXP_COARSE_INTG_MARGIN_ADC_10) * line_time_in_us;

  return 0;
}

int VD55G0_SetExposureTime(VD55G0_Ctx_t *ctx, int exposure_us)
{
  struct drv_ctx *drv_ctx = &ctx->ctx;
  int32_t ret;
  uint32_t exp_min, exp_max;
  uint32_t line_time_in_us;
  uint32_t exposure_reg;

  ret = VD55G0_GetExposureRegRange(ctx, &exp_min, &exp_max);
  if (ret)
    return ret;

  if ((uint32_t)exposure_us < exp_min || (uint32_t)exposure_us > exp_max)
    return -1;

  ret = VD55G0_GetLineTimeInUs(ctx, &line_time_in_us);
  if (ret)
    return ret;

  exposure_reg = CEIL(exposure_us / line_time_in_us);
  ret = ctx->write16(ctx, VD55G0_REG_MANUAL_COARSE_EXPOSURE, exposure_reg);
  VD55G0_TraceError(ctx, ret);

  drv_ctx->manual_coarse_integration = (uint16_t)exposure_reg;

  return 0;
}

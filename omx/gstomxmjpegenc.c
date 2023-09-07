/*
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 * Copyright (C) 2017 Xilinx, Inc.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstomxmjpegenc.h"
#include "gstomxvideo.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_mjpeg_enc_debug_category);
#define GST_CAT_DEFAULT gst_omx_mjpeg_enc_debug_category

#define JPEG_DEFAULT_QFACTOR 1

/* prototypes */
static gboolean gst_omx_mjpeg_enc_set_format (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstCaps *gst_omx_mjpeg_enc_get_caps (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static void gst_omx_mjpeg_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_mjpeg_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

enum
{
  PROP_0,
  PROP_QFACTOR
};

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_mjpeg_enc_debug_category, "omxmjpegenc", 0, \
      "debug category for gst-omx MJPEG video encoder");

#define parent_class gst_omx_mjpeg_enc_parent_class
G_DEFINE_TYPE_WITH_CODE (GstOMXMJPEGEnc, gst_omx_mjpeg_enc,
    GST_TYPE_OMX_VIDEO_ENC, DEBUG_INIT);

static gboolean
gst_omx_mjpeg_enc_flush (GstVideoEncoder * enc)
{
  GstOMXMJPEGEnc *self = GST_OMX_MJPEG_ENC (enc);

  g_list_free_full (self->headers, (GDestroyNotify) gst_buffer_unref);
  self->headers = NULL;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->flush (enc);
}

static gboolean
gst_omx_mjpeg_enc_stop (GstVideoEncoder * enc)
{
  GstOMXMJPEGEnc *self = GST_OMX_MJPEG_ENC (enc);

  g_list_free_full (self->headers, (GDestroyNotify) gst_buffer_unref);
  self->headers = NULL;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->stop (enc);
}

static void
gst_omx_mjpeg_enc_class_init (GstOMXMJPEGEncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *basevideoenc_class = GST_VIDEO_ENCODER_CLASS (klass);
  GstOMXVideoEncClass *videoenc_class = GST_OMX_VIDEO_ENC_CLASS (klass);

  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_mjpeg_enc_set_format);
  videoenc_class->get_caps = GST_DEBUG_FUNCPTR (gst_omx_mjpeg_enc_get_caps);

  basevideoenc_class->flush = gst_omx_mjpeg_enc_flush;
  basevideoenc_class->stop = gst_omx_mjpeg_enc_stop;

  gobject_class->set_property = gst_omx_mjpeg_enc_set_property;
  gobject_class->get_property = gst_omx_mjpeg_enc_get_property;

  g_object_class_install_property (gobject_class, PROP_QFACTOR,
      g_param_spec_int ("qfactor", "Qfactor", "Q factor of encoding",
          0, 10, JPEG_DEFAULT_QFACTOR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  videoenc_class->cdata.default_sink_template_caps =
      GST_VIDEO_CAPS_MAKE (GST_OMX_VIDEO_ENC_SUPPORTED_FORMATS);

  videoenc_class->cdata.default_src_template_caps = "image/jpeg, "
      "width=(int) [1,MAX], " "height=(int) [1,MAX],"
      "framerate = (fraction) [ 0/1, MAX ], ";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX MJPEG Video Encoder",
      "Codec/Encoder/Video/Hardware",
      "Encode MJPEG video streams",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_omx_set_default_role (&videoenc_class->cdata, "video_encoder.jpeg");
}

static void
gst_omx_mjpeg_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOMXMJPEGEnc *self = GST_OMX_MJPEG_ENC (object);

  switch (prop_id) {
    case PROP_QFACTOR:
      self->qfactor = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_mjpeg_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOMXMJPEGEnc *self = GST_OMX_MJPEG_ENC (object);

  switch (prop_id) {
    case PROP_QFACTOR:
      g_value_set_int (value, self->qfactor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_mjpeg_enc_init (GstOMXMJPEGEnc * self)
{
  self->qfactor = JPEG_DEFAULT_QFACTOR;
}

/* Update OMX_IndexParamQFactor
 *
 * Returns TRUE if succeeded or if not supported, FALSE if failed */
static gboolean
update_param_mjpeg (GstOMXMJPEGEnc * self)
{
  OMX_IMAGE_PARAM_QFACTORTYPE param;
  OMX_ERRORTYPE err;

  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = GST_OMX_VIDEO_ENC (self)->enc_out_port->index;

  param.nQFactor = self->qfactor;
  err =
      gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
                                      OMX_IndexParamQFactor, &param);

  if (err == OMX_ErrorUnsupportedIndex) {
    GST_WARNING_OBJECT (self,
        "Setting OMX_IndexParamQFactor not supported by component");
    return TRUE;
  } else if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Error setting QFactor settings (QFactor %u): %s (0x%08x)",
        (guint) param.nQFactor, gst_omx_error_to_string (err), err);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_omx_mjpeg_enc_set_format (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXMJPEGEnc *self = GST_OMX_MJPEG_ENC (enc);
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_ERRORTYPE err;

  gst_omx_port_get_port_definition (GST_OMX_VIDEO_ENC (self)->enc_out_port,
      &port_def);
  port_def.format.video.eCompressionFormat =
      (OMX_VIDEO_CODINGTYPE) OMX_VIDEO_CodingMJPEG;
  err =
      gst_omx_port_update_port_definition (GST_OMX_VIDEO_ENC
      (self)->enc_out_port, &port_def);
  if (err != OMX_ErrorNone)
    return FALSE;

  if (!update_param_mjpeg (self))
    return FALSE;

  return TRUE;
}

static GstCaps *
gst_omx_mjpeg_enc_get_caps (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstCaps *caps;

  caps = gst_caps_new_simple ("image/jpeg", "parsed", G_TYPE_BOOLEAN, "true", NULL);

  return caps;
}

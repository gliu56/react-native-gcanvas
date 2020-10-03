// Copyright (c) 2010 LearnBoost <tj@learnboost.com>

#include "CairoCanvasRenderingContext2d.h"

#include <algorithm>
// #include "backend/ImageBackend.h"
#include "backend/CairoImageBackend.h"
#include "CairoCanvas.h"
#include "CairoCanvasGradient.h"
#include "CairoCanvasPattern.h"
#include <cmath>
#include <cstdlib>
#include "CairoImage.h"
#include "CairoImageData.h"
#include <limits>
#include <map>
#include "Point.h"
#include <string>
#include <vector>

//测量耗时的调试开关
// #define DUMP_RUNNING_TIME 1

#ifdef DUMP_RUNNING_TIME
#define RECORD_TIME_BEGIN \
  clock_t start, finish;  \
  start = clock();

#define RECORD_TIME_END \
  finish = clock();     \
  printf("cost time %f ms\n", (double)(finish - start) * 1000.0f / CLOCKS_PER_SEC);
#else
#define RECORD_TIME_BEGIN
#define RECORD_TIME_END
#endif


#ifdef TRACE_API
#define TRACE_CONTEXT_API     std::cout << "CairoContext2d ::"<< __FUNCTION__ << std::endl;
#else
#define TRACE_CONTEXT_API

#endif
namespace cairocanvas
{
  Napi::FunctionReference Context2d::constructor;
  Napi::FunctionReference Context2d::_DOMMatrix;
  Napi::FunctionReference Context2d::_parseFont;


#ifndef isnan
#define isnan(x) std::isnan(x)
#define isinf(x) std::isinf(x)
#endif
/*
 * Rectangle arg assertions.
 */
#define RECT_ARGS                                     \
  if (info.Length() < 4)                              \
    return;                                           \
  double args[4];                                     \
  args[0] = info[0].As<Napi::Number>().DoubleValue(); \
  args[1] = info[1].As<Napi::Number>().DoubleValue(); \
  args[2] = info[2].As<Napi::Number>().DoubleValue(); \
  args[3] = info[3].As<Napi::Number>().DoubleValue(); \
  double x = args[0];                                 \
  double y = args[1];                                 \
  double width = args[2];                             \
  double height = args[3];

  /*
 * Text baselines.
 */
  enum
  {
    TEXT_BASELINE_ALPHABETIC,
    TEXT_BASELINE_TOP,
    TEXT_BASELINE_BOTTOM,
    TEXT_BASELINE_MIDDLE,
    TEXT_BASELINE_IDEOGRAPHIC,
    TEXT_BASELINE_HANGING
  };
  /*
 * Simple helper macro for a rather verbose function call.
 */

#define PANGO_LAYOUT_GET_METRICS(LAYOUT) pango_context_get_metrics( \
   pango_layout_get_context(LAYOUT), \
   pango_layout_get_font_description(LAYOUT), \
   pango_context_get_language(pango_layout_get_context(LAYOUT)))

inline static bool checkArgs(const Napi::CallbackInfo &info, double *args, int argsNum, int offset = 0)
{
  int argsEnd = offset + argsNum;
  bool areArgsValid = true;

  for (int i = offset; i < argsEnd; i++)
  {
    double val = info[i].As<Napi::Number>().DoubleValue();

    if (areArgsValid)
    {
      if (val != val ||
          val == std::numeric_limits<double>::infinity() ||
          val == -std::numeric_limits<double>::infinity())
      {
        // We should continue the loop instead of returning immediately
        // See https://html.spec.whatwg.org/multipage/canvas.html

        areArgsValid = false;
        continue;
      }

      args[i - offset] = val;
    }
  }

  return areArgsValid;
}

void Context2d::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  Napi::Function func = DefineClass(env, "CanvasRenderingContext2D", {
    InstanceMethod("fillRect", &Context2d::fillRect),
    InstanceMethod("arc", &Context2d::arc),
    InstanceMethod("arcTo", &Context2d::arcTo),
    InstanceMethod("beginPath", &Context2d::beginPath),
    InstanceMethod("bezierCurveTo", &Context2d::bezierCurveTo),
    InstanceMethod("clearRect", &Context2d::clearRect),
    InstanceMethod("clip", &Context2d::clip),
    InstanceMethod("closePath", &Context2d::closePath),
    InstanceMethod("createImageData", &Context2d::createImageData),
    InstanceMethod("createLinearGradient", &Context2d::createLinearGradient),
    InstanceMethod("createPattern", &Context2d::createPattern),
    InstanceMethod("createRadialGradient", &Context2d::createRadialGradient),
    InstanceMethod("drawImage", &Context2d::drawImage),
    InstanceMethod("fill", &Context2d::fill),
    InstanceMethod("fillText", &Context2d::fillText),
    InstanceMethod("getImageData", &Context2d::getImageData),
    InstanceMethod("getLineDash", &Context2d::getLineDash),
    InstanceMethod("lineTo", &Context2d::lineTo),
    InstanceMethod("measureText", &Context2d::measureText),
    InstanceMethod("moveTo", &Context2d::moveTo),
    InstanceMethod("putImageData", &Context2d::putImageData),
    InstanceMethod("quadraticCurveTo", &Context2d::quadraticCurveTo),
    InstanceMethod("rect", &Context2d::rect),
    InstanceMethod("resetTransform", &Context2d::resetTransform),
    InstanceMethod("restore", &Context2d::restore),
    InstanceMethod("rotate", &Context2d::rotate),
    InstanceMethod("save", &Context2d::save),
    InstanceMethod("scale", &Context2d::scale),
    InstanceMethod("setLineDash", &Context2d::setLineDash),
    InstanceMethod("setTransform", &Context2d::setTransform),
    InstanceMethod("stroke", &Context2d::stroke),
    InstanceMethod("strokeRect", &Context2d::strokeRect),
    InstanceMethod("strokeText", &Context2d::strokeText),
    InstanceMethod("transform", &Context2d::transform),
    InstanceMethod("translate", &Context2d::translate),
    InstanceMethod("ellipse", &Context2d::ellipse),

    InstanceAccessor("fillStyle", &Context2d::getFillStyle, &Context2d::setFillStyle),
    InstanceAccessor("font", &Context2d::getfont, &Context2d::setfont),
    InstanceAccessor("globalAlpha", &Context2d::getglobalAlpha, &Context2d::setglobalAlpha),
    InstanceAccessor("globalCompositeOperation", &Context2d::getglobalCompositeOperation, &Context2d::setglobalCompositeOperation),
    InstanceAccessor("lineCap", &Context2d::getlineCap, &Context2d::setlineCap),
    InstanceAccessor("lineDashOffset", &Context2d::getlineDashOffset, &Context2d::setlineDashOffset),
    InstanceAccessor("lineJoin", &Context2d::getlineJoin, &Context2d::setlineJoin),
    InstanceAccessor("lineWidth", &Context2d::getlineWidth, &Context2d::setlineWidth),
    InstanceAccessor("miterLimit", &Context2d::getmiterLimit, &Context2d::setmiterLimit),
    InstanceAccessor("shadowBlur", &Context2d::getshadowBlur, &Context2d::setshadowBlur),
    InstanceAccessor("shadowColor", &Context2d::getshadowColor, &Context2d::setshadowColor),
    InstanceAccessor("shadowOffsetX", &Context2d::getshadowOffsetX, &Context2d::setshadowOffsetX),
    InstanceAccessor("shadowOffsetY", &Context2d::getshadowOffsetY, &Context2d::setshadowOffsetY),
    InstanceAccessor("strokeStyle", &Context2d::getstrokeStyle, &Context2d::setstrokeStyle),
    InstanceAccessor("textAlign", &Context2d::gettextAlign, &Context2d::settextAlign),
    InstanceAccessor("textBaseline", &Context2d::gettextBaseline, &Context2d::settextBaseline),
    InstanceAccessor("canvas", &Context2d::getCanvas, nullptr),
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
}

Napi::Object Context2d::NewInstance(const Napi::CallbackInfo &info)
{
  Napi::Object obj = constructor.New({});
  obj.Set("name", Napi::String::New(info.Env(), "context2d"));
  return obj;
}


Context2d::Context2d(const Napi::CallbackInfo &info): Napi::ObjectWrap<Context2d>(info)
{

}

/*
 * Create a cairo context.
 */

void Context2d::setupContext2d(Canvas *canvas)
{
  //TODO 构造函数确认下
  _canvas = canvas;
  _context = canvas->createCairoContext();

  if( _context != nullptr  )
  {
      std::cout << "Context2d::setupContext2d()  createCairoContext success  " << std::endl;
  }

  _layout = pango_cairo_create_layout(_context);
  state = states[stateno = 0] = (canvas_state_t *)malloc(sizeof(canvas_state_t));

  resetState(true);
}

Napi::Value Context2d::getCanvas(const Napi::CallbackInfo &info)
{
  return _canvas->mRef.Value();
}


/*
 * Destroy cairo context.
 */

Context2d::~Context2d()
{
  while (stateno >= 0)
  {
    pango_font_description_free(states[stateno]->fontDescription);
    free(states[stateno--]);
  }
  g_object_unref(_layout);
  cairo_destroy(_context);
  _resetPersistentHandles();
}

/*
 * Reset canvas state.
 */

void Context2d::resetState(bool init)
{
  if (!init)
  {
    pango_font_description_free(state->fontDescription);
  }

  state->shadowBlur = 0;
  state->shadowOffsetX = state->shadowOffsetY = 0;
  state->globalAlpha = 1;
  state->textAlignment = -1;
  state->fillPattern = nullptr;
  state->strokePattern = nullptr;
  state->fillGradient = nullptr;
  state->strokeGradient = nullptr;
  state->textBaseline = TEXT_BASELINE_ALPHABETIC;
  rgba_t transparent = {0, 0, 0, 1};
  rgba_t transparent_black = {0, 0, 0, 0};
  state->fill = transparent;
  state->stroke = transparent;
  state->shadow = transparent_black;
  state->patternQuality = CAIRO_FILTER_GOOD;
  state->imageSmoothingEnabled = true;
  state->textDrawingMode = TEXT_DRAW_PATHS;
  state->fontDescription = pango_font_description_from_string("sans serif");
  pango_font_description_set_absolute_size(state->fontDescription, 10 * PANGO_SCALE);
  pango_layout_set_font_description(_layout, state->fontDescription);

  _resetPersistentHandles();
}

void Context2d::_resetPersistentHandles()
{
  _fillStyle.Reset();
  _strokeStyle.Reset();
  _font.Reset();
  _textBaseline.Reset();
  _textAlign.Reset();
}

/*
 * Save cairo / canvas state.
 */

void Context2d::save()
{
  if (stateno < CANVAS_MAX_STATES)
  {
    cairo_save(_context);
    states[++stateno] = (canvas_state_t *)malloc(sizeof(canvas_state_t));
    memcpy(states[stateno], state, sizeof(canvas_state_t));
    states[stateno]->fontDescription = pango_font_description_copy(states[stateno - 1]->fontDescription);
    state = states[stateno];
  }
}

/*
 * Restore cairo / canvas state.
 */

void Context2d::restore()
{
  if (stateno > 0)
  {
    cairo_restore(_context);
    pango_font_description_free(states[stateno]->fontDescription);
    free(states[stateno]);
    states[stateno] = NULL;
    state = states[--stateno];
    pango_layout_set_font_description(_layout, state->fontDescription);
  }
}

/*
 * Save flat path.
 */

void Context2d::savePath()
{
  _path = cairo_copy_path_flat(_context);
  cairo_new_path(_context);
}

/*
 * Restore flat path.
 */

void Context2d::restorePath()
{
  cairo_new_path(_context);
  cairo_append_path(_context, _path);
  cairo_path_destroy(_path);
}

/*
 * Create temporary surface for gradient or pattern transparency
 */
cairo_pattern_t * create_transparent_gradient(cairo_pattern_t *source, float alpha)
{
  double x0;
  double y0;
  double x1;
  double y1;
  double r0;
  double r1;
  int count;
  int i;
  double offset;
  double r;
  double g;
  double b;
  double a;
  cairo_pattern_t *newGradient;
  cairo_pattern_type_t type = cairo_pattern_get_type(source);
  cairo_pattern_get_color_stop_count(source, &count);
  if (type == CAIRO_PATTERN_TYPE_LINEAR)
  {
    cairo_pattern_get_linear_points(source, &x0, &y0, &x1, &y1);
    newGradient = cairo_pattern_create_linear(x0, y0, x1, y1);
  }
  else if (type == CAIRO_PATTERN_TYPE_RADIAL)
  {
    cairo_pattern_get_radial_circles(source, &x0, &y0, &r0, &x1, &y1, &r1);
    newGradient = cairo_pattern_create_radial(x0, y0, r0, x1, y1, r1);
  }
  else
  {
    // Nan::ThrowError("Unexpected gradient type");
    return NULL;
  }
  for (i = 0; i < count; i++)
  {
    cairo_pattern_get_color_stop_rgba(source, i, &offset, &r, &g, &b, &a);
    cairo_pattern_add_color_stop_rgba(newGradient, offset, r, g, b, a * alpha);
  }
  return newGradient;
}

cairo_pattern_t * create_transparent_pattern(cairo_pattern_t *source, float alpha)
{
  cairo_surface_t *surface;
  cairo_pattern_get_surface(source, &surface);
  int width = cairo_image_surface_get_width(surface);
  int height = cairo_image_surface_get_height(surface);
  cairo_surface_t *mask_surface = cairo_image_surface_create(
      CAIRO_FORMAT_ARGB32,
      width,
      height);
  cairo_t *mask_context = cairo_create(mask_surface);
  if (cairo_status(mask_context) != CAIRO_STATUS_SUCCESS)
  {
    // Nan::ThrowError("Failed to initialize context");
    return NULL;
  }
  cairo_set_source(mask_context, source);
  cairo_paint_with_alpha(mask_context, alpha);
  cairo_destroy(mask_context);
  cairo_pattern_t *newPattern = cairo_pattern_create_for_surface(mask_surface);
  cairo_surface_destroy(mask_surface);
  return newPattern;
}

/*
 * Fill and apply shadow.
 */

void Context2d::setFillRule(const Napi::Value &value)
{
  cairo_fill_rule_t rule = CAIRO_FILL_RULE_WINDING;
  if (value.IsString())
  {
    std::string rule = value.As<Napi::String>().Utf8Value();
    if (rule == "evenodd")
    {
      rule = CAIRO_FILL_RULE_EVEN_ODD;
    }
  }
  cairo_set_fill_rule(_context, rule);
}

void Context2d::fill(bool preserve)
{
  cairo_pattern_t *new_pattern;
  if (state->fillPattern)
  {
    if (state->globalAlpha < 1)
    {
      new_pattern = create_transparent_pattern(state->fillPattern, state->globalAlpha);
      if (new_pattern == NULL)
      {
        // failed to allocate; Nan::ThrowError has already been called, so return from this fn.
        return;
      }
      cairo_set_source(_context, new_pattern);
      cairo_pattern_destroy(new_pattern);
    }
    else
    {
      cairo_set_source(_context, state->fillPattern);
    }
    repeat_type_t repeat = Pattern::get_repeat_type_for_cairo_pattern(state->fillPattern);
    if (NO_REPEAT == repeat)
    {
      cairo_pattern_set_extend(cairo_get_source(_context), CAIRO_EXTEND_NONE);
    }
    else
    {
      cairo_pattern_set_extend(cairo_get_source(_context), CAIRO_EXTEND_REPEAT);
    }
  }
  else if (state->fillGradient)
  {
    if (state->globalAlpha < 1)
    {
      new_pattern = create_transparent_gradient(state->fillGradient, state->globalAlpha);
      if (new_pattern == NULL)
      {
        // failed to recognize gradient; Nan::ThrowError has already been called, so return from this fn.
        return;
      }
      cairo_pattern_set_filter(new_pattern, state->patternQuality);
      cairo_set_source(_context, new_pattern);
      cairo_pattern_destroy(new_pattern);
    }
    else
    {
      cairo_pattern_set_filter(state->fillGradient, state->patternQuality);
      cairo_set_source(_context, state->fillGradient);
    }
  }
  else
  {
    setSourceRGBA(state->fill);
  }
  if (preserve)
  {
    hasShadow() ? shadow(cairo_fill_preserve) : cairo_fill_preserve(_context);
  }
  else
  {
    hasShadow() ? shadow(cairo_fill) : cairo_fill(_context);
  }
}

/*
 * Stroke and apply shadow.
 */

void Context2d::stroke(bool preserve)
{
  cairo_pattern_t *new_pattern;
  if (state->strokePattern)
  {
    if (state->globalAlpha < 1)
    {
      new_pattern = create_transparent_pattern(state->strokePattern, state->globalAlpha);
      if (new_pattern == NULL)
      {
        // failed to allocate; Nan::ThrowError has already been called, so return from this fn.
        return;
      }
      cairo_set_source(_context, new_pattern);
      cairo_pattern_destroy(new_pattern);
    }
    else
    {
      cairo_set_source(_context, state->strokePattern);
    }
    repeat_type_t repeat = Pattern::get_repeat_type_for_cairo_pattern(state->strokePattern);
    if (NO_REPEAT == repeat)
    {
      cairo_pattern_set_extend(cairo_get_source(_context), CAIRO_EXTEND_NONE);
    }
    else
    {
      cairo_pattern_set_extend(cairo_get_source(_context), CAIRO_EXTEND_REPEAT);
    }
  }
  else if (state->strokeGradient)
  {
    if (state->globalAlpha < 1)
    {
      new_pattern = create_transparent_gradient(state->strokeGradient, state->globalAlpha);
      if (new_pattern == NULL)
      {
        // failed to recognize gradient; Nan::ThrowError has already been called, so return from this fn.
        return;
      }
      cairo_pattern_set_filter(new_pattern, state->patternQuality);
      cairo_set_source(_context, new_pattern);
      cairo_pattern_destroy(new_pattern);
    }
    else
    {
      cairo_pattern_set_filter(state->strokeGradient, state->patternQuality);
      cairo_set_source(_context, state->strokeGradient);
    }
  }
  else
  {
    setSourceRGBA(state->stroke);
  }

  if (preserve)
  {
    hasShadow() ? shadow(cairo_stroke_preserve) : cairo_stroke_preserve(_context);
  }
  else
  {
    hasShadow() ? shadow(cairo_stroke) : cairo_stroke(_context);
  }
}

/*
 * Apply shadow with the given draw fn.
 */

void Context2d::shadow(void(fn)(cairo_t *cr))
{
  cairo_path_t *path = cairo_copy_path_flat(_context);
  cairo_save(_context);

  // shadowOffset is unaffected by current transform
  cairo_matrix_t path_matrix;
  cairo_get_matrix(_context, &path_matrix);
  cairo_identity_matrix(_context);

  // Apply shadow
  cairo_push_group(_context);

  // No need to invoke blur if shadowBlur is 0
  if (state->shadowBlur)
  {
    // find out extent of path
    double x1, y1, x2, y2;
    if (fn == cairo_fill || fn == cairo_fill_preserve)
    {
      cairo_fill_extents(_context, &x1, &y1, &x2, &y2);
    }
    else
    {
      cairo_stroke_extents(_context, &x1, &y1, &x2, &y2);
    }

    // create new image surface that size + padding for blurring
    double dx = x2 - x1, dy = y2 - y1;
    cairo_user_to_device_distance(_context, &dx, &dy);
    int pad = state->shadowBlur * 2;
    cairo_surface_t *shadow_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32,
        dx + 2 * pad,
        dy + 2 * pad);
    cairo_t *shadow_context = cairo_create(shadow_surface);

    // transform path to the right place
    cairo_translate(shadow_context, pad - x1, pad - y1);
    cairo_transform(shadow_context, &path_matrix);

    // set lineCap lineJoin lineDash
    cairo_set_line_cap(shadow_context, cairo_get_line_cap(_context));
    cairo_set_line_join(shadow_context, cairo_get_line_join(_context));

    double offset;
    int dashes = cairo_get_dash_count(_context);
    std::vector<double> a(dashes);
    cairo_get_dash(_context, a.data(), &offset);
    cairo_set_dash(shadow_context, a.data(), dashes, offset);

    // draw the path and blur
    cairo_set_line_width(shadow_context, cairo_get_line_width(_context));
    cairo_new_path(shadow_context);
    cairo_append_path(shadow_context, path);
    setSourceRGBA(shadow_context, state->shadow);
    fn(shadow_context);
    blur(shadow_surface, state->shadowBlur);

    // paint to original context
    cairo_set_source_surface(_context, shadow_surface,
                              x1 - pad + state->shadowOffsetX + 1,
                              y1 - pad + state->shadowOffsetY + 1);
    cairo_paint(_context);
    cairo_destroy(shadow_context);
    cairo_surface_destroy(shadow_surface);
  }
  else
  {
    // Offset first, then apply path's transform
    cairo_translate(
        _context, state->shadowOffsetX, state->shadowOffsetY);
    cairo_transform(_context, &path_matrix);

    // Apply shadow
    cairo_new_path(_context);
    cairo_append_path(_context, path);
    setSourceRGBA(state->shadow);

    fn(_context);
  }

  // Paint the shadow
  cairo_pop_group_to_source(_context);
  cairo_paint(_context);

  // Restore state
  cairo_restore(_context);
  cairo_new_path(_context);
  cairo_append_path(_context, path);
  fn(_context);

  cairo_path_destroy(path);
}

/*
 * Set source RGBA for the current context
 */

void Context2d::setSourceRGBA(rgba_t color)
{
  setSourceRGBA(_context, color);
}

  /*
 * Set source RGBA
 */

void Context2d::setSourceRGBA(cairo_t *ctx, rgba_t color)
{
  cairo_set_source_rgba(ctx, color.r, color.g, color.b, color.a * state->globalAlpha);
}

/*
 * Check if the context has a drawable shadow.
 */

bool Context2d::hasShadow()
{
  return state->shadow.a && (state->shadowBlur || state->shadowOffsetX || state->shadowOffsetY);
}

/*
 * Blur the given surface with the given radius.
 */

void Context2d::blur(cairo_surface_t *surface, int radius)
{
  // Steve Hanov, 2009
  // Released into the public domain.
  radius = radius * 0.57735f + 0.5f;
  // get width, height
  int width = cairo_image_surface_get_width(surface);
  int height = cairo_image_surface_get_height(surface);
  unsigned *precalc = (unsigned *)malloc(width * height * sizeof(unsigned));
  cairo_surface_flush(surface);
  unsigned char *src = cairo_image_surface_get_data(surface);
  double mul = 1.f / ((radius * 2) * (radius * 2));
  int channel;

  // The number of times to perform the averaging. According to wikipedia,
  // three iterations is good enough to pass for a gaussian.
  const int MAX_ITERATIONS = 3;
  int iteration;

  for (iteration = 0; iteration < MAX_ITERATIONS; iteration++)
  {
    for (channel = 0; channel < 4; channel++)
    {
      int x, y;

      // precomputation step.
      unsigned char *pix = src;
      unsigned *pre = precalc;

      pix += channel;
      for (y = 0; y < height; y++)
      {
        for (x = 0; x < width; x++)
        {
          int tot = pix[0];
          if (x > 0)
            tot += pre[-1];
          if (y > 0)
            tot += pre[-width];
          if (x > 0 && y > 0)
            tot -= pre[-width - 1];
          *pre++ = tot;
          pix += 4;
        }
      }

      // blur step.
      pix = src + (int)radius * width * 4 + (int)radius * 4 + channel;
      for (y = radius; y < height - radius; y++)
      {
        for (x = radius; x < width - radius; x++)
        {
          int l = x < radius ? 0 : x - radius;
          int t = y < radius ? 0 : y - radius;
          int r = x + radius >= width ? width - 1 : x + radius;
          int b = y + radius >= height ? height - 1 : y + radius;
          int tot = precalc[r + b * width] + precalc[l + t * width] -
                    precalc[l + b * width] - precalc[r + t * width];
          *pix = (unsigned char)(tot * mul);
          pix += 4;
        }
        pix += (int)radius * 2 * 4;
      }
    }
  }

  cairo_surface_mark_dirty(surface);
  free(precalc);
}

/*
 * Initialize a new Context2d with the given canvas.
 */

// NAN_METHOD(Context2d::New) {
//   if (!info.IsConstructCall()) {
//     return Nan::ThrowTypeError("Class constructors cannot be invoked without 'new'");
//   }

//   if (!info[0]->IsObject())
//     return Nan::ThrowTypeError("Canvas expected");
//   Local<Object> obj = Nan::To<Object>(info[0]).ToLocalChecked();
//   if (!Nan::New(Canvas::constructor)->HasInstance(obj))
//     return Nan::ThrowTypeError("Canvas expected");
//   Canvas *canvas = Napi::ObjectWrap::Unwrap<Canvas>(obj);

//   bool isImageBackend = canvas->backend()->getName() == "image";
//   if (isImageBackend) {
//     cairo_format_t format = ImageBackend::DEFAULT_FORMAT;
//     if (info[1]->IsObject()) {
//       Local<Object> ctxAttributes = Nan::To<Object>(info[1]).ToLocalChecked();

//       Local<Value> pixelFormat = Nan::Get(ctxAttributes, Nan::New("pixelFormat").ToLocalChecked()).ToLocalChecked();
//       if (pixelFormat->IsString()) {
//         Nan::Utf8String utf8PixelFormat(pixelFormat);
//         if (!strcmp(*utf8PixelFormat, "RGBA32")) format = CAIRO_FORMAT_ARGB32;
//         else if (!strcmp(*utf8PixelFormat, "RGB24")) format = CAIRO_FORMAT_RGB24;
//         else if (!strcmp(*utf8PixelFormat, "A8")) format = CAIRO_FORMAT_A8;
//         else if (!strcmp(*utf8PixelFormat, "RGB16_565")) format = CAIRO_FORMAT_RGB16_565;
//         else if (!strcmp(*utf8PixelFormat, "A1")) format = CAIRO_FORMAT_A1;
// #ifdef CAIRO_FORMAT_RGB30
//         else if (!strcmp(utf8PixelFormat, "RGB30")) format = CAIRO_FORMAT_RGB30;
// #endif
//       }

//       // alpha: false forces use of RGB24
//       Local<Value> alpha = Nan::Get(ctxAttributes, Nan::New("alpha").ToLocalChecked()).ToLocalChecked();
//       if (alpha->IsBoolean() && !Nan::To<bool>(alpha).FromMaybe(false)) {
//         format = CAIRO_FORMAT_RGB24;
//       }
//     }
//     static_cast<ImageBackend*>(canvas->backend())->setFormat(format);
//   }

//   Context2d *context = new Context2d(canvas);

//   context->Wrap(info.This());
//   info.GetReturnValue().Set(info.This());
// }

/*
* Save some external modules as private references.
*/

// NAN_METHOD(Context2d::SaveExternalModules) {
//   _DOMMatrix.Reset(Nan::To<Function>(info[0]).ToLocalChecked());
//   _parseFont.Reset(Nan::To<Function>(info[1]).ToLocalChecked());
// }

/*
* Get format (string).
*/

// NAN_GETTER(Context2d::GetFormat) {
//   Context2d *context = Napi::ObjectWrap::Unwrap<Context2d>(info.This());
//   std::string pixelFormatString;
//   switch (context->canvas()->backend()->getFormat()) {
//   case CAIRO_FORMAT_ARGB32: pixelFormatString = "RGBA32"; break;
//   case CAIRO_FORMAT_RGB24: pixelFormatString = "RGB24"; break;
//   case CAIRO_FORMAT_A8: pixelFormatString = "A8"; break;
//   case CAIRO_FORMAT_A1: pixelFormatString = "A1"; break;
//   case CAIRO_FORMAT_RGB16_565: pixelFormatString = "RGB16_565"; break;
// #ifdef CAIRO_FORMAT_RGB30
//   case CAIRO_FORMAT_RGB30: pixelFormatString = "RGB30"; break;
// #endif
//   default: return info.GetReturnValue().SetNull();
//   }
//   info.GetReturnValue().Set(Nan::New<String>(pixelFormatString).ToLocalChecked());
// }

// NAN_METHOD(Context2d::PutImageData)
void Context2d::putImageData(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  //TODO
}

// NAN_METHOD(Context2d::GetImageData)
Napi::Value Context2d::getImageData(const Napi::CallbackInfo &info)
{  
  TRACE_CONTEXT_API

  //TODO
}

// NAN_METHOD(Context2d::CreateImageData)
Napi::Value Context2d::createImageData(const Napi::CallbackInfo &info)
{
    TRACE_CONTEXT_API
  //TODO
  Napi::Env env = info.Env();
  int32_t width, height;

  if (info[0].IsObject())
  {
    //TODO get canvas width/height
    // Local<Object> obj = Nan::To<Object>(info[0]).ToLocalChecked();
    // width = Nan::To<int32_t>(Nan::Get(obj, Nan::New("width").ToLocalChecked()).ToLocalChecked()).FromMaybe(0);
    // height = Nan::To<int32_t>(Nan::Get(obj, Nan::New("height").ToLocalChecked()).ToLocalChecked()).FromMaybe(0);
  }
  else
  {
    width = info[0].As<Napi::Number>().Int32Value();
    height = info[1].As<Napi::Number>().Int32Value();
  }

  int stride = _canvas->stride();
  double Bpp = static_cast<double>(stride) / _canvas->getWidth();
  int nBytes = static_cast<int>(Bpp * width * height + .5);

  Napi::ArrayBuffer ab = Napi::ArrayBuffer::New(env, nBytes);
  Napi::Object arr;

  if (_canvas->backend()->getFormat() == CAIRO_FORMAT_RGB16_565)
    arr = Napi::Uint16Array::New(env, nBytes / 2, ab, 0);
  else
  {
    //TODO  Uint8ClampedArray
    // arr = Napi::Uint8ClampedArray::New(env, nBytes, ab, 0);
  }
  //TODO ImageData NewInstance
}

/*
 * Take a transform matrix and return its components
 * 0: angle, 1: scaleX, 2: scaleY, 3: skewX, 4: translateX, 5: translateY
 */
void decompose_matrix(cairo_matrix_t matrix, double *destination)
{
  double denom = pow(matrix.xx, 2) + pow(matrix.yx, 2);
  destination[0] = atan2(matrix.yx, matrix.xx);
  destination[1] = sqrt(denom);
  destination[2] = (matrix.xx * matrix.yy - matrix.xy * matrix.yx) / destination[1];
  destination[3] = atan2(matrix.xx * matrix.xy + matrix.yx * matrix.yy, denom);
  destination[4] = matrix.x0;
  destination[5] = matrix.y0;
}

/*
 * Draw image src image to the destination (context).
 *
 *  - dx, dy
 *  - dx, dy, dw, dh
 *  - sx, sy, sw, sh, dx, dy, dw, dh
 *
 */

// NAN_METHOD(Context2d::DrawImage) {}
void Context2d::drawImage(const Napi::CallbackInfo &info)
{
    TRACE_CONTEXT_API

  //TODO
}

Napi::Value Context2d::getglobalAlpha(const Napi::CallbackInfo &info)
{
    TRACE_CONTEXT_API

  return Napi::Number::New(info.Env(), state->globalAlpha);
}

void Context2d::setglobalAlpha(const Napi::CallbackInfo &info, const Napi::Value &value)
{
    TRACE_CONTEXT_API

  //TODO check args
  if (info.Length() < 1 || !info[0].IsNumber())
  {
    return;
  }

  double n = info[0].As<Napi::Number>().DoubleValue();
  if (n >= 0 && n <= 1)
  {
    state->globalAlpha = n;
  }
}

Napi::Value Context2d::getglobalCompositeOperation(const Napi::CallbackInfo &info)
{
    TRACE_CONTEXT_API

  cairo_t *ctx = context();
  const char *op = "source-over";
  switch (cairo_get_operator(ctx))
  {
  // composite modes:
  case CAIRO_OPERATOR_CLEAR:
    op = "clear";
    break;
  case CAIRO_OPERATOR_SOURCE:
    op = "copy";
    break;
  case CAIRO_OPERATOR_DEST:
    op = "destination";
    break;
  case CAIRO_OPERATOR_OVER:
    op = "source-over";
    break;
  case CAIRO_OPERATOR_DEST_OVER:
    op = "destination-over";
    break;
  case CAIRO_OPERATOR_IN:
    op = "source-in";
    break;
  case CAIRO_OPERATOR_DEST_IN:
    op = "destination-in";
    break;
  case CAIRO_OPERATOR_OUT:
    op = "source-out";
    break;
  case CAIRO_OPERATOR_DEST_OUT:
    op = "destination-out";
    break;
  case CAIRO_OPERATOR_ATOP:
    op = "source-atop";
    break;
  case CAIRO_OPERATOR_DEST_ATOP:
    op = "destination-atop";
    break;
  case CAIRO_OPERATOR_XOR:
    op = "xor";
    break;
  case CAIRO_OPERATOR_ADD:
    op = "lighter";
    break;
  // blend modes:
  // Note: "source-over" and "normal" are synonyms. Chrome and FF both report
  // "source-over" after setting gCO to "normal".
  // case CAIRO_OPERATOR_OVER: op = "normal";
  case CAIRO_OPERATOR_MULTIPLY:
    op = "multiply";
    break;
  case CAIRO_OPERATOR_SCREEN:
    op = "screen";
    break;
  case CAIRO_OPERATOR_OVERLAY:
    op = "overlay";
    break;
  case CAIRO_OPERATOR_DARKEN:
    op = "darken";
    break;
  case CAIRO_OPERATOR_LIGHTEN:
    op = "lighten";
    break;
  case CAIRO_OPERATOR_COLOR_DODGE:
    op = "color-dodge";
    break;
  case CAIRO_OPERATOR_COLOR_BURN:
    op = "color-burn";
    break;
  case CAIRO_OPERATOR_HARD_LIGHT:
    op = "hard-light";
    break;
  case CAIRO_OPERATOR_SOFT_LIGHT:
    op = "soft-light";
    break;
  case CAIRO_OPERATOR_DIFFERENCE:
    op = "difference";
    break;
  case CAIRO_OPERATOR_EXCLUSION:
    op = "exclusion";
    break;
  case CAIRO_OPERATOR_HSL_HUE:
    op = "hue";
    break;
  case CAIRO_OPERATOR_HSL_SATURATION:
    op = "saturation";
    break;
  case CAIRO_OPERATOR_HSL_COLOR:
    op = "color";
    break;
  case CAIRO_OPERATOR_HSL_LUMINOSITY:
    op = "luminosity";
    break;
  // non-standard:
  case CAIRO_OPERATOR_SATURATE:
    op = "saturate";
    break;
  }

  Napi::Env env = info.Env();
  return Napi::String::New(env, op);
}

void Context2d::setglobalCompositeOperation(const Napi::CallbackInfo &info, const Napi::Value &value)
{
    TRACE_CONTEXT_API

  cairo_t *ctx = context();
  std::string opStr = info[0].As<Napi::String>().Utf8Value();

  const std::map<std::string, cairo_operator_t> blendmodes = {
      // composite modes:
      {"clear", CAIRO_OPERATOR_CLEAR},
      {"copy", CAIRO_OPERATOR_SOURCE},
      {"destination", CAIRO_OPERATOR_DEST}, // this seems to have been omitted from the spec
      {"source-over", CAIRO_OPERATOR_OVER},
      {"destination-over", CAIRO_OPERATOR_DEST_OVER},
      {"source-in", CAIRO_OPERATOR_IN},
      {"destination-in", CAIRO_OPERATOR_DEST_IN},
      {"source-out", CAIRO_OPERATOR_OUT},
      {"destination-out", CAIRO_OPERATOR_DEST_OUT},
      {"source-atop", CAIRO_OPERATOR_ATOP},
      {"destination-atop", CAIRO_OPERATOR_DEST_ATOP},
      {"xor", CAIRO_OPERATOR_XOR},
      {"lighter", CAIRO_OPERATOR_ADD},
      // blend modes:
      {"normal", CAIRO_OPERATOR_OVER},
      {"multiply", CAIRO_OPERATOR_MULTIPLY},
      {"screen", CAIRO_OPERATOR_SCREEN},
      {"overlay", CAIRO_OPERATOR_OVERLAY},
      {"darken", CAIRO_OPERATOR_DARKEN},
      {"lighten", CAIRO_OPERATOR_LIGHTEN},
      {"color-dodge", CAIRO_OPERATOR_COLOR_DODGE},
      {"color-burn", CAIRO_OPERATOR_COLOR_BURN},
      {"hard-light", CAIRO_OPERATOR_HARD_LIGHT},
      {"soft-light", CAIRO_OPERATOR_SOFT_LIGHT},
      {"difference", CAIRO_OPERATOR_DIFFERENCE},
      {"exclusion", CAIRO_OPERATOR_EXCLUSION},
      {"hue", CAIRO_OPERATOR_HSL_HUE},
      {"saturation", CAIRO_OPERATOR_HSL_SATURATION},
      {"color", CAIRO_OPERATOR_HSL_COLOR},
      {"luminosity", CAIRO_OPERATOR_HSL_LUMINOSITY},
      // non-standard:
      {"saturate", CAIRO_OPERATOR_SATURATE}};
  auto op = blendmodes.find(opStr);
  if (op != blendmodes.end())
    cairo_set_operator(ctx, op->second);
}

//TODO PatternQuality Getter & Setter

//TODO ImageSmoothingEnabled  Getter & Setter

// ShadowOffsetX
Napi::Value Context2d::getshadowOffsetX(const Napi::CallbackInfo &info)
{
    TRACE_CONTEXT_API

  return Napi::Number::New(info.Env(), state->shadowOffsetX);
}

void Context2d::setshadowOffsetX(const Napi::CallbackInfo &info, const Napi::Value &value)
{
    TRACE_CONTEXT_API

  double offsetX = info[0].As<Napi::Number>().DoubleValue();
  state->shadowOffsetX = offsetX;
}

// ShadowOffsetY
Napi::Value Context2d::getshadowOffsetY(const Napi::CallbackInfo &info)
{
    TRACE_CONTEXT_API

  Napi::Number::New(info.Env(), state->shadowOffsetY);
}
void Context2d::setshadowOffsetY(const Napi::CallbackInfo &info, const Napi::Value &value)
{
    TRACE_CONTEXT_API

  double offsetY = info[0].As<Napi::Number>().DoubleValue();
  state->shadowOffsetY = offsetY;
}

// Shadow blur
Napi::Value Context2d::getshadowBlur(const Napi::CallbackInfo &info)
{
    TRACE_CONTEXT_API

  Napi::Number::New(info.Env(), state->shadowBlur);
}

void Context2d::setshadowBlur(const Napi::CallbackInfo &info, const Napi::Value &value)
{
    TRACE_CONTEXT_API

  double blur = info[0].As<Napi::Number>().DoubleValue();
  if (blur >= 0)
  {
    state->shadowBlur = blur;
  }
}

/* TODO 非标准接口
// AntiAlias
Napi::Value Context2d::getantiAlias(const Napi::CallbackInfo &info)
{
  const char *aa;
  switch (cairo_get_antialias(context())) {
    case CAIRO_ANTIALIAS_NONE: aa = "none"; break;
    case CAIRO_ANTIALIAS_GRAY: aa = "gray"; break;
    case CAIRO_ANTIALIAS_SUBPIXEL: aa = "subpixel"; break;
    default: aa = "default";
  }
  return  Napi::String::New(info.Env(), aa);
}

void Context2d::setantiAlias(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  cairo_t *ctx = context();
  std::string opValue = info[0].As<Napi::String>().Utf8Value();
  const char *opStr = opValue.c_str();

  cairo_antialias_t a;
  if (0 == strcmp("none", *str)) {
    a = CAIRO_ANTIALIAS_NONE;
  } else if (0 == strcmp("default", *str)) {
    a = CAIRO_ANTIALIAS_DEFAULT;
  } else if (0 == strcmp("gray", *str)) {
    a = CAIRO_ANTIALIAS_GRAY;
  } else if (0 == strcmp("subpixel", *str)) {
    a = CAIRO_ANTIALIAS_SUBPIXEL;
  } else {
    a = cairo_get_antialias(ctx);
  }
  cairo_set_antialias(ctx, a);
}

// drawing mode
Napi::Value Context2d::gettextDrawingMode(const Napi::CallbackInfo &info)
{
  const char *mode;
  if (state->textDrawingMode == TEXT_DRAW_PATHS) {
    mode = "path";
  } else if (state->textDrawingMode == TEXT_DRAW_GLYPHS) {
    mode = "glyph";
  } else {
    mode = "unknown";
  }
  return Napi::String::New(info.Env(), mode);
}

void Context2d::settextDrawingMode(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  std::string opValue = info[0].As<Napi::String>().Utf8Value();
  const char *opStr = opValue.c_str();
  if (0 == strcmp("path", *opStr)) {
    state->textDrawingMode = TEXT_DRAW_PATHS;
  } else if (0 == strcmp("glyph", *str)) {
    state->textDrawingMode = TEXT_DRAW_GLYPHS;
  }
}

//Quality
Napi::Value Context2d::getquality(const Napi::CallbackInfo &info)
{
  const char *filter;
  switch (cairo_pattern_get_filter(cairo_get_source(context()))) {
    case CAIRO_FILTER_FAST: filter = "fast"; break;
    case CAIRO_FILTER_BEST: filter = "best"; break;
    case CAIRO_FILTER_NEAREST: filter = "nearest"; break;
    case CAIRO_FILTER_BILINEAR: filter = "bilinear"; break;
    default: filter = "good";
  }
  return Napi::String::New(info.Env(), filter);
}
void Context2d::setquality(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  std::string opValue = info[0].As<Napi::String>().Utf8Value();
  const char *opStr = opValue.c_str();

  cairo_filter_t filter;
  if (0 == strcmp("fast", *str)) {
    filter = CAIRO_FILTER_FAST;
  } else if (0 == strcmp("best", *str)) {
    filter = CAIRO_FILTER_BEST;
  } else if (0 == strcmp("nearest", *str)) {
    filter = CAIRO_FILTER_NEAREST;
  } else if (0 == strcmp("bilinear", *str)) {
    filter = CAIRO_FILTER_BILINEAR;
  } else {
    filter = CAIRO_FILTER_GOOD;
  }
  cairo_pattern_set_filter(cairo_get_source(context()), filter);
}
*/

//TODO CurrentTransform
// Napi::Value Context2d::getcurrentTransform(const Napi::CallbackInfo &info)
// {
//   Napi::Value v;
//   return v;
// }

// void Context2d::setcurrentTransform(const Napi::CallbackInfo &info, const Napi::Value &value)
// {
//   //TODO
// }

// TODO FillStyle
Napi::Value Context2d::getFillStyle(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API

  Napi::Value style;
  if (_fillStyle.IsEmpty())
  {
    style = _getFillColor(info);
  }
  else
  {
    style = _fillStyle.Value();
  }
  return style;
}
void Context2d::setFillStyle(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  TRACE_CONTEXT_API
  if (value.IsString())
  {    
    _fillStyle.Reset();
    _setFillColor(value);
  }
  else if( value.IsObject() )
  {
    Napi::Object object = value.As<Napi::Object>();
    Napi::Value name = object.Get("name");
    if (!name.IsString())
    {
      // throwError(info, "wrong argument for fillstyle");
      return;
    }
    std::string namePropetry = name.As<Napi::String>().Utf8Value();
    if (namePropetry == "linearGradient" ||  namePropetry == "radialGradient")
    {
      Gradient *grad = Napi::ObjectWrap<Gradient>::Unwrap(object);
      state->fillGradient = grad->pattern();
    }
    else if(namePropetry == "pattern")
    {
      Pattern *pattern = Napi::ObjectWrap<Pattern>::Unwrap(object);
      state->fillPattern = pattern->pattern();
    }
    else
    {

    }
  }
}

Napi::Value Context2d::getstrokeStyle(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API

  Napi::Value style;
  if (_strokeStyle.IsEmpty())
  {
    style = _getStrokeColor(info);
  }
  else
  {
    style = _strokeStyle.Value();
  }
  return style;
}
void Context2d::setstrokeStyle(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  TRACE_CONTEXT_API

  if (value.IsString())
  {    
    _strokeStyle.Reset();
    _setStrokeColor(value);
  }
  else
  {
    //TODO Pattern & Gradient
    // if (Nan::New(Gradient::constructor)->HasInstance(obj)){
    //   Gradient *grad = Napi::ObjectWrap::Unwrap<Gradient>(obj);
    //   context->state->strokeGradient = grad->pattern();
    // } else if(Nan::New(Pattern::constructor)->HasInstance(obj)){
    //   Pattern *pattern = Napi::ObjectWrap::Unwrap<Pattern>(obj);
    //   context->state->strokePattern = pattern->pattern();
    // } else {
    //   return Nan::ThrowTypeError("Gradient or Pattern expected");
    // }
  }
}

Napi::Value Context2d::getmiterLimit(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API

  double value = cairo_get_miter_limit(context());
  return Napi::Number::New(info.Env(), value);
}
void Context2d::setmiterLimit(const Napi::CallbackInfo &info, const Napi::Value &value)
{
   TRACE_CONTEXT_API

  double v = info[0].As<Napi::Number>().DoubleValue();
  if (v > 0)
  {
    cairo_set_miter_limit(_context, v);
  }
}

// LineWidth
Napi::Value Context2d::getlineWidth(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API

  double v = cairo_get_line_width(context());
  return Napi::Number::New(info.Env(), v);
}
void Context2d::setlineWidth(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  TRACE_CONTEXT_API

  double v = info[0].As<Napi::Number>().DoubleValue();
  if (v > 0 && v != std::numeric_limits<double>::infinity())
  {
    cairo_set_line_width(_context, v);
  }
}

//Line Join
Napi::Value Context2d::getlineJoin(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API

  const char *join;
  switch (cairo_get_line_join(context()))
  {
  case CAIRO_LINE_JOIN_BEVEL:
    join = "bevel";
    break;
  case CAIRO_LINE_JOIN_ROUND:
    join = "round";
    break;
  default:
    join = "miter";
  } 
  return Napi::String::New(info.Env(), join);
}

void Context2d::setlineJoin(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  TRACE_CONTEXT_API

  std::string v = info[0].As<Napi::String>().Utf8Value();
  const char *type = v.c_str();
  cairo_t *ctx = context();

  if (0 == strcmp("round", type))
  {
    cairo_set_line_join(ctx, CAIRO_LINE_JOIN_ROUND);
  }
  else if (0 == strcmp("bevel", type))
  {
    cairo_set_line_join(ctx, CAIRO_LINE_JOIN_BEVEL);
  }
  else
  {
    cairo_set_line_join(ctx, CAIRO_LINE_JOIN_MITER);
  }
}

//LineCap
Napi::Value Context2d::getlineCap(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  const char *cap;
  switch (cairo_get_line_cap(context()))
  {
  case CAIRO_LINE_CAP_ROUND:
    cap = "round";
    break;
  case CAIRO_LINE_CAP_SQUARE:
    cap = "square";
    break;
  default:
    cap = "butt";
  }
  return Napi::String::New(info.Env(), cap);
}
void Context2d::setlineCap(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  TRACE_CONTEXT_API
  
  std::string v = info[0].As<Napi::String>().Utf8Value();
  const char *type = v.c_str();
  cairo_t *ctx = context();

  if (0 == strcmp("round", type))
  {
    cairo_set_line_cap(ctx, CAIRO_LINE_CAP_ROUND);
  }
  else if (0 == strcmp("square", type))
  {
    cairo_set_line_cap(ctx, CAIRO_LINE_CAP_SQUARE);
  }
  else
  {
    cairo_set_line_cap(ctx, CAIRO_LINE_CAP_BUTT);
  }
}

//TODO  IsPointInPath
Napi::Value Context2d::getshadowColor(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  char buf[64];
  rgba_to_string(state->shadow, buf, sizeof(buf));
  return Napi::String::New(info.Env(), buf);
}
void Context2d::setshadowColor(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  TRACE_CONTEXT_API
  
  std::string v = info[0].As<Napi::String>().Utf8Value();
  short ok;
  uint32_t rgba = rgba_from_string(v.c_str(), &ok);
  if (ok)
  {
    state->shadow = rgba_create(rgba);
  }
}

/*
 * Set fill color, used internally for fillStyle=
 */
void Context2d::_setFillColor( const Napi::Value &value)
{
  short ok;
  std::string str = value.As<Napi::String>().Utf8Value();
  uint32_t rgba = rgba_from_string(str.c_str(), &ok);
  if (!ok)
    return;
  state->fillPattern = state->fillGradient = NULL;
  state->fill = rgba_create(rgba);
}
/*
 * Get fill color.
 */
Napi::Value Context2d::_getFillColor(const Napi::CallbackInfo &info)
{
  char buf[64];
  rgba_to_string(state->fill, buf, sizeof(buf));
  return Napi::String::New(info.Env(), buf);
}

/*
 * Set stroke color, used internally for strokeStyle=
 */

void Context2d::_setStrokeColor(const Napi::Value &value)
{
  short ok;
  std::string str = value.As<Napi::String>().Utf8Value();
  uint32_t rgba = rgba_from_string(str.c_str(), &ok);
  if (!ok)
    return;
  state->strokePattern = state->strokeGradient = NULL;
  state->stroke = rgba_create(rgba);
}

/*
 * Get stroke color.
 */

Napi::Value Context2d::_getStrokeColor(const Napi::CallbackInfo &info)
{
  char buf[64];
  rgba_to_string(state->stroke, buf, sizeof(buf));
  return Napi::String::New(info.Env(), buf);
}

//TODO createPattern
Napi::Value Context2d::createPattern(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  //TODO
}

Napi::Value Context2d::createLinearGradient(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API  
  if( info.Length() < 4  ){
    return info.Env().Undefined();
  }
  return Gradient::NewInstance(info,  info[0], info[1], info[2], info[3]);
}

Napi::Value Context2d::createRadialGradient(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  if( info.Length() < 6  ){
    return  info.Env().Undefined();
  }
  return Gradient::NewInstance(info,  info[0], info[1], info[2], info[3], info[5], info[6]);
}

/*
 * Bezier curve.
 */

void Context2d::bezierCurveTo(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  double args[6];
  if (!checkArgs(info, args, 6))
    return;

  cairo_curve_to(context(), args[0], args[1], args[2], args[3], args[4], args[5]);
}

/*
 * Quadratic curve approximation from libsvg-cairo.
 */

void Context2d::quadraticCurveTo(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  double args[4];
  if (!checkArgs(info, args, 4))
    return;

  cairo_t *ctx = context();
  double x, y, x1 = args[0], y1 = args[1], x2 = args[2], y2 = args[3];

  cairo_get_current_point(ctx, &x, &y);

  if (0 == x && 0 == y)
  {
    x = x1;
    y = y1;
  }

  cairo_curve_to(ctx, x + 2.0 / 3.0 * (x1 - x), y + 2.0 / 3.0 * (y1 - y), x2 + 2.0 / 3.0 * (x1 - x2), y2 + 2.0 / 3.0 * (y1 - y2), x2, y2);
}

/*
 * Save state.
 */

void Context2d::save(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  save();
}

/*
 * Restore state.
 */

void Context2d::restore(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  restore();
}

/*
 * Creates a new subpath.
 */
void Context2d::beginPath(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  cairo_new_path(context());
}

/*
 * Marks the subpath as closed.
 */

void Context2d::closePath(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  cairo_close_path(context());
}

/*
 * Rotate transformation.
 */

void Context2d::rotate(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  double args[1];
  if (!checkArgs(info, args, 1))
    return;

  args[0] = info[0].As<Napi::Number>().DoubleValue();
  cairo_rotate(context(), args[0]);
}

/*
 * Modify the CTM.
 */

void Context2d::transform(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  double args[6];
  if (!checkArgs(info, args, 6))
    return;

  cairo_matrix_t matrix;
  cairo_matrix_init(&matrix, args[0], args[1], args[2], args[3], args[4], args[5]);

  cairo_transform(context(), &matrix);
}

/*
 * Reset the CTM, used internally by setTransform().
 */

void Context2d::resetTransform(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  cairo_identity_matrix(context());
}

/*
 * Reset transform matrix to identity, then apply the given args.
 */

void Context2d::setTransform(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  cairo_identity_matrix(context());
  transform(info);
}

/*
 * Translate transformation.
 */

void Context2d::translate(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  double args[2];
  if (!checkArgs(info, args, 2))
    return;

  cairo_translate(context(), args[0], args[1]);
}

/*
 * Scale transformation.
 */
void Context2d::scale(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  double args[2];
  if (!checkArgs(info, args, 2))
    return;

  cairo_scale(context(), args[0], args[1]);
}

/*
 * Use path as clipping region.
 */

void Context2d::clip(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  if (info.Length() < 1)
  {
    return;
  }

  setFillRule(info[0]);
  cairo_t *ctx = context();
  cairo_clip_preserve(ctx);
}

/*
 * Fill the path.
 */

void Context2d::fill(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  if (info.Length() < 1)
  {
    return;
  }
  setFillRule(info[0]);
  fill(true);
}

/*
 * Stroke the path.
 */

void Context2d::stroke(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  stroke(true);
}

/*
 * Helper for fillText/strokeText
 */

double
get_text_scale(PangoLayout *layout, double maxWidth)
{

  PangoRectangle logical_rect;
  pango_layout_get_pixel_extents(layout, NULL, &logical_rect);

  if (logical_rect.width > maxWidth)
  {
    return maxWidth / logical_rect.width;
  }
  else
  {
    return 1.0;
  }
}

void Context2d::paintText(const Napi::CallbackInfo &info, bool stroke)
{
  int argsNum = info.Length() >= 4 ? 3 : 2;

  if (argsNum == 3 && info[3].IsUndefined())
    argsNum = 2;

  double args[3];
  if(!checkArgs(info, args, argsNum, 1))
    return;

  std::string str = info[0].As<Napi::String>().Utf8Value();
  double x = args[0];
  double y = args[1];
  double scaled_by = 1;

  PangoLayout *layout = Context2d::layout();

  pango_layout_set_text(layout, str.c_str(), -1);
  pango_cairo_update_layout(context(), layout);

  if (argsNum == 3)
  {
    scaled_by = get_text_scale(layout, args[2]);
    cairo_save(context());
    cairo_scale(context(), scaled_by, 1);
  }

  savePath();
  if (state->textDrawingMode == TEXT_DRAW_GLYPHS)
  {
    if (stroke == true)
    {
      Context2d::stroke();
    }
    else
    {
      Context2d::fill();
    }
    setTextPath(x / scaled_by, y);
  }
  else if (state->textDrawingMode == TEXT_DRAW_PATHS)
  {
    setTextPath(x / scaled_by, y);
    if (stroke == true)
    {
      Context2d::stroke();
    }
    else
    {
      Context2d::fill();
    }
  }
  restorePath();
  if (argsNum == 3)
  {
    cairo_restore(context());
  }
}

/*
 * Fill text at (x, y).
 */
void Context2d::fillText(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  paintText(info, false);
}

/*
 * Stroke text at (x ,y).
 */

void Context2d::strokeText(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  paintText(info, true);
}

/*
 * Gets the baseline adjustment in device pixels
 */
inline double getBaselineAdjustment(PangoLayout *layout, short baseline)
{
  PangoRectangle logical_rect;
  pango_layout_line_get_extents(pango_layout_get_line(layout, 0), NULL, &logical_rect);

  double scale = 1.0 / PANGO_SCALE;
  double ascent = scale * pango_layout_get_baseline(layout);
  double descent = scale * logical_rect.height - ascent;

  switch (baseline)
  {
  case TEXT_BASELINE_ALPHABETIC:
    return ascent;
  case TEXT_BASELINE_MIDDLE:
    return (ascent + descent) / 2.0;
  case TEXT_BASELINE_BOTTOM:
    return ascent + descent;
  default:
    return 0;
  }
}

/*
 * Set text path for the string in the layout at (x, y).
 * This function is called by paintText and won't behave correctly
 * if is not called from there.
 * it needs pango_layout_set_text and pango_cairo_update_layout to be called before
 */

void Context2d::setTextPath(double x, double y)
{
  PangoRectangle logical_rect;

  switch (state->textAlignment)
  {
  // center
  case 0:
    pango_layout_get_pixel_extents(_layout, NULL, &logical_rect);
    x -= logical_rect.width / 2;
    break;
  // right
  case 1:
    pango_layout_get_pixel_extents(_layout, NULL, &logical_rect);
    x -= logical_rect.width;
    break;
  }

  y -= getBaselineAdjustment(_layout, state->textBaseline);

  cairo_move_to(_context, x, y);
  if (state->textDrawingMode == TEXT_DRAW_PATHS)
  {
    pango_cairo_layout_path(_context, _layout);
  }
  else if (state->textDrawingMode == TEXT_DRAW_GLYPHS)
  {
    pango_cairo_show_layout(_context, _layout);
  }
}

/*
 * Adds a point to the current subpath.
 */

void Context2d::lineTo(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  double args[2];
  if (!checkArgs(info, args, 2))
    return;

  cairo_line_to(context(), args[0], args[1]);
}

/*
 * Creates a new subpath at the given point.
 */

void Context2d::moveTo(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  double args[2];
  if (!checkArgs(info, args, 2))
    return;

  cairo_move_to(context(), args[0], args[1]);
}

/*
 * Get font.
 */

Napi::Value Context2d::getfont(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  Napi::Value font;
  if (_font.IsEmpty())
    font = Napi::String::New(info.Env(), "10px sans-serif" );
  else
    font = _font.Value();
  return font;
}

/*
 * Set font:
 *   - weight
 *   - style
 *   - size
 *   - unit
 *   - family
 */

//TODO setFont
void Context2d::setfont(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  TRACE_CONTEXT_API
  
  //TODO
  if (!value.IsString())
    return;

  std::string str = value.As<Napi::String>().Utf8Value();
  if (!str.empty())
    return;

  // const int argc = 1;
  // Local<Value> argv[argc] = { value };

  // Local<Value> mparsed = Nan::Call(_parseFont.Get(iso), ctx->Global(), argc, argv).ToLocalChecked();
  // // parseFont returns undefined for invalid CSS font strings
  // if (mparsed->IsUndefined()) return;
  // Local<Object> font = Nan::To<Object>(mparsed).ToLocalChecked();

  // Nan::Utf8String weight(Nan::Get(font, Nan::New("weight").ToLocalChecked()).ToLocalChecked());
  // Nan::Utf8String style(Nan::Get(font, Nan::New("style").ToLocalChecked()).ToLocalChecked());
  // double size = Nan::To<double>(Nan::Get(font, Nan::New("size").ToLocalChecked()).ToLocalChecked()).FromMaybe(0);
  // Nan::Utf8String unit(Nan::Get(font, Nan::New("unit").ToLocalChecked()).ToLocalChecked());
  // Nan::Utf8String family(Nan::Get(font, Nan::New("family").ToLocalChecked()).ToLocalChecked());

  // PangoFontDescription *desc = pango_font_description_copy(state->fontDescription);
  // pango_font_description_free(state->fontDescription);

  // pango_font_description_set_style(desc, Canvas::GetStyleFromCSSString(*style));
  // pango_font_description_set_weight(desc, Canvas::GetWeightFromCSSString(*weight));

  // if (strlen(*family) > 0) pango_font_description_set_family(desc, *family);

  // PangoFontDescription *sys_desc = Canvas::ResolveFontDescription(desc);
  // pango_font_description_free(desc);

  // if (size > 0) pango_font_description_set_absolute_size(sys_desc, size * PANGO_SCALE);

  // state->fontDescription = sys_desc;
  // pango_layout_set_font_description(_layout, sys_desc);

  // _font.Reset(value);
}

/*
 * Get text baseline.
 */

Napi::Value Context2d::gettextBaseline(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  Napi::Value v;
  if (_textBaseline.IsEmpty())
    v = Napi::String::New(info.Env(), "alphabetic");
  else
    v = _textBaseline.Value();
  return v;
}

/*
 * Set text baseline.
 */

void Context2d::settextBaseline(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  TRACE_CONTEXT_API
  
  if (!value.IsString())
    return;

  std::string v = value.As<Napi::String>().Utf8Value();
  const std::map<std::string, int32_t> modes = {
      {"alphabetic", 0},
      {"top", 1},
      {"bottom", 2},
      {"middle", 3},
      {"ideographic", 4},
      {"hanging", 5}};
  auto op = modes.find(v);
  if (op == modes.end())
    return;

  state->textBaseline = op->second;
  //TODO
   _textBaseline.Reset();
  // _textBaseline.Reset(value);
}

/*
 * Get text align.
 */

Napi::Value Context2d::gettextAlign(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  Napi::Value v;
  if (_textAlign.IsEmpty())
    v = Napi::String::New(info.Env(), "start");
  else
    v = _textAlign.Value();
  return v;
}

/*
 * Set text align.
 */

void Context2d::settextAlign(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  TRACE_CONTEXT_API
  
  if (!value.IsString())
    return;

  std::string v = value.As<Napi::String>().Utf8Value();
  const std::map<std::string, int32_t> modes = {
      {"center", 0},
      {"left", -1},
      {"start", -1},
      {"right", 1},
      {"end", 1}};
  auto op = modes.find(v);
  if (op == modes.end())
    return;

  state->textAlignment = op->second;

    //TODO
    _textAlign.Reset();
  // _textAlign.Reset(value);
}

/*
 * Return the given text extents.
 * TODO: Support for:
 * hangingBaseline, ideographicBaseline,
 * fontBoundingBoxAscent, fontBoundingBoxDescent
 */

Napi::Value Context2d::measureText(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  cairo_t *ctx = context();

  std::string str = info[0].As<Napi::String>().Utf8Value();
  Napi::Object obj = Napi::Object::New(info.Env());

  PangoRectangle _ink_rect, _logical_rect;
  float_rectangle ink_rect, logical_rect;
  PangoFontMetrics *metrics;
  PangoLayout *layout = Context2d::layout();

  pango_layout_set_text(layout, str.c_str(), -1);
  pango_cairo_update_layout(ctx, layout);

  // Normally you could use pango_layout_get_pixel_extents and be done, or use
  // pango_extents_to_pixels, but both of those round the pixels, so we have to
  // divide by PANGO_SCALE manually
  pango_layout_get_extents(layout, &_ink_rect, &_logical_rect);

  float inverse_pango_scale = 1. / PANGO_SCALE;

  logical_rect.x = _logical_rect.x * inverse_pango_scale;
  logical_rect.y = _logical_rect.y * inverse_pango_scale;
  logical_rect.width = _logical_rect.width * inverse_pango_scale;
  logical_rect.height = _logical_rect.height * inverse_pango_scale;

  ink_rect.x = _ink_rect.x * inverse_pango_scale;
  ink_rect.y = _ink_rect.y * inverse_pango_scale;
  ink_rect.width = _ink_rect.width * inverse_pango_scale;
  ink_rect.height = _ink_rect.height * inverse_pango_scale;

  metrics = PANGO_LAYOUT_GET_METRICS(layout);

  double x_offset;
  switch (state->textAlignment)
  {
  case 0: // center
    x_offset = logical_rect.width / 2;
    break;
  case 1: // right
    x_offset = logical_rect.width;
    break;
  default: // left
    x_offset = 0.0;
  }

  cairo_matrix_t matrix;
  cairo_get_matrix(ctx, &matrix);
  double y_offset = getBaselineAdjustment(layout, state->textBaseline);

  obj.Set("width", Napi::Number::New(info.Env(), logical_rect.width));
  obj.Set("actualBoundingBoxLeft", Napi::Number::New(info.Env(), x_offset - PANGO_LBEARING(logical_rect)));
  obj.Set("actualBoundingBoxRight", Napi::Number::New(info.Env(), x_offset + PANGO_RBEARING(logical_rect)));
  obj.Set("actualBoundingBoxAscent", Napi::Number::New(info.Env(), y_offset + PANGO_ASCENT(ink_rect)));
  obj.Set("actualBoundingBoxDescent",  Napi::Number::New(info.Env(), PANGO_DESCENT(ink_rect) - y_offset));
  obj.Set("emHeightAscent",  Napi::Number::New(info.Env(), -(PANGO_ASCENT(logical_rect) - y_offset)));
  obj.Set("emHeightDescent",  Napi::Number::New(info.Env(), PANGO_DESCENT(logical_rect) - y_offset));
  obj.Set("alphabeticBaseline",  Napi::Number::New(info.Env(),-(pango_font_metrics_get_ascent(metrics) * inverse_pango_scale - y_offset)));

  pango_font_metrics_unref(metrics);
  return obj;
}

/*
 * Set line dash
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */

//TODO LineDash
void Context2d::setLineDash(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  //TODO Input value
}

// NAN_METHOD(Context2d::SetLineDash) {
//   if (!info[0]->IsArray()) return;
//   Local<Array> dash = Local<Array>::Cast(info[0]);
//   uint32_t dashes = dash->Length() & 1 ? dash->Length() * 2 : dash->Length();
//   uint32_t zero_dashes = 0;
//   std::vector<double> a(dashes);
//   for (uint32_t i=0; i<dashes; i++) {
//     Local<Value> d = Nan::Get(dash, i % dash->Length()).ToLocalChecked();
//     if (!d->IsNumber()) return;
//     a[i] = Nan::To<double>(d).FromMaybe(0);
//     if (a[i] == 0) zero_dashes++;
//     if (a[i] < 0 || isnan(a[i]) || isinf(a[i])) return;
//   }
//   Context2d *context = Napi::ObjectWrap::Unwrap<Context2d>(info.This());
//   cairo_t *ctx = context->context();
//   double offset;
//   cairo_get_dash(ctx, NULL, &offset);
//   if (zero_dashes == dashes) {
//     std::vector<double> b(0);
//     cairo_set_dash(ctx, b.data(), 0, offset);
//   } else {
//     cairo_set_dash(ctx, a.data(), dashes, offset);
//   }
// }

/*
 * Get line dash
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */

Napi::Value Context2d::getLineDash(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  //TODO
}

// NAN_METHOD(Context2d::GetLineDash) {
//   Context2d *context = Napi::ObjectWrap::Unwrap<Context2d>(info.This());
//   cairo_t *ctx = context->context();
//   int dashes = cairo_get_dash_count(ctx);
//   std::vector<double> a(dashes);
//   cairo_get_dash(ctx, a.data(), NULL);

//   Local<Array> dash = Nan::New<Array>(dashes);
//   for (int i=0; i<dashes; i++) {
//     Nan::Set(dash, Nan::New<Number>(i), Nan::New<Number>(a[i])).Check();
//   }

//   info.GetReturnValue().Set(dash);
// }

/*
 * Set line dash offset
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */
void Context2d::setlineDashOffset(const Napi::CallbackInfo &info, const Napi::Value &value)
{
  TRACE_CONTEXT_API
  
  double offset = value.As<Napi::Number>().DoubleValue();
  if (isnan(offset) || isinf(offset))
    return;

  cairo_t *ctx = context();

  int dashes = cairo_get_dash_count(ctx);
  std::vector<double> a(dashes);
  cairo_get_dash(ctx, a.data(), NULL);
  cairo_set_dash(ctx, a.data(), dashes, offset);
}

/*
 * Get line dash offset
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */
Napi::Value Context2d::getlineDashOffset(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  cairo_t *ctx = context();
  double offset;
  cairo_get_dash(ctx, NULL, &offset);
  return Napi::Number::New(info.Env(), offset);
}

/*
 * Fill the rectangle defined by x, y, width and height.
 */

void Context2d::fillRect(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  RECT_ARGS;
  if (0 == width || 0 == height)
    return;
  cairo_t *ctx = context();
  savePath();
  cairo_rectangle(ctx, x, y, width, height);
  fill();
  restorePath();
}

/*
 * Stroke the rectangle defined by x, y, width and height.
 */

void Context2d::strokeRect(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  RECT_ARGS;
  if (0 == width && 0 == height)
    return;
  cairo_t *ctx = context();
  savePath();
  cairo_rectangle(ctx, x, y, width, height);
  stroke();
  restorePath();
}

/*
 * Clears all pixels defined by x, y, width and height.
 */

void Context2d::clearRect(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  RECT_ARGS;
  if (0 == width || 0 == height)
    return;
  cairo_t *ctx = context();
  cairo_save(ctx);
  savePath();
  cairo_rectangle(ctx, x, y, width, height);
  cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
  cairo_fill(ctx);
  restorePath();
  cairo_restore(ctx);
}

/*
 * Adds a rectangle subpath.
 */

void Context2d::rect(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  RECT_ARGS;
  cairo_t *ctx = context();
  if (width == 0)
  {
    cairo_move_to(ctx, x, y);
    cairo_line_to(ctx, x, y + height);
  }
  else if (height == 0)
  {
    cairo_move_to(ctx, x, y);
    cairo_line_to(ctx, x + width, y);
  }
  else
  {
    cairo_rectangle(ctx, x, y, width, height);
  }
}

/*
 * Adds an arc at x, y with the given radis and start/end angles.
 */
void Context2d::arc(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
if (!info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() || !info[3].IsNumber() || !info[4].IsNumber())
    return;

  bool anticlockwise = false;
  if (info.Length() == 6)
  {
    anticlockwise = info[5].As<Napi::Boolean>().ToBoolean();
  }

anticlockwise =false;
  cairo_t *ctx = context();
  if (anticlockwise && M_PI * 2 !=  info[4].As<Napi::Number>().DoubleValue()  )
  {
    cairo_arc_negative(ctx, info[0].As<Napi::Number>().DoubleValue(), info[1].As<Napi::Number>().DoubleValue(), info[2].As<Napi::Number>().DoubleValue(), info[3].As<Napi::Number>().DoubleValue(), info[4].As<Napi::Number>().DoubleValue());
  }
  else
  {
    cairo_arc(ctx, info[0].As<Napi::Number>().DoubleValue(), info[1].As<Napi::Number>().DoubleValue(), info[2].As<Napi::Number>().DoubleValue(), info[3].As<Napi::Number>().DoubleValue(), info[4].As<Napi::Number>().DoubleValue());
  }
}

/*
 * Ad, const Napi::Value &valueds an arcTo point (x0,y0) to (x1,y1) with the given radius.
 *
 * Implementation influenced by WebKit.
 */

void Context2d::arcTo(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  double args[5];
  if (!checkArgs(info, args, 5))
    return;

  cairo_t *ctx = context();

  // Current path point
  double x, y;
  cairo_get_current_point(ctx, &x, &y);
  Point<float> p0(x, y);

  // Point (x0,y0)
  Point<float> p1(args[0], args[1]);

  // Point (x1,y1)
  Point<float> p2(args[2], args[3]);

  float radius = args[4];

  if ((p1.x == p0.x && p1.y == p0.y) || (p1.x == p2.x && p1.y == p2.y) || radius == 0.f)
  {
    cairo_line_to(ctx, p1.x, p1.y);
    return;
  }

  Point<float> p1p0((p0.x - p1.x), (p0.y - p1.y));
  Point<float> p1p2((p2.x - p1.x), (p2.y - p1.y));
  float p1p0_length = sqrtf(p1p0.x * p1p0.x + p1p0.y * p1p0.y);
  float p1p2_length = sqrtf(p1p2.x * p1p2.x + p1p2.y * p1p2.y);

  double cos_phi = (p1p0.x * p1p2.x + p1p0.y * p1p2.y) / (p1p0_length * p1p2_length);
  // all points on a line logic
  if (-1 == cos_phi)
  {
    cairo_line_to(ctx, p1.x, p1.y);
    return;
  }

  if (1 == cos_phi)
  {
    // add infinite far away point
    unsigned int max_length = 65535;
    double factor_max = max_length / p1p0_length;
    Point<float> ep((p0.x + factor_max * p1p0.x), (p0.y + factor_max * p1p0.y));
    cairo_line_to(ctx, ep.x, ep.y);
    return;
  }

  float tangent = radius / tan(acos(cos_phi) / 2);
  float factor_p1p0 = tangent / p1p0_length;
  Point<float> t_p1p0((p1.x + factor_p1p0 * p1p0.x), (p1.y + factor_p1p0 * p1p0.y));

  Point<float> orth_p1p0(p1p0.y, -p1p0.x);
  float orth_p1p0_length = sqrt(orth_p1p0.x * orth_p1p0.x + orth_p1p0.y * orth_p1p0.y);
  float factor_ra = radius / orth_p1p0_length;

  double cos_alpha = (orth_p1p0.x * p1p2.x + orth_p1p0.y * p1p2.y) / (orth_p1p0_length * p1p2_length);
  if (cos_alpha < 0.f)
    orth_p1p0 = Point<float>(-orth_p1p0.x, -orth_p1p0.y);

  Point<float> p((t_p1p0.x + factor_ra * orth_p1p0.x), (t_p1p0.y + factor_ra * orth_p1p0.y));

  orth_p1p0 = Point<float>(-orth_p1p0.x, -orth_p1p0.y);
  float sa = acos(orth_p1p0.x / orth_p1p0_length);
  if (orth_p1p0.y < 0.f)
    sa = 2 * M_PI - sa;

  bool anticlockwise = false;

  float factor_p1p2 = tangent / p1p2_length;
  Point<float> t_p1p2((p1.x + factor_p1p2 * p1p2.x), (p1.y + factor_p1p2 * p1p2.y));
  Point<float> orth_p1p2((t_p1p2.x - p.x), (t_p1p2.y - p.y));
  float orth_p1p2_length = sqrtf(orth_p1p2.x * orth_p1p2.x + orth_p1p2.y * orth_p1p2.y);
  float ea = acos(orth_p1p2.x / orth_p1p2_length);

    if (orth_p1p2.y < 0)
    ea = 2 * M_PI - ea;
  if ((sa > ea) && ((sa - ea) < M_PI))
    anticlockwise = true;
  if ((sa < ea) && ((ea - sa) > M_PI))
    anticlockwise = true;

  cairo_line_to(ctx, t_p1p0.x, t_p1p0.y);

  if (anticlockwise && M_PI * 2 != radius)
  {
    cairo_arc_negative(ctx, p.x, p.y, radius, sa, ea);
  }
  else
  {
    cairo_arc(ctx, p.x, p.y, radius, sa, ea);
  }
}
/*
 * Adds an ellipse to the path which is centered at (x, y) position with the
 * radii radiusX and radiusY starting at startAngle and ending at endAngle
 * going in the given direction by anticlockwise (defaulting to clockwise).
 */
void Context2d::ellipse(const Napi::CallbackInfo &info)
{
  TRACE_CONTEXT_API
  
  double args[7];
  if (!checkArgs(info, args, 7))
    return;

  double radiusX = args[2];
  double radiusY = args[3];

  if (radiusX == 0 || radiusY == 0)
    return;

  double x = args[0];
  double y = args[1];
  double rotation = args[4];
  double startAngle = args[5];
  double endAngle = args[6];

  bool anticlockwise = info[7].As<Napi::Boolean>().ToBoolean();
  cairo_t *ctx = context();

  // See https://www.cairographics.org/cookbook/ellipses/
  double xRatio = radiusX / radiusY;

  cairo_matrix_t save_matrix;
  cairo_get_matrix(ctx, &save_matrix);
  cairo_translate(ctx, x, y);
  cairo_rotate(ctx, rotation);
  cairo_scale(ctx, xRatio, 1.0);
  cairo_translate(ctx, -x, -y);
  if (anticlockwise && M_PI * 2 != args[4])
  {
    cairo_arc_negative(ctx,
                       x,
                       y,
                       radiusY,
                       startAngle,
                       endAngle);
  }
  else
  {
    cairo_arc(ctx,
              x,
              y,
              radiusY,
              startAngle,
              endAngle);
  }
  cairo_set_matrix(ctx, &save_matrix);
}

}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <string.h>

#include <vector>
#include <queue>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/media_stream_video_track.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/cpp/video_frame.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/input_event.h"

// When compiling natively on Windows, PostMessage can be #define-d to
// something else.
#ifdef PostMessage
#undef PostMessage
#endif

// Assert |context_| isn't holding any GL Errors.  Done as a macro instead of a
// function to preserve line number information in the failure message.
#define AssertNoGLError() \
  PP_DCHECK(!glGetError());

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
}

struct QueueItem {
	uint8_t* data;
	int size;
};

namespace {

// This object is the global object representing this plugin library as long
// as it is loaded.
class ClientInstance : public pp::Module {
 public:
  ClientInstance() : pp::Module() {}
  virtual ~ClientInstance() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance);
};

class VideoDecodePack : public pp::Instance,
                        public pp::Graphics3DClient {
 public:
	 AVCodec *codec;
     AVCodecContext *context;
     int frame, got_picture, len;
     AVFrame *picture;
     char buf[1024];
     AVPacket avpkt;
     int nbFrames;
     std::queue<QueueItem> input_queue;
     bool is_decoding;
     
  VideoDecodePack(PP_Instance instance, pp::Module* module);
  virtual ~VideoDecodePack();

  // pp::Instance implementation (see PPP_Instance).
  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& clip_ignored);
  virtual void HandleMessage(const pp::Var& message_data);
  
  virtual void decode(uint8_t* data,int size);

  virtual void checkDecode();
  
  // pp::Graphics3DClient implementation.
  virtual void Graphics3DContextLost() {
	
  }

 private:
  void DrawYUV();
  void Render();

  // GL-related functions.
  void InitGL();
  GLuint CreateTexture(int32_t width, int32_t height, int unit, bool rgba);
  void CreateGLObjects();
  void CreateShader(GLuint program, GLenum type, const char* source);
  void PaintFinished(int32_t result);
  void CreateTextures();

  pp::Size position_size_;
  bool is_painting_;
  bool needs_paint_;
  GLuint program_yuv_;
  GLuint program_rgb_;
  GLuint buffer_;
  GLuint texture_y_;
  GLuint texture_u_;
  GLuint texture_v_;
  pp::CompletionCallbackFactory<VideoDecodePack> callback_factory_;

  // Owned data.
  pp::Graphics3D* context_;

  pp::Size frame_size_;
};

VideoDecodePack::VideoDecodePack(
    PP_Instance instance, pp::Module* module)
    : pp::Instance(instance),
      pp::Graphics3DClient(this),
      is_painting_(false),
      needs_paint_(false),
      texture_y_(0),
      texture_u_(0),
      texture_v_(0),
      callback_factory_(this),
      context_(NULL) {
		  
	  if (!glInitializePPAPI(pp::Module::Get()->get_browser_interface())) {
		LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Unable to initialize GL PPAPI!"));
		assert(false);
	  }
  
	 this->is_decoding = false;
	 this->nbFrames = 0;
	 
	 // inits packet
     av_init_packet(&this->avpkt);
     
     // registers codecs
     avcodec_register_all();

	 // find the h264 video decoder 
     this->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
     
     if (!this->codec) {
         LogToConsole(PP_LOGLEVEL_ERROR, "codec not found");
         exit(1);
     } else {
		LogToConsole(PP_LOGLEVEL_LOG, "codec Found !");
	 }
	 
	 this->context = avcodec_alloc_context3(this->codec);
	 
	 LogToConsole(PP_LOGLEVEL_LOG, "context initialized");

	 this->picture= av_frame_alloc();
	 
	 LogToConsole(PP_LOGLEVEL_LOG, "Frame allocated");
	 
	 if(this->codec->capabilities&CODEC_CAP_TRUNCATED) {
      // we do not send complete frames 
         this->context->flags|= CODEC_FLAG_TRUNCATED; 
	 }
	 
	 if (avcodec_open2(this->context, this->codec,0) < 0) {
        LogToConsole(PP_LOGLEVEL_ERROR, "could not open codec");
        exit(1);
     }
     
	 LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("InitGL.."));  
	 InitGL();
	 LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Init Textures.."));  
	 CreateTextures();
	 LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Rendering.."));  
	 Render();
    
     CreateTextures();
}

VideoDecodePack::~VideoDecodePack() {
  delete context_;
}

void VideoDecodePack::DidChangeView(
    const pp::Rect& position, const pp::Rect& clip_ignored) {
  if (position.width() == 0 || position.height() == 0)
    return;
  if (position.size() == position_size_)
    return;

  position_size_ = position.size();

  // Initialize graphics.
  InitGL();
  Render();
}


void  VideoDecodePack::checkDecode() {
	if(!this->is_decoding) {
		this->is_decoding = true;
		QueueItem item;
		while(!input_queue.empty()) {
			item = input_queue.front();
			decode(item.data,item.size);
			input_queue.pop();
		}
		this->is_decoding = false;
	}
}

void VideoDecodePack::decode(uint8_t* data,int size) {
	this->avpkt.data = data;
	this->avpkt.size = size;
	
	len = avcodec_decode_video2(this->context, this->picture, &this->got_picture, &this->avpkt);
	
	if (len < 0) {
		 LogToConsole(PP_LOGLEVEL_ERROR, "Error while decoding frame");
		 //exit(1);
	 }
	 if (got_picture) {
        //memset(this->avpkt.data, 0, 0);
        std::string result = "Frame decoded: "+ std::to_string(this->picture->width)+ "x" + std::to_string(this->picture->height) ;
        
		pp::VarDictionary dictionary;
		dictionary.Set(pp::Var("nbFrames"), (++this->nbFrames));
		PostMessage(dictionary);

		pp::Size size(this->picture->width,this->picture->height);

		if (size != frame_size_) {
			frame_size_ = size;
			CreateTextures();
		}

		int32_t width = frame_size_.width();
		int32_t height = frame_size_.height();

		result = "Size Y: "+ std::to_string(width) +"x"+ std::to_string(height);	
		//LogToConsole(PP_LOGLEVEL_LOG, result);
		
		/**
		 * glTexSubImage2D() is used to replace parts or all of a texture that already has image data allocated. 
		 * You have to call glTexImage2D() on the texture at least once before you can use glTexSubImage2D() on it. 
		 * Unlike glTexSubImage2D(), glTexImage2D() allocates image data. You can use NULL for the last (data) argument 
		 * to glTexImage2D() if you only want to allocate image data, and later set the data with glTexSubImage2D().
		 *
		 * Newer versions of OpenGL (4.4 and ES 3.0) have a new entry point glTexStorage2D() that can be used as an
		 * alternative to glTexImage2D() to allocate the image data for a texture without specifying the data.
		 * It is similar to calling glTexImage2D() with data = NULL, but also allows specifying ahead of time 
		 * if space for mipmaps will be needed.
		*/
		
		glActiveTexture(GL_TEXTURE0);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,GL_LUMINANCE, GL_UNSIGNED_BYTE, this->picture->data[0]);

		glActiveTexture(GL_TEXTURE1);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2,GL_LUMINANCE, GL_UNSIGNED_BYTE, this->picture->data[1]);
		
		glActiveTexture(GL_TEXTURE2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2,GL_LUMINANCE, GL_UNSIGNED_BYTE, this->picture->data[2]);

		if (is_painting_) {
			needs_paint_ = true;
		} else {
			Render();
		}
	}
}

void VideoDecodePack::HandleMessage(const pp::Var& var_message) {
  //LogToConsole(PP_LOGLEVEL_LOG, "Got compressed frame");
	    
	if (!var_message.is_array_buffer()) return;
	
	pp::VarArrayBuffer buffer(var_message);
	
	/*QueueItem queue_item;
	queue_item.data = static_cast<uint8_t*>(buffer.Map());
	queue_item.size = buffer.ByteLength();
	input_queue.push(queue_item);
	
	checkDecode();*/
	
	decode(static_cast<uint8_t*>(buffer.Map()),buffer.ByteLength());
}

void VideoDecodePack::InitGL() {
  PP_DCHECK(position_size_.width() && position_size_.height());
  is_painting_ = false;

  delete context_;
  int32_t attributes[] = {
    PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 0,
    PP_GRAPHICS3DATTRIB_BLUE_SIZE, 8,
    PP_GRAPHICS3DATTRIB_GREEN_SIZE, 8,
    PP_GRAPHICS3DATTRIB_RED_SIZE, 8,
    PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 0,
    PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 0,
    PP_GRAPHICS3DATTRIB_SAMPLES, 0,
    PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
    PP_GRAPHICS3DATTRIB_WIDTH, position_size_.width(),
    PP_GRAPHICS3DATTRIB_HEIGHT, position_size_.height(),
    PP_GRAPHICS3DATTRIB_NONE,
  };
  context_ = new pp::Graphics3D(this, attributes);
  PP_DCHECK(!context_->is_null());

  glSetCurrentContextPPAPI(context_->pp_resource());

  glClearColor(1, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(0, 0, position_size_.width(), position_size_.height());

  BindGraphics(*context_);
  AssertNoGLError();

  CreateGLObjects();
}

void VideoDecodePack::DrawYUV() {
  static const float kColorMatrix[9] = {
    1.1643828125f, 1.1643828125f, 1.1643828125f,
    0.0f, -0.39176171875f, 2.017234375f,
    1.59602734375f, -0.81296875f, 0.0f
  };

  glUseProgram(program_yuv_);
  glUniform1i(glGetUniformLocation(program_yuv_, "y_texture"), 0);
  glUniform1i(glGetUniformLocation(program_yuv_, "u_texture"), 1);
  glUniform1i(glGetUniformLocation(program_yuv_, "v_texture"), 2);
  glUniformMatrix3fv(glGetUniformLocation(program_yuv_, "color_matrix"),
      1, GL_FALSE, kColorMatrix);
  AssertNoGLError();

  GLint pos_location = glGetAttribLocation(program_yuv_, "a_position");
  GLint tc_location = glGetAttribLocation(program_yuv_, "a_texCoord");
  AssertNoGLError();
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0,
      static_cast<float*>(0) + 16);  // Skip position coordinates.
  AssertNoGLError();

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  AssertNoGLError();
}

void VideoDecodePack::Render() {
  PP_DCHECK(!is_painting_);
  is_painting_ = true;
  needs_paint_ = false;

  if (texture_y_) {
    DrawYUV();
  } else {
    glClear(GL_COLOR_BUFFER_BIT);
  }
  pp::CompletionCallback cb = callback_factory_.NewCallback(
      &VideoDecodePack::PaintFinished);
  context_->SwapBuffers(cb);
}

void VideoDecodePack::PaintFinished(int32_t result) {
  is_painting_ = false;
  if (needs_paint_)
    Render();
}

GLuint VideoDecodePack::CreateTexture(
    int32_t width, int32_t height, int unit, bool rgba) {
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  AssertNoGLError();

  // Assign parameters.
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Allocate texture.
  glTexImage2D(GL_TEXTURE_2D, 0,
               GL_LUMINANCE,
               width, height, 0,
               GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
  AssertNoGLError();
  return texture_id;
}

void VideoDecodePack::CreateGLObjects() {
  // Code and constants for shader.
  static const char kVertexShader[] =
      "varying vec2 v_texCoord;            \n"
      "attribute vec4 a_position;          \n"
      "attribute vec2 a_texCoord;          \n"
      "void main()                         \n"
      "{                                   \n"
      "    v_texCoord = a_texCoord;        \n"
      "    gl_Position = a_position;       \n"
      "}";

  static const char kFragmentShaderYUV[] =
      "precision mediump float;                                   \n"
      "varying vec2 v_texCoord;                                   \n"
      "uniform sampler2D y_texture;                               \n"
      "uniform sampler2D u_texture;                               \n"
      "uniform sampler2D v_texture;                               \n"
      "uniform mat3 color_matrix;                                 \n"
      "void main()                                                \n"
      "{                                                          \n"
      "  vec3 yuv;                                                \n"
      "  yuv.x = texture2D(y_texture, v_texCoord).r;              \n"
      "  yuv.y = texture2D(u_texture, v_texCoord).r;              \n"
      "  yuv.z = texture2D(v_texture, v_texCoord).r;              \n"
      "  vec3 rgb = color_matrix * (yuv - vec3(0.0625, 0.5, 0.5));\n"
      "  gl_FragColor = vec4(rgb, 1.0);                           \n"
      "}";

  static const char kFragmentShaderRGB[] =
      "precision mediump float;                                   \n"
      "varying vec2 v_texCoord;                                   \n"
      "uniform sampler2D rgb_texture;                             \n"
      "void main()                                                \n"
      "{                                                          \n"
      "  gl_FragColor = texture2D(rgb_texture, v_texCoord);       \n"
      "}";

  // Create shader programs.
  program_yuv_ = glCreateProgram();
  CreateShader(program_yuv_, GL_VERTEX_SHADER, kVertexShader);
  CreateShader(program_yuv_, GL_FRAGMENT_SHADER, kFragmentShaderYUV);
  glLinkProgram(program_yuv_);
  AssertNoGLError();

  program_rgb_ = glCreateProgram();
  CreateShader(program_rgb_, GL_VERTEX_SHADER, kVertexShader);
  CreateShader(program_rgb_, GL_FRAGMENT_SHADER, kFragmentShaderRGB);
  glLinkProgram(program_rgb_);
  AssertNoGLError();

  // Assign vertex positions and texture coordinates to buffers for use in
  // shader program.
  static const float kVertices[] = {
    -1, 1, -1, -1, 0, 1, 0, -1,  // Position coordinates.
    0, 1, 0, -1, 1, 1, 1, -1,  // Position coordinates.
    0, 0, 0, 1, 1, 0, 1, 1,  // Texture coordinates.
    0, 0, 0, 1, 1, 0, 1, 1,  // Texture coordinates.
  };

  glGenBuffers(1, &buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);
  AssertNoGLError();
}

void VideoDecodePack::CreateShader(
    GLuint program, GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  GLint length = strlen(source) + 1;
  glShaderSource(shader, 1, &source, &length);
  glCompileShader(shader);
  glAttachShader(program, shader);
  glDeleteShader(shader);
}

void VideoDecodePack::CreateTextures() {
  int32_t width = frame_size_.width();
  int32_t height = frame_size_.height();
  if (width == 0 || height == 0)
    return;
    
  glDeleteTextures(1, &texture_y_);
  glDeleteTextures(1, &texture_u_);
  glDeleteTextures(1, &texture_v_);
    
  texture_y_ = CreateTexture(width, height, 0, false);
  texture_u_ = CreateTexture(width / 2, height / 2, 1, false);
  texture_v_ = CreateTexture(width / 2, height / 2, 2, false);
}

pp::Instance* ClientInstance::CreateInstance(PP_Instance instance) {
  return new VideoDecodePack(instance, this);
}

}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new ClientInstance();
}
}  // namespace pp

#include "pch.h"
#include "FFMpegDecoder.h"
#include "StatsRenderer.h"

#include <Common\DirectXHelper.h>
#include <d3d11_1.h>
#include "Utils.hpp"
#include "moonlight_xbox_dxMain.h"
#include <gamingdeviceinformation.h>

extern "C" {
#include "Limelight.h"
#include <third_party\h264bitstream\h264_stream.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/time.h>
}
#define DECODER_BUFFER_SIZE 1048576

namespace moonlight_xbox_dx {

	void lock_context(void* dec) {
		auto ff = (FFMpegDecoder*)dec;
		ff->mutex.lock();
	}

	void unlock_context(void* dec) {
		auto ff = (FFMpegDecoder*)dec;
		ff->mutex.unlock();
	}

	enum AVPixelFormat ffGetFormat(AVCodecContext* context, const enum AVPixelFormat* pixFmts)
	{
		FFMpegDecoder* d = ((FFMpegDecoder*)context->opaque);
		const enum AVPixelFormat* p;

		for (p = pixFmts; *p != -1; p++) {
			// Only match our hardware decoding codec or preferred SW pixel
			// format (if not using hardware decoding). It's crucial
			// to override the default get_format() which will try
			// to gracefully fall back to software decode and break us.
			if (*p == AV_PIX_FMT_D3D11) {
				return *p;
			}
		}



		return AV_PIX_FMT_NONE;
	}

	void ffmpeg_log_callback(void* avcl, int	level, const char* fmt, va_list vl) {
		if (level > AV_LOG_INFO) return;

		char message[2048];
		vsprintf_s(message, fmt, vl);
		OutputDebugStringA("[FFMPEG]");
		OutputDebugStringA(message);
		if (level <= AV_LOG_INFO) {
			Utils::Log("[FFMPEG]");
			Utils::Log(message);
			Utils::Log("\n");
		}
	}

	int FFMpegDecoder::Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
		this->videoFormat = videoFormat;
		this->width = width;
		this->height = height;


#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,10,100)
		avcodec_register_all();
#endif
		av_log_set_level(AV_LOG_INFO);
		av_log_set_callback(&ffmpeg_log_callback);
#pragma warning(suppress : 4996)
		av_init_packet(&pkt);

		if (videoFormat & VIDEO_FORMAT_MASK_H264) {
			decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
			Utils::Log("Using H264\n");
		}
		else if (videoFormat & VIDEO_FORMAT_MASK_H265) {
			decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
			Utils::Log("Using HEVC\n");
		}
		else if (videoFormat & VIDEO_FORMAT_MASK_AV1) {
			decoder = avcodec_find_decoder(AV_CODEC_ID_AV1);
			Utils::Log("Using AV1\n");
		}

		if (decoder == NULL) {
			Utils::Log("Couldn't find decoder\n");
			return -1;
		}

		decoder_ctx = avcodec_alloc_context3(decoder);
		if (decoder_ctx == NULL) {
			Utils::Log("Couldn't allocate context");
			return -1;
		}
		decoder_ctx->opaque = this;

		AVBufferRef* hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
		device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
		d3d11va_device_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
		d3d11va_device_ctx->device = this->resources->GetD3DDevice();
		d3d11va_device_ctx->device_context = this->resources->GetD3DDeviceContext();
		d3d11va_device_ctx->lock = lock_context;
		d3d11va_device_ctx->unlock = unlock_context;
		d3d11va_device_ctx->lock_ctx = this;
		int err2;
		if ((err2 = av_hwdevice_ctx_init(hw_device_ctx)) < 0) {
			char msg[2048];
			sprintf(msg, "Failed to create specified DirectX Video device: %d\n", err2);
			Utils::Log(msg);
			return err2;

		}

		decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
		decoder_ctx->pix_fmt = AV_PIX_FMT_D3D11;
		decoder_ctx->sw_pix_fmt = (videoFormat & VIDEO_FORMAT_MASK_10BIT) ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;
		decoder_ctx->width = width;
		decoder_ctx->height = height;
		decoder_ctx->get_format = ffGetFormat;
		
		AVDictionary* opts = nullptr;
		
		if (isXboxSeriesX) {
			decoder_ctx->thread_count = 0;
			decoder_ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
			
			// Increase hardware frames for high bitrate content
			decoder_ctx->extra_hw_frames = (width >= 3840 || height >= 2160) ? 6 : 4;
			
			if (videoFormat & VIDEO_FORMAT_MASK_AV1) {
				av_dict_set(&opts, "threads", "8", 0);
				av_dict_set(&opts, "film_grain", "0", 0);
				av_dict_set(&opts, "tiles", "auto", 0);  // Enable tile-based decoding
				av_dict_set(&opts, "lowdelay", "1", 0);  // Low latency for streaming
				Utils::Log("Xbox Series X: Optimized AV1 decode parameters with tiles and low-latency");
			} else if (videoFormat & VIDEO_FORMAT_MASK_H265) {
				av_dict_set(&opts, "threads", "6", 0);
				av_dict_set(&opts, "strict", "-2", 0);   // Allow experimental features
				av_dict_set(&opts, "lowdelay", "1", 0);  // Low latency for streaming
				// Enable range extensions for high quality content
				if (videoFormat & (VIDEO_FORMAT_H265_REXT8_444 | VIDEO_FORMAT_H265_REXT10_444)) {
					av_dict_set(&opts, "strict", "experimental", 0);
				}
				Utils::Log("Xbox Series X: Optimized HEVC decode parameters with range extensions");
			} else if (videoFormat & VIDEO_FORMAT_MASK_H264) {
				av_dict_set(&opts, "threads", "4", 0);
				av_dict_set(&opts, "lowdelay", "1", 0);  // Low latency for streaming
				Utils::Log("Xbox Series X: Optimized H.264 decode parameters");
			}
		}

		int err = avcodec_open2(decoder_ctx, decoder, &opts);
		if (opts) {
			av_dict_free(&opts);
		}
		if (err < 0) {
			char msg[2048];
			sprintf(msg, "Failed to create FFMpeg Codec: %d\n", err);
			Utils::Log(msg);
			return err;
		}
		// Optimize buffer size based on resolution and codec
		size_t buffer_multiplier = 2;
		if (isXboxSeriesX) {
			if (width >= 3840 || height >= 2160) {
				buffer_multiplier = 4;  // 4K content needs larger buffers
			} else if (videoFormat & VIDEO_FORMAT_MASK_AV1) {
				buffer_multiplier = 3;  // AV1 benefits from larger buffers
			}
		}
		size_t buffer_size = isXboxSeriesX ? (DECODER_BUFFER_SIZE * buffer_multiplier) : DECODER_BUFFER_SIZE;
		ffmpeg_buffer = (unsigned char*)malloc(buffer_size + AV_INPUT_BUFFER_PADDING_SIZE);
		if (ffmpeg_buffer == NULL) {
			Utils::Log("OOM\n");
			Cleanup();
			return -1;
		}
		GAMING_DEVICE_MODEL_INFORMATION info = {};
		GetGamingDeviceModelInformation(&info);
		bool isXboxSeriesX = (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT &&
			(info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X ||
			 info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X_DEVKIT ||
			 info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S ||
			 info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S_DEVKIT));

		int buffer_count = isXboxSeriesX ? 4 : 2;
		dec_frames_cnt = buffer_count;
		dec_frames = (AVFrame**)malloc(buffer_count * sizeof(AVFrame*));
		ready_frames = (AVFrame**)malloc(buffer_count * sizeof(AVFrame*));
		if (dec_frames == NULL) {
			Utils::Log("Cannot allocate Frames\n");
			return -1;
		}

		for (int i = 0; i < buffer_count; i++) {
			dec_frames[i] = av_frame_alloc();
			if (dec_frames[i] == NULL) {
				Utils::Log("Cannot allocate Frames\n");
				return -1;
			}
			ready_frames[i] = av_frame_alloc();
			if (ready_frames[i] == NULL) {
				Utils::Log("Cannot allocate Frames\n");
				return -1;
			}
		}
		if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT &&
			(info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE ||
				info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE_S ||
				info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X ||
				info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X_DEVKIT)) {
			Utils::Log("Using hack for Xbox One Consoles");
			hackWait = true;
		}
		return 0;
	}

	void FFMpegDecoder::Start() {
		Utils::Log("Decoding Started\n");
	}

	void FFMpegDecoder::Stop() {
		Utils::Log("Decoding Stopped\n");
	}

	void FFMpegDecoder::Cleanup() {
		if (dec_frames != NULL) {
			for (int i = 0; i < dec_frames_cnt; i++) {
				av_frame_free(&dec_frames[i]);
			}
			free(dec_frames);
		}
		avcodec_close(decoder_ctx);
		avcodec_free_context(&decoder_ctx);
		if (ffmpeg_buffer != NULL) {
			free(ffmpeg_buffer);
			ffmpeg_buffer = NULL;
		}
		if (ready_frames != NULL) {
			free(ready_frames);
			ready_frames = NULL;
		}
		Utils::Log("Decoding Clean\n");
	}

	bool FFMpegDecoder::SubmitDU() {
		PDECODE_UNIT decodeUnit = nullptr;
		VIDEO_FRAME_HANDLE frameHandle = nullptr;
		bool status = LiWaitForNextVideoFrame(&frameHandle, &decodeUnit);
		if (status == false)return false;
		int n = LiGetPendingVideoFrames();
		GAMING_DEVICE_MODEL_INFORMATION info = {};
		GetGamingDeviceModelInformation(&info);
		bool isXboxSeriesX = (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT &&
			(info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X ||
			 info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X_DEVKIT ||
			 info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S ||
			 info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S_DEVKIT));
		// Use same buffer size calculation as Init
		size_t buffer_multiplier = 2;
		if (isXboxSeriesX) {
			if (width >= 3840 || height >= 2160) {
				buffer_multiplier = 4;
			} else if (videoFormat & VIDEO_FORMAT_MASK_AV1) {
				buffer_multiplier = 3;
			}
		}
		size_t max_buffer_size = isXboxSeriesX ? (DECODER_BUFFER_SIZE * buffer_multiplier) : DECODER_BUFFER_SIZE;
		
		if (decodeUnit->fullLength > max_buffer_size) {
			Utils::Log("Decoder Buffer Size reached\n");
			LiCompleteVideoFrame(frameHandle, DR_NEED_IDR);
			return false;
		}
		PLENTRY entry = decodeUnit->bufferList;
		uint32_t length = 0;
		while (entry != NULL) {
			memcpy(ffmpeg_buffer + length, entry->data, entry->length);
			length += entry->length;
			entry = entry->next;
		}

		// Detect breaks in the frame sequence indicating dropped packets
		static uint32_t lastFrameNumber = 0;
		uint32_t droppedFrames = 0;
		if (lastFrameNumber > 0) {
			// Any frame number greater than m_LastFrameNumber + 1 represents a dropped frame
			droppedFrames = decodeUnit->frameNumber - (lastFrameNumber + 1);
		}
		lastFrameNumber = decodeUnit->frameNumber;

		// track stats for a variety of things we can track at the same time
		this->resources->GetStats()->SubmitVideoBytesAndReassemblyTime(
			length,
			(uint32_t)(decodeUnit->enqueueTimeMs - decodeUnit->receiveTimeMs),
			decodeUnit->frameHostProcessingLatency,
			droppedFrames);

		int err;
		err = Decode(ffmpeg_buffer, length);
		if (err < 0) {
			LiCompleteVideoFrame(frameHandle, DR_NEED_IDR);
			return false;
		}
		LiCompleteVideoFrame(frameHandle, DR_OK);
		return true;
	}

	int FFMpegDecoder::Decode(unsigned char* indata, int inlen) {
		int err;

		pkt.data = indata;
		pkt.size = inlen;
		decodeStartTime = av_gettime_relative();
		err = avcodec_send_packet(decoder_ctx, &pkt);
		if (err < 0) {

			char errorstringnew[2048], ffmpegError[1024];
			av_strerror(err, ffmpegError, 1024);
			sprintf(errorstringnew, "Error avcodec_send_packet: %s\n", ffmpegError);
			Utils::Log(errorstringnew);
			return err == 1 ? 0 : err;
		}
		return err < 0 ? err : 0;
	}

	AVFrame* FFMpegDecoder::GetFrame() {
		int err = avcodec_receive_frame(decoder_ctx, dec_frames[next_frame]);
		if (err != 0 && err != AVERROR(EAGAIN)) {
			char errorstringnew[1024];
			sprintf(errorstringnew, "Error avcodec_receive_frame: %d\n", AVERROR(err));
			Utils::Log(errorstringnew);
			return nullptr;
		}
		if (err == 0) {
			this->resources->GetStats()->SubmitDecodeMs((double)(av_gettime_relative() - decodeStartTime) / 1000.0);
			
			GAMING_DEVICE_MODEL_INFORMATION info = {};
			GetGamingDeviceModelInformation(&info);
			bool isXboxSeriesX = (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT &&
				(info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X ||
				 info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X_DEVKIT ||
				 info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S ||
				 info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S_DEVKIT));

			if (isXboxSeriesX) {
				// Adjust frame queue based on resolution and codec
				int max_pending_frames = 3;
				if (width >= 3840 || height >= 2160) {
					max_pending_frames = 2;  // Reduce for 4K to save memory
				} else if (videoFormat & VIDEO_FORMAT_MASK_AV1) {
					max_pending_frames = 4;  // AV1 can handle more pending frames
				}
				if (LiGetPendingVideoFrames() > max_pending_frames) return nullptr;
			} else {
				if (LiGetPendingVideoFrames() > 1) return nullptr;
				if (hackWait && LiGetPendingVideoFrames() < 2) moonlight_xbox_dx::usleep(12000);
			}
			AVFrame* frame = dec_frames[next_frame];
			return frame;
		}
		return nullptr;
	}

	//Helpers
	FFMpegDecoder* instance;

	FFMpegDecoder* FFMpegDecoder::createDecoderInstance(std::shared_ptr<DX::DeviceResources> resources) {
		if (instance == NULL) {
			instance = new FFMpegDecoder(resources);
		}
		return instance;
	}

	FFMpegDecoder* getDecoderInstance() {
		return instance;
	}

	int initCallback(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
		return instance->Init(videoFormat, width, height, redrawRate, context, drFlags);
	}
	void startCallback() {
		instance->Start();
	}
	void stopCallback() {
		instance->Stop();
	}
	void cleanupCallback() {
		instance->Cleanup();
		delete instance;
		instance = NULL;
	}

	FFMpegDecoder* FFMpegDecoder::getInstance() {
		return instance;
	}

	DECODER_RENDERER_CALLBACKS FFMpegDecoder::getDecoder() {
		instance = FFMpegDecoder::getInstance();
		DECODER_RENDERER_CALLBACKS decoder_callbacks_sdl;
		LiInitializeVideoCallbacks(&decoder_callbacks_sdl);
		decoder_callbacks_sdl.setup = initCallback;
		decoder_callbacks_sdl.start = startCallback;
		decoder_callbacks_sdl.stop = stopCallback;
		decoder_callbacks_sdl.cleanup = cleanupCallback;
		decoder_callbacks_sdl.submitDecodeUnit = NULL;
		decoder_callbacks_sdl.capabilities = CAPABILITY_PULL_RENDERER | CAPABILITY_INTRA_REFRESH | CAPABILITY_REFERENCE_FRAME_INVALIDATION_AV1;
		return decoder_callbacks_sdl;
	}


}


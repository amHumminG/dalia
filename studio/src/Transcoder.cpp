#include "Transcoder.h"

#include <iostream>

extern "C"{
#include "libavformat/avformat.h"
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
}

namespace dalia {
   Transcoder::Transcoder(AudioFormat format) {
      m_Format = format;
   }

   //TODO: fix actual error handling
   AudioData Transcoder::Transcode(const char *filepath) {
      std::cout << "Soundbank Tool" << std::endl;
      AudioData reData;
      reData.format = m_Format;
      //ffmpeg demonstration code with funny error handling

      std::cout << "FFmpeg Audio Decoding\n";

      // file header information?
      AVFormatContext* in_fmt_ctx = avformat_alloc_context();
      if (avformat_open_input(&in_fmt_ctx, filepath, nullptr, nullptr) != 0) {
         std::cerr << "Error: Could not open file " << filepath << "\n";
         return {};
      }

      // Read file stream information
      if (avformat_find_stream_info(in_fmt_ctx, nullptr) < 0) {
         std::cerr << "Error: Could not find stream info\n";
         //return -1;
      }
      // find first audio stream in file
      int audio_stream_index = -1;
      for (unsigned int i = 0; i < in_fmt_ctx->nb_streams; i++) {
         if (in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i; break;
         }
      }

      if (audio_stream_index == -1) {
         std::cerr << "Error: No audio stream found in file" << filepath << std::endl;
         //return -1;
      }

      // get codec params and correct decoder
      AVCodecParameters* in_codec_par = in_fmt_ctx->streams[audio_stream_index]->codecpar;
      const AVCodec* codec = avcodec_find_decoder(in_codec_par->codec_id);
      if (!codec) {
         std::cerr << "Error: Unsupported codec\n";
         //return -1;
      }

      // Allocate codec context and init
      AVCodecContext* in_codec_ctx = avcodec_alloc_context3(codec);
      avcodec_parameters_to_context(in_codec_ctx, in_codec_par);
      if (avcodec_open2(in_codec_ctx, codec, nullptr) < 0) {
         std::cerr << "Error: Could not open codec\n";
         //return -1;
      }

      std::cout << "Audio stream opend\n";
      std::cout << "Sample Rate: " << in_codec_ctx->sample_rate << " Hz\n";
      std::cout << "Channels: " << in_codec_ctx->ch_layout.nb_channels << "\n";
      reData.sampleRate = in_codec_ctx->sample_rate;
      reData.channels = in_codec_ctx->ch_layout.nb_channels;

      // allocate memory for packets (packest (compressed) Frames (uncompressed))

      // Resampler Setup (Targeting 16-bit PCM)
      AVChannelLayout target_ch_layout;
      av_channel_layout_copy(&target_ch_layout, &in_codec_ctx->ch_layout);
      int target_sample_rate = in_codec_ctx->sample_rate;

      AVSampleFormat target_sample_fmt;

      // kanske inte är bra sätt att göra saker på men aja
      if (m_Format == AudioFormat::PCM_16) {
         target_sample_fmt = AV_SAMPLE_FMT_S16; // 16-bit integer
      }


      SwrContext* swr_ctx = nullptr;
      swr_alloc_set_opts2(&swr_ctx,
         &target_ch_layout, target_sample_fmt, target_sample_rate,
         &in_codec_ctx->ch_layout, in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate,
         0, nullptr);
      swr_init(swr_ctx);

      // Processing Loop Allocation
      AVPacket* in_packet = av_packet_alloc();
      AVFrame* in_frame = av_frame_alloc();
      AVFrame* resampled_frame = av_frame_alloc();

      resampled_frame->sample_rate = target_sample_rate;
      resampled_frame->format = target_sample_fmt;
      av_channel_layout_copy(&resampled_frame->ch_layout, &target_ch_layout);

      std::vector<uint8_t> audio_data;
      std::cout << "Decoding, resampling, and writing to memory...\n";

      // Read and Decode Loop
      while (av_read_frame(in_fmt_ctx, in_packet) >= 0) {
         if (in_packet->stream_index == audio_stream_index) {
            if (avcodec_send_packet(in_codec_ctx, in_packet) == 0) {
               while (avcodec_receive_frame(in_codec_ctx, in_frame) == 0) {

                  // Resample the frame to our target 16-bit PCM
                  swr_convert_frame(swr_ctx, resampled_frame, in_frame);

                  int data_size = av_samples_get_buffer_size(
                      nullptr,
                      resampled_frame->ch_layout.nb_channels,
                      resampled_frame->nb_samples,
                      (AVSampleFormat)resampled_frame->format,
                      1
                  );

                  if (data_size > 0) {
                     audio_data.insert(audio_data.end(), resampled_frame->data[0], resampled_frame->data[0] + data_size);
                  }
               }
            }
         }
         av_packet_unref(in_packet);
      }

      reData.bytes = audio_data;
      std::cout << "Done! Loaded " << audio_data.size() << " bytes of raw PCM audio.\n";


      // memory clean :)
      av_channel_layout_uninit(&target_ch_layout);
      swr_free(&swr_ctx);
      av_frame_free(&in_frame);
      av_frame_free(&resampled_frame);
      av_packet_free(&in_packet);
      avcodec_free_context(&in_codec_ctx);
      avformat_close_input(&in_fmt_ctx);

      return reData;
   }
}

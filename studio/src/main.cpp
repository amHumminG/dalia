#include <iostream>

extern "C"{
#include "libavformat/avformat.h"
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
}

int main() {
  	std::cout << "Soundbank Tool" << std::endl;

	//ffmpeg demonstration code with funny error handling
	const char* filepath = "assets/Zeri.ogg";
	const char* out_file = "assets/output.wav";

	std::cout << "FFmpeg Audio Decoding\n";

	// file header information?
	AVFormatContext* in_fmt_ctx = avformat_alloc_context();
	if (avformat_open_input(&in_fmt_ctx, filepath, nullptr, nullptr) != 0) {
		std::cerr << "Error: Could not open file " << filepath << "\n";
		return -1;
	}

	// Read file stream information
	if (avformat_find_stream_info(in_fmt_ctx, nullptr) < 0) {
		std::cerr << "Error: Could not find stream info\n";
		return -1;
	}

	// find first audio stream in file
	int audio_stream_index = -1;
	for (unsigned int i = 0; i < in_fmt_ctx->nb_streams; i++) {
		if (in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_stream_index = i; break;
		}
	}

	if (audio_stream_index == -1) {
		std::cerr << "Error: No audio stream found in file\n";
		return -1;
	}

	// get codec params and correct decoder
	AVCodecParameters* in_codec_par = in_fmt_ctx->streams[audio_stream_index]->codecpar;
	const AVCodec* codec = avcodec_find_decoder(in_codec_par->codec_id);
	if (!codec) {
		std::cerr << "Error: Unsupported codec\n";
		return -1;
	}

	// Allocate codec context and init
	AVCodecContext* in_codec_ctx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(in_codec_ctx, in_codec_par);
	if (avcodec_open2(in_codec_ctx, codec, nullptr) < 0) {
		std::cerr << "Error: Could not open codec\n";
		return -1;
	}

	std::cout << "Audio stream opend\n";
	std::cout << "Sample Rate: " << in_codec_ctx->sample_rate << " Hz\n";
	std::cout << "Channels: " << in_codec_ctx->ch_layout.nb_channels << "\n";

	// allocate memory for packets (packest (compressed) Frames (uncompressed))
	AVPacket* packet = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();

	// encoder setup
	AVFormatContext* out_fmt_ctx = nullptr;
	avformat_alloc_output_context2(&out_fmt_ctx, nullptr, "wav", out_file);

	AVStream* out_stream = avformat_new_stream(out_fmt_ctx, nullptr);
	const AVCodec* out_codec = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE); // standard pcm in wav file
	AVCodecContext* out_codec_ctx = avcodec_alloc_context3(out_codec);

	// Configure encoder to match the inputs sample rate and channel layout
	out_codec_ctx->sample_rate = in_codec_ctx->sample_rate;
	av_channel_layout_copy(&out_codec_ctx->ch_layout, &in_codec_ctx->ch_layout);
	out_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16; // Force 16-bit interleaved

	avcodec_open2(out_codec_ctx, out_codec, nullptr);
	avcodec_parameters_from_context(out_stream->codecpar, out_codec_ctx);

	// Open the output file and write the WAV header
	avio_open(&out_fmt_ctx->pb, out_file, AVIO_FLAG_WRITE);
	avformat_write_header(out_fmt_ctx, nullptr);

	// resampler setup

	SwrContext* swr_ctx = nullptr;
	swr_alloc_set_opts2(&swr_ctx,
		&out_codec_ctx->ch_layout, out_codec_ctx->sample_fmt, out_codec_ctx->sample_rate,
		&in_codec_ctx->ch_layout, in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate,
		0, nullptr);
	swr_init(swr_ctx);

	// prossecing loop
	// allocate
	AVPacket* in_packet = av_packet_alloc();
	AVFrame* in_frame = av_frame_alloc();

	AVPacket* out_packet = av_packet_alloc();
	AVFrame* resampled_frame = av_frame_alloc();

	// Set up the resampled frame properties so FFmpeg knows how much memory to allocate
	resampled_frame->sample_rate = out_codec_ctx->sample_rate;
	resampled_frame->format = out_codec_ctx->sample_fmt;
	av_channel_layout_copy(&resampled_frame->ch_layout, &out_codec_ctx->ch_layout);

	std::cout << "Decoding, resampling, and writing\n";

	while (av_read_frame(in_fmt_ctx, in_packet) >= 0) {
		// Check if right packet aka if it belongs to the stream
		if (in_packet->stream_index == audio_stream_index) {
			// Send compressed packet to decoder
			if (avcodec_send_packet(in_codec_ctx, in_packet) == 0) {
				while (avcodec_receive_frame(in_codec_ctx, in_frame) == 0) {

					// Resample the frame
					swr_convert_frame(swr_ctx, resampled_frame, in_frame);

					// Send the resampled frame to the PCM encoder
					if (avcodec_send_frame(out_codec_ctx, resampled_frame) == 0) {

						// Receive the encoded packet and write it to the WAV file
						while (avcodec_receive_packet(out_codec_ctx, out_packet) == 0) {
							av_interleaved_write_frame(out_fmt_ctx, out_packet);
							av_packet_unref(out_packet);
						}
					}
				}
			}
		}
		av_packet_unref(in_packet);
	}
	// Write the end of the WAV file
	av_write_trailer(out_fmt_ctx);
	std::cout << "Done: " << out_file << "\n";



	// memory clean :)
	avio_closep(&out_fmt_ctx->pb);
	avformat_free_context(out_fmt_ctx);
	avcodec_free_context(&out_codec_ctx);

	avformat_close_input(&in_fmt_ctx);
	avcodec_free_context(&in_codec_ctx);

	swr_free(&swr_ctx);
	av_frame_free(&in_frame);
	av_frame_free(&resampled_frame);
	av_packet_free(&in_packet);
	av_packet_free(&out_packet);
	return 0;
}

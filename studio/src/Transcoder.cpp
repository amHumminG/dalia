#include "Transcoder.h"
#include <iostream>
#include <thread>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
}

namespace dalia::studio {

    void Transcoder::TranscodeToOggAsync(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destinationPath,
        std::function<void(bool)> onComplete)
    {
        if (!std::filesystem::exists(sourcePath)) {
            std::cerr << "Transcode error: Source file does not exist.\n";
            if (onComplete) onComplete(false);
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(destinationPath.parent_path(), ec);
        if (ec) {
            std::cerr << "Transcode error: Could not create directory.\n";
            if (onComplete) onComplete(false);
            return;
        }

        std::string inPath = sourcePath.string();
        std::string outPath = destinationPath.string();

        std::thread([inPath, outPath, onComplete]() {
            bool success = TranscodeInternal(inPath, outPath);
            if (onComplete) {
                onComplete(success);
            }
        }).detach();
    }

    bool Transcoder::TranscodeInternal(const std::string& inPath, const std::string& outPath) {
        bool success = false;
        bool frames_encoded = false; // Guard for flushing empty encoders

        AVFormatContext* in_fmt_ctx = nullptr;
        AVFormatContext* out_fmt_ctx = nullptr;
        AVCodecContext* dec_ctx = nullptr;
        AVCodecContext* enc_ctx = nullptr;
        SwrContext* swr_ctx = nullptr;
        AVAudioFifo* fifo = nullptr;

        int64_t pts_counter = 0;
        int frame_size = 0;
        int audio_stream_index = -1;
        AVStream* out_stream = nullptr;

        AVPacket* in_packet = av_packet_alloc();
        AVPacket* out_packet = av_packet_alloc();
        AVFrame* in_frame = av_frame_alloc();
        AVFrame* out_frame = av_frame_alloc();

        if (!in_packet || !out_packet || !in_frame || !out_frame) {
            std::cerr << "Failed to allocate core FFmpeg structures.\n";
            goto cleanup;
        }

        // --- SETUP INPUT ---
        if (avformat_open_input(&in_fmt_ctx, inPath.c_str(), nullptr, nullptr) < 0) goto cleanup;
        if (avformat_find_stream_info(in_fmt_ctx, nullptr) < 0) goto cleanup;

        for (unsigned int i = 0; i < in_fmt_ctx->nb_streams; i++) {
            if (in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_stream_index = i; break;
            }
        }
        if (audio_stream_index == -1) goto cleanup;

        {
            AVCodecParameters* in_codec_par = in_fmt_ctx->streams[audio_stream_index]->codecpar;
            const AVCodec* decoder = avcodec_find_decoder(in_codec_par->codec_id);
            if (!decoder) goto cleanup;

            dec_ctx = avcodec_alloc_context3(decoder);
            if (!dec_ctx) goto cleanup;

            avcodec_parameters_to_context(dec_ctx, in_codec_par);
            if (dec_ctx->ch_layout.nb_channels == 0) {
                av_channel_layout_default(&dec_ctx->ch_layout, 2);
            }
            if (avcodec_open2(dec_ctx, decoder, nullptr) < 0) goto cleanup;
        }

        // --- SETUP OUTPUT ---
        avformat_alloc_output_context2(&out_fmt_ctx, nullptr, "ogg", outPath.c_str());
        if (!out_fmt_ctx) goto cleanup;

        {
            const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_VORBIS);
            if (!encoder) goto cleanup;

            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx) goto cleanup;

            enc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
            enc_ctx->sample_rate = dec_ctx->sample_rate;
            av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);

            if (enc_ctx->ch_layout.nb_channels == 0) {
                av_channel_layout_default(&enc_ctx->ch_layout, 2);
            }
            enc_ctx->time_base = { 1, enc_ctx->sample_rate };

            if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            if (avcodec_open2(enc_ctx, encoder, nullptr) < 0) goto cleanup;

            out_stream = avformat_new_stream(out_fmt_ctx, encoder);
            if (!out_stream) goto cleanup;
            avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
        }

        if (!(out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&out_fmt_ctx->pb, outPath.c_str(), AVIO_FLAG_WRITE) < 0) goto cleanup;
        }

        if (avformat_write_header(out_fmt_ctx, nullptr) < 0) goto cleanup;

        // --- SETUP RESAMPLER & FIFO ---
        swr_alloc_set_opts2(&swr_ctx,
            &enc_ctx->ch_layout, enc_ctx->sample_fmt, enc_ctx->sample_rate,
            &dec_ctx->ch_layout, dec_ctx->sample_fmt, dec_ctx->sample_rate,
            0, nullptr);

        if (!swr_ctx || swr_init(swr_ctx) < 0) goto cleanup;

        fifo = av_audio_fifo_alloc(enc_ctx->sample_fmt, enc_ctx->ch_layout.nb_channels, 1);
        if (!fifo) goto cleanup;

        frame_size = enc_ctx->frame_size > 0 ? enc_ctx->frame_size : 1024;

        // --- TRANSCODE LOOP ---
        while (av_read_frame(in_fmt_ctx, in_packet) >= 0) {
            if (in_packet->stream_index == audio_stream_index) {
                if (avcodec_send_packet(dec_ctx, in_packet) == 0) {
                    while (avcodec_receive_frame(dec_ctx, in_frame) == 0) {

                        // Explicitly calculate and allocate the required buffer BEFORE resampling
                        // This prevents the 0x60 crash bug in swr_convert_frame's auto-allocator
                        int out_samples = swr_get_out_samples(swr_ctx, in_frame->nb_samples);
                        if (out_samples > 0) {
                            out_frame->nb_samples = out_samples;
                            out_frame->sample_rate = enc_ctx->sample_rate;
                            out_frame->format = enc_ctx->sample_fmt;
                            av_channel_layout_copy(&out_frame->ch_layout, &enc_ctx->ch_layout);

                            if (av_frame_get_buffer(out_frame, 0) == 0) {
                                if (swr_convert_frame(swr_ctx, out_frame, in_frame) == 0) {
                                    av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + out_frame->nb_samples);
                                    av_audio_fifo_write(fifo, (void**)out_frame->data, out_frame->nb_samples);
                                }
                            }
                            av_frame_unref(out_frame); // Release memory for the next loop
                        }

                        // Feed the encoder from FIFO
                        while (av_audio_fifo_size(fifo) >= frame_size) {
                            AVFrame* enc_frame = av_frame_alloc();
                            enc_frame->nb_samples = frame_size;
                            enc_frame->format = enc_ctx->sample_fmt;
                            enc_frame->sample_rate = enc_ctx->sample_rate;
                            av_channel_layout_copy(&enc_frame->ch_layout, &enc_ctx->ch_layout);

                            if (av_frame_get_buffer(enc_frame, 0) >= 0) {
                                av_audio_fifo_read(fifo, (void**)enc_frame->data, frame_size);
                                enc_frame->pts = pts_counter;
                                pts_counter += frame_size;

                                if (avcodec_send_frame(enc_ctx, enc_frame) == 0) {
                                    frames_encoded = true;
                                    while (avcodec_receive_packet(enc_ctx, out_packet) == 0) {
                                        av_packet_rescale_ts(out_packet, enc_ctx->time_base, out_stream->time_base);
                                        out_packet->stream_index = out_stream->index;
                                        av_interleaved_write_frame(out_fmt_ctx, out_packet);
                                        av_packet_unref(out_packet);
                                    }
                                }
                            }
                            av_frame_free(&enc_frame);
                        }
                    }
                }
            }
            av_packet_unref(in_packet);
        }

        // --- FLUSH FIFO ---
        while (av_audio_fifo_size(fifo) > 0) {
            int read_size = std::min(av_audio_fifo_size(fifo), frame_size);

            AVFrame* enc_frame = av_frame_alloc();
            enc_frame->nb_samples = frame_size;
            enc_frame->format = enc_ctx->sample_fmt;
            enc_frame->sample_rate = enc_ctx->sample_rate;
            av_channel_layout_copy(&enc_frame->ch_layout, &enc_ctx->ch_layout);

            if (av_frame_get_buffer(enc_frame, 0) >= 0) {
                av_audio_fifo_read(fifo, (void**)enc_frame->data, read_size);

                if (read_size < frame_size) {
                    av_samples_set_silence(enc_frame->data, read_size, frame_size - read_size, enc_ctx->ch_layout.nb_channels, enc_ctx->sample_fmt);
                }

                enc_frame->pts = pts_counter;
                pts_counter += frame_size;

                if (avcodec_send_frame(enc_ctx, enc_frame) == 0) {
                    frames_encoded = true;
                    while (avcodec_receive_packet(enc_ctx, out_packet) == 0) {
                        av_packet_rescale_ts(out_packet, enc_ctx->time_base, out_stream->time_base);
                        out_packet->stream_index = out_stream->index;
                        av_interleaved_write_frame(out_fmt_ctx, out_packet);
                        av_packet_unref(out_packet);
                    }
                }
            }
            av_frame_free(&enc_frame);
        }

        // --- FLUSH ENCODER ---
        // Some encoders crash accessing 0x60 if you flush them when they've received exactly 0 frames.
        if (frames_encoded) {
            avcodec_send_frame(enc_ctx, nullptr);
            while (avcodec_receive_packet(enc_ctx, out_packet) == 0) {
                av_packet_rescale_ts(out_packet, enc_ctx->time_base, out_stream->time_base);
                out_packet->stream_index = out_stream->index;
                av_interleaved_write_frame(out_fmt_ctx, out_packet);
                av_packet_unref(out_packet);
            }
        }

        av_write_trailer(out_fmt_ctx);
        success = true;

    cleanup:
        if (fifo) av_audio_fifo_free(fifo);
        if (swr_ctx) swr_free(&swr_ctx);
        if (dec_ctx) avcodec_free_context(&dec_ctx);
        if (enc_ctx) avcodec_free_context(&enc_ctx);
        if (in_fmt_ctx) avformat_close_input(&in_fmt_ctx);
        if (out_fmt_ctx) {
            if (!(out_fmt_ctx->oformat->flags & AVFMT_NOFILE) && out_fmt_ctx->pb) {
                avio_closep(&out_fmt_ctx->pb);
            }
            avformat_free_context(out_fmt_ctx);
        }
        if (in_frame) av_frame_free(&in_frame);
        if (out_frame) av_frame_free(&out_frame);
        if (in_packet) av_packet_free(&in_packet);
        if (out_packet) av_packet_free(&out_packet);

        return success;
    }
}
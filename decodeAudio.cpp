#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <tuple>
#include <memory>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
#include <libavutil/samplefmt.h>
}


struct MemoryAVFormat {
    MemoryAVFormat(const MemoryAVFormat &) = delete;

    std::shared_ptr<AVFormatContext> fmt_ctx;
    std::shared_ptr<AVIOContext> io_ctx;
    std::shared_ptr<AVCodecContext> codec_ctx;
    int audioStreamIndex{0};

    std::vector<char> & compressed_audio;
    size_t audio_offset;

    MemoryAVFormat(std::vector<char> & compressed_audio)
    : fmt_ctx(avformat_alloc_context(), [](AVFormatContext* p) {avformat_close_input(&p);}),
      io_ctx(nullptr),
      compressed_audio(compressed_audio),
      audio_offset(0) {
        if (fmt_ctx == nullptr)
            throw std::runtime_error("Failed to allocate context");

        create_audio_buffer_io_context();

        fmt_ctx->pb = io_ctx.get();
        fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

        auto avFormatPtr = fmt_ctx.get();
        int err = avformat_open_input(&avFormatPtr, "nullptr", nullptr, nullptr);
        if (err != 0 || !avFormatPtr)
            std::runtime_error("Error configuring context from audio buffer:" + std::to_string(err));

        // find codec
        if (avformat_find_stream_info(avFormatPtr, nullptr) < 0) {
                throw(std::runtime_error("Cannot find stream information"));
        }

        open_codec_context();
    }

    bool is_eof() {
        return audio_offset >= compressed_audio.size();
    }

    int read (uint8_t* theBuf, int theBufSize) {
        int aNbRead = std::min (int(compressed_audio.size() - audio_offset), theBufSize);
        if(aNbRead <= 0) {
            return AVERROR_EOF;
        }

        memcpy(theBuf, compressed_audio.data() + audio_offset, aNbRead);
        audio_offset += aNbRead;
        return aNbRead;
    }

    int64_t seek(int64_t offset, int whence) {
         if (whence == AVSEEK_SIZE) { return compressed_audio.size(); }
         audio_offset = offset;

         if(compressed_audio.data() == nullptr || compressed_audio.size() == 0) { return -1; }
         if     (whence == SEEK_SET) { audio_offset = offset; }
         else if(whence == SEEK_CUR) { audio_offset += offset; }
         else if(whence == SEEK_END) { audio_offset = compressed_audio.size() + offset; }

         return offset;
    }

    ~MemoryAVFormat() {
// all resources are freed by shared pointers!
//        av_free(io_ctx);
//        avformat_close_input(&fmt_ctx);
//        avcodec_free_context(&codec_ctx);

    }
protected:
    void create_audio_buffer_io_context() {
        const int aBufferSize = 4096;
        unsigned char* aBufferIO = reinterpret_cast<unsigned char*>(av_malloc(aBufferSize + AV_INPUT_BUFFER_PADDING_SIZE));
        auto p_io_ctx = avio_alloc_context(aBufferIO,
                                           aBufferSize,
                                           0,
                                           this,
                                           [](void* opaque, uint8_t* buf, int bufSize)
                                           { return (reinterpret_cast<MemoryAVFormat*>(opaque))->read(buf, bufSize); },
                                           nullptr,
                                           [](void* opaque, int64_t offset, int whence)
                                           { return (reinterpret_cast<MemoryAVFormat*>(opaque))->seek(offset, whence); });
        io_ctx.reset(p_io_ctx,
                                                [](AVIOContext* io_ctx) {if (io_ctx) {av_freep(&(io_ctx->buffer)); avio_context_free(&io_ctx);}});
        if (!io_ctx) {
            throw std::runtime_error("error allocating avio context");
        }
    }
    void open_codec_context()
    {

        int ret = av_find_best_stream(fmt_ctx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (ret < 0) {
            throw std::runtime_error("Could not find audio stream in input file");
        } else {
            audioStreamIndex = ret;
            AVStream *st = fmt_ctx->streams[audioStreamIndex];

            // find decoder for the stream
            const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
            if (!codec) {
                throw(std::runtime_error("Failed to find %s codec\n"));
            }

            /* Allocate a codec context for the decoder */
            codec_ctx.reset(avcodec_alloc_context3(codec), [](AVCodecContext* p) {avcodec_free_context(&p);});
            if (!codec_ctx) {
                throw(std::runtime_error("Failed to allocate the %s codec context\n"));
            }

            // Copy codec parameters from input stream to output codec context
            if ((ret = avcodec_parameters_to_context(codec_ctx.get(), st->codecpar)) < 0) {
                throw std::runtime_error("Failed to copy codec parameters to decoder context");
            }

            // Init the decoders
            if ((ret = avcodec_open2(codec_ctx.get(), codec, nullptr)) < 0) {
                throw std::runtime_error("Failed to open audio codec");
            }
        }
    }

};

/**
  Decode compressed audio packet to float 32 bit pcm data
  and append result data to resultBuf
  if source file was mono, second channel is artificially added
 *
 */
void decodePacket(AVCodecContext *ctx, AVPacket *pkt, AVFrame *frame, std::vector<float> &resultBuf, bool isLast = true)
{
    // send the packet with the compressed data to the decoder
    int ret = avcodec_send_packet(ctx, pkt);
    if (ret < 0) {
        if (isLast) {
            return;  // it's OK, maybe some padding at the end
        }
        throw std::runtime_error("Error submitting the packet to the decoder");
    }

    // read all the output frames (in general there may be any number of them)
    while (ret >= 0) {
        ret = avcodec_receive_frame(ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            throw std::runtime_error("Error during decoding\n");
        }
        size_t sampleSize = av_get_bytes_per_sample(ctx->sample_fmt);
        bool planar{!!av_sample_fmt_is_planar(ctx->sample_fmt)};
        if (planar) {
            for (size_t i = 0; i < frame->nb_samples; i++) {
                // store only first channel if stereo:
                size_t len = resultBuf.size();
                resultBuf.resize(len + 1);
                if (sampleSize == 4) {
                    memcpy(resultBuf.data() + len, frame->data[0] + sampleSize*i, sampleSize);
                }else {  // resample
                    *(resultBuf.data()+len) = float(*(reinterpret_cast<int16_t*>(frame->data[0]) + i));
                }
            }
        }else {  // interleaved
            size_t len = resultBuf.size();
            resultBuf.resize(len + frame->nb_samples);
            if (frame->channels == 1) {
                if (sampleSize == 4) {  // float 32 bit samples
                    memcpy(resultBuf.data() + len, frame->data[0], frame->nb_samples * frame->channels * sampleSize);
                }else {  // resample:
                    for (int i = 0; i < frame->nb_samples; i++) {
                        *(resultBuf.data()+len + i) = float(*(reinterpret_cast<int16_t*>(frame->data[0]) + i*ctx->channels));
                    }
                }
            }else {
                // store only first channel if stereo:
                for (int i = 0; i < frame->nb_samples; i++) {
                    if (sampleSize == 4) {  // float 32 bit samples
                        *(resultBuf.data()+len + i*2) = *(reinterpret_cast<float*>(frame->data[0]) + i);
                    }else {  // resample
                        *(resultBuf.data()+len + i*2) = float(*(reinterpret_cast<int16_t*>(frame->data[0]) + i));
                    }
                }
            }
        }
        av_frame_unref(frame);
    }
}


/**
  Decode compressed audio (mp3, wma, flac, etc) to float 32 bit pcm data
  compressedBuf - compressed file data (already read to memory)
  return value - vector of float 32 bit pcm samples, interleaved stereo
  if source file was stereo, only first channel is returned
 *
 */
std::tuple<std::vector<float>, int64_t, unsigned> decodeAudio(std::vector<char> &compressedBuf)
{
    std::vector<float> resultWav;
    if (compressedBuf.size() == 0) {
        return {std::move(resultWav), 0, 0};
    }
    MemoryAVFormat av(compressedBuf);
    // decode audio data:
    std::shared_ptr<AVPacket> packet(av_packet_alloc(), [](AVPacket* p) {av_packet_free(&p);});
    std::shared_ptr<AVFrame> decoded_frame(av_frame_alloc(), [](AVFrame* p){av_frame_free(&p);});
    // read frames from the file
    while (int ret=av_read_frame(av.fmt_ctx.get(), packet.get()) >= 0) {
            // check if the packet belongs to a stream we are interested in, otherwise
            // skip it
            if (packet->stream_index == av.audioStreamIndex)
                decodePacket(av.codec_ctx.get(), packet.get(), decoded_frame.get(), resultWav, av.is_eof());
            av_packet_unref(packet.get());
        }

        // flush the decoders
        decodePacket(av.codec_ctx.get(), nullptr, decoded_frame.get(), resultWav);

    int n_channels = av.codec_ctx->channels; // // c->ch_layout.nb_channels
    int sample_rate = av.codec_ctx->sample_rate;
    int64_t duration = av.fmt_ctx->duration;

    return {std::move(resultWav), duration, sample_rate};
}

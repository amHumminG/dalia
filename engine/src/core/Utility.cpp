#include "core/Utility.h"

#ifndef STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_HEADER_ONLY
#endif
#include <stb_vorbis.c>

namespace dalia {

    // A helper function to translate stb_vorbis error codes into readable strings
    const char* GetStbVorbisErrorString(int error) {
        switch (error) {
            case VORBIS__no_error:
                return "No Error";
            case VORBIS_need_more_data:
                return "Need More Data";
            case VORBIS_invalid_api_mixing:
                return "Invalid API Mixing";
            case VORBIS_outofmem:
                return "Out of Memory";
            case VORBIS_feature_not_supported:
                return "Feature Not Supported (Uses Floor 0)";
            case VORBIS_too_many_channels:
                return "Too Many Channels";
            case VORBIS_file_open_failure:
                return "File Open Failure";
            case VORBIS_seek_without_length:
                return "Seek Without Length";
            case VORBIS_unexpected_eof:
                return "Unexpected EOF (Truncated File?)";
            case VORBIS_seek_invalid:
                return "Seek Invalid (Past EOF)";
            case VORBIS_invalid_setup:
                return "Invalid Setup (Corrupt Vorbis Stream)";
            case VORBIS_invalid_stream:
                return "Invalid Stream (Corrupt Vorbis Stream)";
            case VORBIS_missing_capture_pattern:
                return "Missing Capture Pattern (Ogg Error)";
            case VORBIS_invalid_stream_structure_version:
                return "Invalid Stream Structure Version (Ogg Error)";
            case VORBIS_continued_packet_flag_invalid:
                return "Continued Packet Flag Invalid (Ogg Error)";
            case VORBIS_incorrect_stream_serial_number:
                return "Incorrect Stream Serial Number (Ogg Error)";
            case VORBIS_invalid_first_page:
                return "Invalid First Page (Ogg Error)";
            case VORBIS_bad_packet_type:
                return "Bad Packet Type (Ogg Error)";
            case VORBIS_cant_find_last_page:
                return "Can't Find Last Page (Ogg Error)";
            case VORBIS_seek_failed:
                return "Seek Failed (Ogg Error)";
            case VORBIS_ogg_skeleton_not_supported:
                return "Ogg Skeleton Not Supported";
            default:
                return "Unknown STB Vorbis Error Code";
        }
    }

}
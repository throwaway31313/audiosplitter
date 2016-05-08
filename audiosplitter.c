#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h> //TDO: remove after debug
#include <string.h>

// Project description: An command line argument is given by the user,
// which will then indicate the audio file he wishes to split. Based on a) The file name
// and b) the number of channels, the new audio files will be created. 
// ?? To decide: Encoding of the new files.
// ?? To decide: Name conventions


// Procedure for audio splitter:

// 1) Open file, establish AVStream, Codec context and thus the number of channels. 
// 2) Extract the AVStream into frames, and process them, based on AAC input.
// 3) Output to the files correctly. 
// 4) Expand to Vorbis, WMA and ALAC
// 5) Multithread.

// Reminders: Error control (incomplete files, wrong file names, etc.)
//            Clear to understand code.
//            Git commit
//            

// Files
static char * src_filename = NULL;

// Necessary structure definitions
static AVFormatContext *pFormatCtx = NULL;
static AVCodecContext *pAudioCodecCtx = NULL; // will point to the audio context;
static AVCodec *pAudioCodec = NULL;
static AVStream *pAudioStream = NULL; 

static AVFrame *pFrame = NULL;
static AVPacket pkt;

static int audio_stream_idx = -1;


// Opens the context, making sure that a proper audio stream is found and read.
int open_codec_context(int *stream_idx, AVFormatContext *pFormatCtx, enum AVMediaType type);

int main(int argc, char *argv[]){
    
    int ret = -1;
    int numOfChannels = 0;
    
    if(argc != 2){
        fprintf(stderr,"\nusage: %s input_file\n\n",argv[0]);
    }
    src_filename = argv[1];
    
    av_register_all(); // registers all available formats and codecs with the library.
                       // To use automatically.
                       
                       // TODO: Include other Codecs
                       
    // Open file to obtain AVFormatContext:
    if(avformat_open_input(&pFormatCtx,src_filename, NULL,NULL) < 0){
        fprintf(stderr,"Could not open source file\n");
        exit(1);
    }
    
    // Retrieve stream information. This is what really reads the stream packets (?).
    // This area will have to be changed in order to support the other codecs.
    if(avformat_find_stream_info(pFormatCtx, NULL)< 0){
        fprintf(stderr,"Could not find stream\n");
        exit(1);
    }
    
    // Dump information about file onto stderr:
    av_dump_format(pFormatCtx,0,src_filename,0);
    
    
    if (open_codec_context(&audio_stream_idx, pFormatCtx, AVMEDIA_TYPE_AUDIO) >= 0) {
        pAudioStream = pFormatCtx->streams[audio_stream_idx];
        pAudioCodecCtx = pAudioStream->codec;
        
    }
    
    

    numOfChannels = pFormatCtx->streams[audio_stream_idx]->codec->channels;
    
    // loop to create the files
    int i;
    for(i = 0; numOfChannels > 0 && i < numOfChannels;i++){
        
    } 
    
    
    
    uint64_t chanId =  pFormatCtx->streams[audio_stream_idx]->codec->channel_layout;
    printf("chan layout:  %04x\n\n", chanId);
    
    
    // The chanId can be used to map back to the desired channels using the table below.
    // For example, if chanId = 0x003f, this means that the channels are
    // FL, FR, FC, LF, BL, BR. 
    
    // TO DO: implement this to name files meaningfully.
    
//    49 #define AV_CH_FRONT_LEFT             0x00000001
//    50 #define AV_CH_FRONT_RIGHT            0x00000002
//    51 #define AV_CH_FRONT_CENTER           0x00000004
//    52 #define AV_CH_LOW_FREQUENCY          0x00000008
//    53 #define AV_CH_BACK_LEFT              0x00000010
//    54 #define AV_CH_BACK_RIGHT             0x00000020
//    55 #define AV_CH_FRONT_LEFT_OF_CENTER   0x00000040
//    56 #define AV_CH_FRONT_RIGHT_OF_CENTER  0x00000080
//    57 #define AV_CH_BACK_CENTER            0x00000100
//    58 #define AV_CH_SIDE_LEFT              0x00000200
//    59 #define AV_CH_SIDE_RIGHT             0x00000400
//    60 #define AV_CH_TOP_CENTER             0x00000800
//    61 #define AV_CH_TOP_FRONT_LEFT         0x00001000
//    62 #define AV_CH_TOP_FRONT_CENTER       0x00002000
//    63 #define AV_CH_TOP_FRONT_RIGHT        0x00004000
//    64 #define AV_CH_TOP_BACK_LEFT          0x00008000
//    65 #define AV_CH_TOP_BACK_CENTER        0x00010000
//    66 #define AV_CH_TOP_BACK_RIGHT         0x00020000
//    67 #define AV_CH_STEREO_LEFT            0x20000000  ///< Stereo downmix.
//    68 #define AV_CH_STEREO_RIGHT           0x40000000  ///< See AV_CH_STEREO_LEFT.


    
    
    
    
    // TODO: remove reference
    // int av_read_frame	(	AVFormatContext * 	s,
    //     AVPacket * 	pkt     
    // )	
        
    
}

int open_codec_context(int *stream_idx, AVFormatContext *pFormatCtx, enum AVMediaType type){
    int ret, strIndex;
    
    AVStream *pAvStream;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    AVDictionary *opts = NULL;
    
    ret = av_find_best_stream(pFormatCtx,type,-1,-1,NULL,0);
    if(ret<0){
        fprintf(stderr,"Could not find %s in input file '%s'",av_get_media_type_string(type),src_filename);
    }
    else{
        strIndex = ret;
        pAvStream = pFormatCtx->streams[strIndex]; // Guaranteed to be audio.
        
        // Find decoder:
        pCodecCtx = pAvStream->codec;
        pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
        //TODO: REMEMBER THE ALAC AND WMA FORMATS! THIS RIGHT HERE MUST BE CHANGED!!!
        if(!pCodec){
            fprintf(stderr,"Oops! The codec %s is not currently implemented\n"
            ,av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        // Do I need to init the decoders?
        
        *stream_idx = strIndex;
    }
    
    return 0;
    
    // TODO: Remove this later.
    // int av_find_best_stream	(	AVFormatContext * 	ic,
        // enum AVMediaType 	type,
        // int 	wanted_stream_nb,
        // int 	related_stream,
        // AVCodec ** 	decoder_ret,
        // int 	flags 
    // )	
    
    
}

/* 
REFERENCES:
http://ffmpeg.org/doxygen/trunk/demuxing_decoding_8c-example.html
http://dranger.com/ffmpeg/tutorial01.html
http://ffmpeg.org/doxygen/trunk/index.html

*/
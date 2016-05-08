#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/timestamp.h>

#include <stdio.h> //TDO: remove after debug
#include <string.h>
#include <stdlib.h>

// Project description: An command line argument is given by the user,
// which will then indicate the audio file he wishes to split. Based on a) The file name
// and b) the number of channels, the new audio files will be created. 
// ?? To decide: Encoding of the new files.
// ?? To decide: Name conventions


// Procedure for audio splitter:

// 1) Open file, establish AVStream, Codec context and thus the number of channels.  ... Done!
// 2) Extract the AVStream into frames, and process them, based on AAC input.
// 3) Output to the files correctly. 
// 4) Expand to Vorbis, WMA and ALAC ... No need!
// 5) Multithread.

// Reminders: Error control (incomplete files, wrong file names, etc.)
//            Clear to understand code.
//            Git commit
//            


// Queue strucure. 
// Example used from http://dranger.com/ffmpeg/tutorial03.html
// The current idea is to have this structure ready for when 
// we begin to create the callback functions for each channel.

typedef struct PacketQueue{
    AVPacketList *first_pkt, *last_pkt;
    
    //TODO: remove reference
    // typedef struct AVPacketList {
    //   AVPacket pkt;
    //   struct AVPacketList *next;
    // } AVPacketList;
    
    int nb_packets;
    int size;
    // SDL_mutex *mutex;   ---> Not using SDL at he moment
    // SDL_cond *cond;
} PacketQueue;

//Queue function protoypes:
void packet_queue_init(PacketQueue *q);
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);

// Opens the context, making sure that a proper audio stream is found and read.
int open_codec_context(int *stream_idx, AVFormatContext *pFormatCtx, enum AVMediaType type);
// decodes the audio packet into the global "frame"
//static int decode_audio_packet(int *got_frame,int cached);


// Files
static char * src_filename = NULL;

// Necessary structure definitions
static AVFormatContext *pFormatCtx = NULL;
static AVCodecContext *pAudioCodecCtx; // will point to the audio context;
static AVCodec *pAudioCodec = NULL;
static AVStream *pAudioStream = NULL; 

static AVFrame *pFrame = NULL;
static AVPacket packet;

static int audio_stream_idx = -1;
static int refcount = 0;
static int audio_frame_count = 0;

static int quit = 0;

// Main queue to store AVPackets
PacketQueue audioq;

int main(int argc, char *argv[]){
    
    packet_queue_init(&audioq);
    
    int ret = -1;
    int numOfChannels = 0;
    
    if(argc != 2){
        fprintf(stderr,"\nusage: %s input_file\n\n",argv[0]);
    }
    src_filename = argv[1];
    
    av_register_all(); // registers all available formats and codecs with the library.
                       // To use automatically.
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
    
    
    // Opens up the stream if there is an audio codec 
    if (open_codec_context(&audio_stream_idx, pFormatCtx, AVMEDIA_TYPE_AUDIO) >= 0) {
        pAudioStream = pFormatCtx->streams[audio_stream_idx];
        pAudioCodecCtx = pAudioStream->codec;
        printf("Check\n");
    }
    
    // numOfChannels = pFormatCtx->streams[audio_stream_idx]->codec->channels; 
    // numOfChannels will eventually determine the number of threads.
    numOfChannels = pAudioCodecCtx->channels;
    
    // loop to create the files. Ugly, but works for now.
    int i;  
    char * token;
    char * temp = malloc(20*sizeof(char));
    char * outNames[numOfChannels]; 
    token = "Output_";
    for(i = 0; numOfChannels > 0 && i < numOfChannels;i++){
        char a = i+'0';
        asprintf(&temp,"%s%c",token,a);
        outNames[i] = temp;
        // printf("%s\n",outNames[i]);
    } 
    
    
    
    uint64_t chanLayout =  pAudioCodecCtx->channel_layout;
    printf("chan layout:  %04x\n\n", chanLayout);
    uint64_t chanId[numOfChannels]; 
    
    for(i = 0; i < numOfChannels; i ++){
        chanId[i] = av_channel_layout_extract_channel(chanLayout,i);
        printf("Channel %d: %s\n",i,av_get_channel_description(chanId[i]));
        printf("Channel name: %s\n",av_get_channel_name(chanId[i]));
    }
    
    // The chanId can be used to map back to the desired channels using the table below.
    // For example, if chanId = 0x003f, this means that the channels are
    // FL, FR, FC, LF, BL, BR. 
    
    // TO DO: implement this to name files meaningfully.
    
    // There are functions that can be used for this! 


    
    
    // TODO: remove reference
    // int av_read_frame	(	AVFormatContext * 	s,
    //     AVPacket * 	pkt     
    // )
    // This function returns the next frame of a stream. Does not validate 
    // That the frames are valid for the decoder. It will split what is stored 
    // in the file into frames and return one for each call.
    
    //Now we will read the packets. 
    
    pFrame = av_frame_alloc();
    if (!pFrame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
    }
    
    
    
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;
    
    
    int got_frame;
    // Reminder: audio_stream_idx contains already the id of the audiostream
    
    while(av_read_frame(pFormatCtx, &packet)>=0){
       if(packet.stream_index==audio_stream_idx){
           packet_queue_put(&audioq,&packet);
       }else av_free_packet(&packet);
    }
    
    // Flush cached frames
    packet.data = NULL;
    packet.size = 0;
    
    printf("Demuxing succeeded");
    
    
    
    // enum AVSampleFormat sample_format = pAudioCodecCtx->sample_fmt;
    // const char *fmt;
    
   //pFrame = av_frame_alloc();
   
   printf("DEBUG: nb_streams: %d\n", pFormatCtx->nb_streams);
        
    
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
        
        if(!pCodec){
            fprintf(stderr,"Oops! The codec %s is not currently implemented\n"
            ,av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        // Do I need to init the decoders?
               
        *stream_idx = strIndex; // sets the index for the audio stream
    }
    return 0;
}



/* PACKET QUEUE FUNCTIONS */

// TODO: Finish queue functions when using beginning multithreading stage.

void packet_queue_init(PacketQueue *q){
    memset(q,0,sizeof(PacketQueue)); // initialize with 0's
}
int packet_queue_put(PacketQueue *q, AVPacket *pkt){
    //Create temporary
    AVPacketList *temp;
    
    // Doxygen says function is deprecated. Try to find alternative if there are errors.
    if(av_dup_packet(pkt) < 0){
        return -1;
    }
    
    // Allocate memory
    temp = av_malloc(sizeof(AVPacketList));
    if(!temp) return -1;
    
    // Instantiate (necessary if queue is empty)
    temp->pkt = *pkt;
    temp->next = NULL;
    
    // Update last and first packets
    if(!q->last_pkt){
        q->first_pkt = temp;
    }else q->last_pkt->next = temp;
        q->last_pkt = temp;
    
    
    // Update other queue parameters
    q->nb_packets++;
    q->size+=temp->pkt.size; 
    
    
}
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block){ 
    // assumes a global variable "quit" 
   
    AVPacketList * temp;
    int ret;
    
    if(quit){
        ret = -1;
    }
    // retrieve first packet in Queue
    temp = q->first_pkt;
    
    // temp to first 
    if(temp){
        q->first_pkt = temp->next;
        if(!q->first_pkt) q->last_pkt = NULL;
        ret = 1;
        // update size and nb_packets
        q->size -= temp->pkt.size;
        q->nb_packets--;
        
        //free temp
        av_free(temp);
    }else if(!block){
        ret = 0;
    }    
}
    


// Decodes the packet and returns its value (?)
// Adapted from http://ffmpeg.org/doxygen/3.0/demuxing_decoding_8c-example.html
// static int decode_audio_packet(int *got_frame,int cached){
//     int ret = 0;
//     int decoded = packet.size; // references static variable
    
//     *got_frame = 0;
    
//     printf("--3.1--\n");
//     printf("** %d\n",pAudioCodecCtx->channels);
//     printf("** %d\n",*got_frame);
//     printf("** %d\n", packet.stream_index);
//     printf("** %d\n",audio_stream_idx);
    
//     if(packet.stream_index == audio_stream_idx){
//         ret = avcodec_decode_audio4(pAudioCodecCtx,pFrame,got_frame,&packet);
//         printf("--3.2--\n");
//         // avcodec_decode_audio4 doc: 
//         // Decode the audio frame of size avpkt->size from avpkt->data into frame.
        
//         if(ret < 0){
//             fprintf(stderr,"Error decoding audio frame (%s)\n", av_err2str(ret));
//         }
//         /* Some audio decoders decode only part of the packet, and have to be
//             * called again with the remainder of the packet data.
//             * Sample: fate-suite/lossless-audio/luckynight-partial.shn
//             * Also, some decoders might over-read the packet. */
//         decoded = FFMIN(ret,packet.size);
        
        
//         if(*got_frame){ // frame is valid
//             printf("--3.3--\n");
//             size_t unpadded_linesize = pFrame->nb_samples*av_get_bytes_per_sample(pFrame->format);
//             printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
//                     cached ? "(cached)" : "",
//                     audio_frame_count++, pFrame->nb_samples,
//                     av_ts2timestr(pFrame->pts, &pAudioCodecCtx->time_base));
//             //fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
//         }
//     }
//     printf("--3.4--\n");
//     if (*got_frame && refcount)
//         av_frame_unref(pFrame);

//     return decoded;
// }
/* 
REFERENCES:
http://ffmpeg.org/doxygen/trunk/demuxing_decoding_8c-example.html
http://dranger.com/ffmpeg/tutorial01.html
http://ffmpeg.org/doxygen/trunk/index.html

*/
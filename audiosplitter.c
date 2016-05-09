#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/timestamp.h>

#include <stdio.h> //TDO: remove after debug
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

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
static int get_format_from_sample_fmt(const char **fmt,
                                         enum AVSampleFormat sample_fmt);
// Opens the context, making sure that a proper audio stream is found and read.
int open_codec_context(int *stream_idx, AVFormatContext *pFormatCtx, enum AVMediaType type);
// decodes the audio packet into the global "frame"
static int decode_audio_packet(int *got_frame,int cached);


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
    
    int ret = 0;
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
        
    }
    
    // numOfChannels = pFormatCtx->streams[audio_stream_idx]->codec->channels; 
    // numOfChannels will eventually determine the number of threads.
    numOfChannels = pAudioCodecCtx->channels;
    
    
    // loop to create the files. Ugly, but works for now.
    int i;  
    char * token;
    char * temp = malloc(20*sizeof(char));
    char * outNames[numOfChannels]; 
    uint64_t chanLayout =  pAudioCodecCtx->channel_layout;
    printf("chan layout:  %04x\n\n", chanLayout);
    uint64_t chanId[numOfChannels]; 
    token = src_filename;
    
    FILE * outFile[numOfChannels]; // create one file for each channel.
    for(i = 0; numOfChannels > 0 && i < numOfChannels;i++){
        
        chanId[i] = av_channel_layout_extract_channel(chanLayout,i);
        asprintf(&temp,"%s_%s",av_get_channel_name(chanId[i]),token);
        outNames[i] = temp;
        outFile[i] = fopen(outNames[i],"wb");
        // printf("%s\n",outNames[i]);
    } 
    // TODO: remove reference
    // int av_read_frame	(	AVFormatContext * 	s,
    //     AVPacket * 	pkt     
    // )
    // This function returns the next frame of a stream. Does not validate 
    // That the frames are valid for the decoder. It will split what is stored 
    // in the file into frames and return one for each call.
    
    //Now we will read the packets. 
    
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;
    
    
    
    // Reminder: audio_stream_idx contains already the id of the audiostream
    
    // while(av_read_frame(pFormatCtx, &packet)>=0){
    //    if(packet.stream_index==audio_stream_idx){
    //        packet_queue_put(&audioq,&packet);
           
    //    }else av_free_packet(&packet);
    // }
    
    
    pFrame = av_frame_alloc();
    printf("pFrame samples: %d\npFrame format: %d\n",pFrame->nb_samples,pFrame->format);
    
    if (!pFrame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM); 
    }
    
    int got_frame;
    while(av_read_frame(pFormatCtx, &packet)>=0){
       AVPacket orig_pkt = packet;
       do{
           ret = decode_audio_packet(&got_frame,0);
           if(ret < 0) break;
           packet.data += ret;
           packet.size -= ret;
           
           if(got_frame){
            size_t unpadded_linesize = pFrame->nb_samples * av_get_bytes_per_sample(pFrame->format);
            // printf("Packet data: %x\n", packet.data);
            // printf("pFrame samples/channels: %d\n",pFrame->nb_samples);
            // printf("pFrame channel layout: %d\n",pFrame->channel_layout);
            // printf("pFrame linesize: %d\n",pFrame->linesize[0]);
            // printf("Bytes per sample: %d\n", av_get_bytes_per_sample(pFrame->format));
            // printf("Unpadded linesize: %d",unpadded_linesize);
            
            
            // Write data for each channel to the specific file.
           
            for(i = 0; i < numOfChannels; i++){
                    fwrite(pFrame->extended_data[i],1,unpadded_linesize,outFile[i]);
                    //printf("pFrame data[%d]: %x\n" ,i,pFrame->extended_data[i]);
            }
            //fwrite(pFrame->extended_data[0],1,unpadded_linesize,outFile[0]);
            av_frame_unref(pFrame);
       }
       
       } while(packet.size>0);
       
       
       av_packet_unref(&orig_pkt);
    }
    // Flush cached frames
    packet.data = NULL;
    packet.size = 0;
    
    // Test queue
    // while(packet_queue_get(&audioq,&packet,0)>0){
    //     printf("Packet size: %d\n", packet.size);
    // }
    
    
    
    
    // Test read from queue
    // while(packet_queue_get(&audioq,&packet,0)>0){
        
    //     int ret = avcodec_decode_audio4(pAudioCodecCtx,pFrame,&got_frame,&packet);  
        
    //     if(ret < 0) fprintf(stderr,"Error decoding audio frame");
    //     if(got_frame){
    //         size_t unpadded_linesize = pFrame->nb_samples*av_get_bytes_per_sample(pFrame->format);
    //         for(i = 0; i < numOfChannels; i++){
    //             fopen(outNames[i],"wb");
    //             fwrite(pFrame->extended_data[i],1,unpadded_linesize,outFile[i]);
    //             // printf("pFrame data[%d]: %x\n" ,i,pFrame->extended_data[i]);
    //         }
    //     }
    //     av_frame_unref(pFrame);
    // }
    
    
    printf("Demuxing succeeded\n");
    
    if(pAudioStream){
        enum AVSampleFormat sfmt = pAudioCodecCtx->sample_fmt;
        
        const char * fmt;
        if(av_sample_fmt_is_planar(sfmt)){
            const char *packer = av_get_sample_fmt_name(sfmt);
            sfmt=av_get_packed_sample_fmt(sfmt);
        } 
        if((ret = get_format_from_sample_fmt(&fmt,sfmt))<0){
            fprintf(stderr ,"whoops\n" );
        }
        printf("Play the output audio file with the command:\n"
                "ffplay -f %s -ar %d %s\n",
                fmt, pAudioCodecCtx->sample_rate,
                "file_name");
    }
  
}

/* FUNCTION DEFINITIONS */ 

int open_codec_context(int *stream_idx, AVFormatContext *pFormatCtx, enum AVMediaType type){
    int ret, strIndex;
    
    AVStream *pAvStream;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    AVDictionary *opts = NULL;
    
    ret = av_find_best_stream(pFormatCtx,type,-1,-1,NULL,0);
    if(ret<0){
        fprintf(stderr,"Could not find %s in input file '%s'",av_get_media_type_string(type),src_filename);
        return ret;
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
        // Do I need to init the decoders? YES. I DO.
        av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
        if ((ret = avcodec_open2(pCodecCtx,pCodec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
               
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
        
        // update size and nb_packets
        q->size -= temp->pkt.size;
        q->nb_packets--;
        *pkt =temp->pkt;
        //free temp
        av_free(temp);
        ret = 1;
    }else if(!block){
        ret = 0;
    }    
    return ret;
}

// int audio_decode_frame(AVCodecContext * aCodecCtx, uint8_t *audio_buf, int buf_size){
//     static AVPacket pkt;
//     static uint8_t *audio_pkt_data = NULL;
//     static in audio_pkt_size = 0;
//     static AVFrame frame;
    
//     int len1, data_size = 0;
    
//     while(1){
//         while(audio_pkt_size > 0){
//             int got_frame = 0;
//             len1 = avcodec_decode_audio4(aCodecCtx, &frame, &got_frame, &pkt);
//             if(len1<0){
//                 audio_pkt_size = 0;
//                 fprintf(stderr,"Unable to decode audio frame\n");
//                 break;
//             }
//             audio_pkt_data += len1;
//             audio_pkt_size -= len1;
//             data_size = 0;
//             if(got_frame){
//                 data_size = av_samples_get_buffer_size(NULL,
//                 aCodecCtx->channels, frame.nb_samples, aCodecCtx->sample_fmt, 1);
//             }
//             assert(data_size<=buf_size);
//             memcpy(audio_buf,frame.data[0],data_size);
        
//         }
//         if(data_size <= 0) {
//             /* No data yet, get more frames */
//             continue;
//         }
//         /* We have data, return it and come back for more later */
//         return data_size;
//     }
//     if(pkt.data) av_free_packet(&pkt);
//     if(quit){
//         return -1;
//     }
//     if(packet_queue_get(&audioq,&pkt,1)<0){
//         return -1;
//     }
//     audio_pkt_data=pkt.data;
//     audio_pkt_size = pkt.size;
    
// }
// Decodes the packet and returns its value (?)
// Adapted from http://ffmpeg.org/doxygen/3.0/demuxing_decoding_8c-example.html
static int decode_audio_packet(int *got_frame,int cached){
    int ret = 0;
    int decoded = packet.size; // references static variable
    
    *got_frame = 0;
    
    if(packet.stream_index == audio_stream_idx){
        ret = avcodec_decode_audio4(pAudioCodecCtx,pFrame,got_frame,&packet);
        
        
        // avcodec_decode_audio4 doc: 
        // Decode the audio frame of size avpkt->size from avpkt->data into frame.
        
        if(ret < 0){
            fprintf(stderr,"Error decoding audio frame (%s)\n", av_err2str(ret));
        }
        /* Some audio decoders decode only part of the packet, and have to be
            * called again with the remainder of the packet data.
            * Sample: fate-suite/lossless-audio/luckynight-partial.shn
            * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret,packet.size);
        
    }
    

    return decoded;
}


// Borrowed from http://ffmpeg.org/doxygen/3.0/demuxing_decoding_8c-example.html
static int get_format_from_sample_fmt(const char **fmt,
                                         enum AVSampleFormat sample_fmt){
       int i;
       struct sample_fmt_entry {
           enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
       } sample_fmt_entries[] = {
           { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
           { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
           { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
           { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
          { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
       };
       *fmt = NULL;
   
       for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
           struct sample_fmt_entry *entry = &sample_fmt_entries[i];
           if (sample_fmt == entry->sample_fmt) {
               *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
               return 0;
           }
       }
   
       fprintf(stderr,
               "sample format %s is not supported as output format\n",
               av_get_sample_fmt_name(sample_fmt));
     return -1;
  }
/* 
REFERENCES:
http://ffmpeg.org/doxygen/trunk/demuxing_decoding_8c-example.html
http://dranger.com/ffmpeg/tutorial01.html
http://ffmpeg.org/doxygen/trunk/index.html

*/
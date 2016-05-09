#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/timestamp.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

// Project description: An command line argument is given by the user,
// which will then indicate the audio file he wishes to split. Based on a) The file name
// and b) the number of channels, the new audio files will be created. 

// The solution obtained is a single-threaded solution. However, a multithreaded solution
// was conceived and it is explained in detail:

// ==================================================
// Proposed solution for making use of multithreading:

/*
This program is often writing to 6 different files at the same time.
Since writing to a file is expensive, one thread will mostly be doing this during runtime.
A better approach would be to divide the file write into separate threads. 

Each thread is then associated to a single file. Now we need a mechanism for 
handing the information to the threads, so they can write to the file.

A proposed solution is to employ a consumer-producer scheme, through a queue. The main thread is
responsible for creating several queues and associate each queue to a thread.
The queue datapackets will provide the information to the thread when called upon.
A convenient "pthread_cond" signal will be set everytime a queue has a datapacket.

This way, each packet is processed into the file-writing threads as soon as they are 
indexed into the queues, leaving the main thread to decode the next AVPacket on the stream.

I did not implement this solution due to lack of time. I left some of the code to better
give an idea as to my implementation.
*/
 // ==================================================
 
 
// Procedure for audio splitter:

// 1) Open file, establish AVStream, Codec context and thus the number of channels. 
// 2) Extract the decoded AVPacketsinto frames, identify the channel data, and  input.
// 3) Output to the files correctly.

// Simple queue definitions (to be used in the multithreading scheme above)
typedef struct datapacket{
    int data;
    FILE * file;
    int linesize;
    struct datapacket * next;
} datapacket;

typedef struct queue{
    datapacket * first;
    datapacket * last;
    int packets;
} queue;

// Queue function prototypes
void queue_init(queue *q);
int queue_put(queue *q, int val, int linesize, FILE * file);
int queue_get(queue *q, datapacket *pack);

// Auxiliary function prototypes:
static int get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt);
static int open_codec_context(int *stream_idx, AVFormatContext *pFormatCtx, enum AVMediaType type);
static int decode_audio_packet(int *got_frame,int cached);


// Input file
static char * src_filename = NULL;

// Necessary structure definitions. Refer to http://ffmpeg.org/doxygen/3.0/ for documentation.
static AVFormatContext *pFormatCtx = NULL;
static AVCodecContext *pAudioCodecCtx; 
static AVCodec *pAudioCodec = NULL;
static AVStream *pAudioStream = NULL; 

static AVFrame *pFrame = NULL;
static AVPacket packet;

static int audio_stream_idx = -1; // Identifies the audio strea


int main(int argc, char *argv[]){
    
    // Helper variables
    int ret = 0;
    int i;
    int got_frame;
    int numOfChannels;
    
    if(argc != 2){
        fprintf(stderr,"\nusage: %s input_file\n\n",argv[0]);
        exit(1);
    }
    src_filename = argv[1]; // File name obtained from command line
    
    av_register_all(); // registers all available formats and codecs with the library.
                       // To use automatically.
                       
    // Open file to obtain AVFormatContext:
    if(avformat_open_input(&pFormatCtx,src_filename, NULL,NULL) < 0){
        fprintf(stderr,"Could not open source file\n");
        exit(1);
    }
    
    // Retrieve stream information. 
    if(avformat_find_stream_info(pFormatCtx, NULL)< 0){
        fprintf(stderr,"Could not find stream\n");
        exit(1);
    }
    // Dump information about file onto console:
    av_dump_format(pFormatCtx,0,src_filename,0);
    
    // Opens up the stream if there is an audio codec 
    if (open_codec_context(&audio_stream_idx, pFormatCtx, AVMEDIA_TYPE_AUDIO) >= 0) {
        pAudioStream = pFormatCtx->streams[audio_stream_idx];
        pAudioCodecCtx = pAudioStream->codec;
        
    }
    // Very important
    numOfChannels = pAudioCodecCtx->channels;
    
   
    // Now we determine the prefix for each file name based on the channel layout.
    // and name the files accordingly.
    char * token;
    char * temp = malloc(20*sizeof(char));
    char * outNames[numOfChannels]; 
    
    uint64_t chanLayout =  pAudioCodecCtx->channel_layout;
    fprintf(stderr, "chan layout:  %04x\n\n", chanLayout);
    uint64_t chanId[numOfChannels]; 
    token = src_filename;
    
    FILE * outFile[numOfChannels]; // create one file for each channel.
    for(i = 0; numOfChannels > 0 && i < numOfChannels;i++){
        // chanId gives the channel the audio is directed towards
        chanId[i] = av_channel_layout_extract_channel(chanLayout,i);
        asprintf(&temp,"%s_%s",av_get_channel_name(chanId[i]),token);
        outNames[i] = temp;
        outFile[i] = fopen(outNames[i],"wb"); // open file
    } 
    
    // Create queue and thread for each channel 
    // (not necessary in the actual program, but shown for proof of concept)
    queue channelQueue[numOfChannels];
    pthread_t pth[numOfChannels]; 
    for(i = 0; i < numOfChannels;i++){
        queue_init(&channelQueue[i]);
    }
    
    //Now we will read the packets. 
    
    // Create packet
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;
    
    // Allocate space for AVFrame 
    pFrame = av_frame_alloc();
    if (!pFrame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM); 
    }
    // Main loop for decoding the packets into frames
    while(av_read_frame(pFormatCtx, &packet)>=0){
       AVPacket orig_pkt = packet;
       do{
           ret = decode_audio_packet(&got_frame,0);
           if(ret < 0) break;
           packet.data += ret;
           packet.size -= ret;
           
           if(got_frame){
                size_t unpadded_linesize = pFrame->nb_samples * av_get_bytes_per_sample(pFrame->format);
                for(i = 0; i < numOfChannels; i++){
                        fwrite(pFrame->extended_data[i],1,unpadded_linesize,outFile[i]);
                        
                        // Add to queue
                        //queue_put(&channelQueue[i],pFrame->extended_data[i], unpadded_linesize, outFile[i]);
                }
                av_frame_unref(pFrame); // Clear frame
            }
       } while(packet.size>0);
       av_packet_unref(&orig_pkt); // Clear packet
    }
    // Flush cached frames
    packet.data = NULL;
    packet.size = 0;
    
    printf("Demuxing succeeded\n");
    
    // Show user the command for playing the uncompressed audio files.
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

// FUNCTION DEFINITIONS

/* Sets the Codec Context, which identifies the required codec and checks for implementation.  */
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
        av_dict_set(&opts, "refcounted_frames", "0", 0);
        if ((ret = avcodec_open2(pCodecCtx,pCodec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
               
        *stream_idx = strIndex; // sets the index for the audio stream
    }
    return 0;
}

// Adapted from http://ffmpeg.org/doxygen/3.0/demuxing_decoding_8c-example.html

/*Decodes the audio packets into the AVFrame struct. A proper codec is required for this 
function to run, and will not work if this is not done properly. */
static int decode_audio_packet(int *got_frame,int cached){
    int ret = 0;
    int decoded = packet.size; // references static variable
    *got_frame = 0;
    if(packet.stream_index == audio_stream_idx){
        ret = avcodec_decode_audio4(pAudioCodecCtx,pFrame,got_frame,&packet);
        if(ret < 0){
            fprintf(stderr,"Error decoding audio frame (%s)\n", av_err2str(ret));
        }
        decoded = FFMIN(ret,packet.size);
    }
    return decoded;
}


// Borrowed from http://ffmpeg.org/doxygen/3.0/demuxing_decoding_8c-example.html

/* Obtains the format from a sample_fmt entry. */
static int get_format_from_sample_fmt(
                        const char **fmt,
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
 
 
 /*Function that will perform the writing of the queue values on the corresponding file.
 Each thread will run this function with different arguments, that give the necessary
 information for writing the file. */
void * WriteToFile(void *args){
    datapacket * pack = (datapacket *)args;
}

// QUEUE FUNCTIONS

/* Initializes the Queue*/
void queue_init(queue *q){
    memset(q,0,sizeof(queue)); // initialize with 0's
}

/* Add element to queue */
int queue_put( queue *q ,int val, int line, FILE * f){
    datapacket *temp;
   
    temp = malloc(sizeof(datapacket));
    if(!temp) return -1;
    
    // set fields
    temp->data = val;
    temp->file = f;
    temp->linesize = line;
    temp->next = NULL;
    
    // update pointers
    if(!q->last) q->first = temp;
    else q->last->next = temp;
    q->last = temp;
    
    q->packets++;
    return 1;
}

/* Store values from first element in queue into the datapacket argument */
int queue_get(queue *q, datapacket *pack){
    datapacket *temp;
    int ret;
    temp = q->first;
    if(temp){
        q->first = temp->next;
        if(!q->first) q->last = NULL;
        
        q->packets--;
        ret = temp->data; 
        
        pack = temp;
        free(temp);
    }else ret = -1;
    return ret;
}



/* 
REFERENCES:
http://ffmpeg.org/doxygen/trunk/demuxing_decoding_8c-example.html
http://dranger.com/ffmpeg/tutorial01.html
http://ffmpeg.org/doxygen/trunk/index.html

*/
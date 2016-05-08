#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h> //TDO: remove after debug

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
static FILE * src_filename = NULL;

// Necessary structure definitions
static AVFormatContext *pFormatCtx = NULL;
static AVCodecContext *pCodecCtx = NULL;
static AVCodec *pAudioCodec = NULL;

static AVFrame *pFrame = NULL;
static AVPacket pkt;

int main(int argc, char *argv[]){
    
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
    
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)< 0){
        fprintf(stderr,"Could not find stream\n");
        exit(1);
    }
    
}


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
// 2) Extract the AVStream into frames, and process them.
// 3) Output to the files correctly. 
// 4) Multithread.

// Reminders: Error control (incomplete files, wrong file names, etc.)
//            Clear to understand code.
//            Git commit
//            



int main(int argc, char *argv[]){
    
    av_register_all(); // registers all available formats and codecs with the library.
                       // To use automatically.
                       
    AVFormatContext *pFormatCtx = NULL;
    
    
}


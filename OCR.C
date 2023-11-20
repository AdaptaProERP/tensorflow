//-----------------------------------------------------------------------------------
//Author:  Reinaldo Crespo
//reinaldo.crespo@gmail.com
//reinaldocrespo@structuredsystems.com
//reinaldo.crespo@icloud.com
//-----------------------------------------------------------------------------------


#include <string>

#include <stdlib.h>
#include <hbapi.h>
#include <hbapistr.h>
#include "hbvm.h"

#include <windows.h>

#include <stdio.h>
#include <time.h>
#include <TOCRdll.h>
#include <TOCRuser.h>
#include <TOCRerrs.h>


#include <conio.h>
#include <io.h>
#include <fcntl.h>
#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers

#pragma warning(disable : 4996)
#include "TOCRlangsel.h"

// for PDF
#include "DL_TOCRPDF.h"

#define MAX_LOG_SIZE    8192

typedef struct
{
   long JobNo    ;
   long JobType   ;
   long ErrorMode ;
   char * LogFileName ;
   long MaxLogSize;  
   long Orientation ;//from one of TOCRJOBORIENT_s from TOCRUser.h
   long CCThreshold; //0 to 100.  Values between .5 and .80 seems to work best.
                     //referr to Transym docs for more information on CCThreshold.

} HB_TOCRJOB;

static HB_TOCRJOB hb_tocrJob = { TOCRCONFIG_DEFAULTJOB,
                                 TOCRJOBTYPE_PDFFILE,
                                 TOCRERRORMODE_LOG,
                                 "tocr.log",
                                 MAX_LOG_SIZE,
                                 TOCRJOBORIENT_AUTO, 0.75 };

bool OCRWait(long JobNo, TOCRJOBINFO2 JobInfo2);
bool OCRWait(long JobNo, TOCRJOBINFO_EG JobInfoEg);
bool OCRWait_PDF(long JobNo, TOCRJOBINFO_EG& JobInfoEg, char* filename, TOCRPROCESSPDFOPTIONS_EG& pPdfOpts);
bool OCRPoll(long JobNo, TOCRJOBINFO2 JobInfo2);
bool GetResults(long JobNo, TOCRRESULTS **Results);
bool GetResults(long JobNo, TOCRRESULTSEX **Results);
bool GetResults(long JobNo, TOCRRESULTSEX_EG **Results);
bool FormatResults(TOCRRESULTS *Results, char *Msg);
bool FormatResults(TOCRRESULTSEX *Results, char *Msg);
bool FormatResults(TOCRRESULTSEX_EG *Results, wchar_t *Msg);

void LogData( const char * Msg );
void LogData( const wchar_t * Msg );
void trimTrailing(wchar_t *str);


HANDLE ConvertGlobalMemoryToMMF(HANDLE hMem);

//-----------------------------------------------------------------------------------------------------
HB_FUNC( TOCRSETUP )
{
   if( hb_pcount() > 0 ) {
      hb_tocrJob.JobType = HB_ISNUM( 1 ) ? hb_parnl( 1 ) : TOCRJOBTYPE_PDFFILE ;
   }

   if( hb_pcount() > 1 ) {
      hb_tocrJob.ErrorMode = HB_ISNUM( 1 ) ? hb_parnl( 1 ) : TOCRERRORMODE_LOG ;
   }

   if( hb_pcount() > 2 ){
      hb_tocrJob.LogFileName =  (char *) hb_parcx(3);
   }

   if( hb_pcount() > 3 ){
      hb_tocrJob.MaxLogSize =  hb_parnl(4);
   }

}

//-----------------------------------------------------------------------------------------------------
HB_FUNC( TOCRRESET )
{
   hb_retnl(TOCRShutdown(hb_tocrJob.JobNo));
}


//-----------------------------------------------------------------------------------------------------
//Parm #1 is filename with full path
//returns the number of pages
HB_FUNC( TOCRGETNUMPAGES )
{
   char * InputFile  = ( char * ) hb_parcx( 1 );  
   long NumPages     = 0 ;
   long Status ;
   long JobNo        = 0 ;
   char Msg[TOCRJOBMSGLENGTH];

   Status = TOCRSetConfig(TOCRCONFIG_DEFAULTJOB, TOCRCONFIG_DLL_ERRORMODE, hb_tocrJob.ErrorMode );
   sprintf(Msg, "inputfile %s JobType %ld Error Mode %ld\n", InputFile, hb_tocrJob.JobType, hb_tocrJob.ErrorMode );
   LogData( Msg ) ;

   //Status = TOCRSetConfigStr(TOCRCONFIG_DEFAULTJOB, TOCRCONFIG_LOGFILE, hb_tocrJob.LogFileName);
   //sprintf(Msg, "inputfile %s JobType %ld Status %ld Error Mode %ld\n",
   //InputFile, hb_tocrJob.JobType, Status, hb_tocrJob.ErrorMode );
   //LogData( Msg ) ;

   if ( Status == TOCR_OK ){

      Status = TOCRInitialise( &JobNo );
      hb_tocrJob.JobNo = JobNo ;

      sprintf(Msg, "TOCRInitialise Status %ld \n", Status );
      LogData( Msg ) ;

      if ( Status == TOCR_OK ) {

         Status = TOCRGetNumPages( JobNo, InputFile, hb_tocrJob.JobType, &NumPages);
         sprintf(Msg, "TOCRGetNumPages() Status %ld. Numb Pages %ld", Status, NumPages );
         LogData( Msg ) ;
      }

   }

   if ( Status != TOCR_OK ) {

      sprintf(Msg, "InputFile %s TOCRGetNumPages failed with error %ld", InputFile, Status);
      LogData( Msg ) ;

   } else {

      sprintf(Msg, "InputFile %s Number of Pages %ld", InputFile, NumPages);
      LogData( Msg);

   }

   TOCRShutdown( JobNo );
   hb_retnl(NumPages);

}


//-----------------------------------------------------------------------------------------------------
//parameters
// 1. input file with image
// 2. type of file to ocr defaults to TIFF
// 3. options mask defaults to 0
//returns OCRed text
// 4. Paper Orientation.  It defaults to AUTO
// 5. TOCR error mode.  It defautls to TOCR.log file
// 6. Page No in Inputfile to OCR

HB_FUNC( TOCRFROMFILE )
{
   TOCRJOBINFO_EG    JobInfo_EG;
   TOCRRESULTSEX_EG* Results = 0;
   long           Status;
   long           JobNo = 0;
   char           *InputFile = ( char * ) hb_parcx( 1 );   //parm 1 is input file
   long           PageNo      = ( HB_ISNUM( 2 ) ? hb_parnl( 2 ) : 1 )-1; //page num to OCR dimensioned from 0
   HB_WCHAR       Msg[10240];   //will contain OCRed text extracted from filename page #PageNo.
   char           LogMsg[ TOCRJOBMSGLENGTH ];

   //byte            OptionsMask = HB_ISNUM( 3 ) ? hb_parnl( 3 ) : 0x00000000 ;
   //long            Orientation = HB_ISNUM( 4 ) ? hb_parnl( 4 ) : TOCRJOBORIENT_AUTO;
   //long            ErrorMode   = HB_ISNUM( 5 ) ? hb_parnl( 5 ) : TOCRERRORMODE_LOG ;

   memset( Msg, 0, sizeof( Msg ) );  //make sure Msg is empty

   TOCRSetConfig( TOCRCONFIG_DEFAULTJOB, TOCRCONFIG_DLL_ERRORMODE, hb_tocrJob.ErrorMode ) ;
   //Status = TOCRSetConfigStr(TOCRCONFIG_DEFAULTJOB, TOCRCONFIG_LOGFILE, hb_tocrJob.LogFileName);
   //sprintf( LogMsg, "Inputfile: %s. TOCRSetConfig status: %d.  ErrorMode %d.  Page Numb: %ld",
   //      InputFile, Status, hb_tocrJob.ErrorMode, PageNo );
   //LogData( LogMsg ) ;

   memset( &JobInfo_EG, 0, sizeof( TOCRJOBINFO_EG ) );

   JobInfo_EG.JobType   = hb_tocrJob.JobType;
   JobInfo_EG.InputFile = InputFile ;
   JobInfo_EG.PageNo    = PageNo;

   //JobInfo_EG.ProcessOptions.MergeBreakOff = OptionsMask & 0x00000001 ;   //1 bit MergeBreakOff  ??
   //JobInfo_EG.ProcessOptions.DeskewOff = (OptionsMask>>1) & 0x00000010 ;    //2nd bit DeskewOff
   JobInfo_EG.ProcessOptions.Orientation = hb_tocrJob.Orientation ;
   JobInfo_EG.ProcessOptions.CCThreshold = 0.75; //try to darken colored font if in shades of grey

   Status = TOCRInitialise( &JobNo );
   hb_tocrJob.JobNo = JobNo ;

   if ( Status == TOCR_OK ) {
      if ( OCRWait( JobNo, JobInfo_EG ) ) {

         LogData("OCRWait successfull");

         if ( GetResults( JobNo, &Results ) )  {

            LogData("GetResults successfull");

            if ( FormatResults( Results, Msg ) ) {
               sprintf(LogMsg, "FormatResults  page %ld successful.  OCRed text below:", PageNo);
               LogData(LogMsg );
               LogData(Msg);

            }
            hb_xfree(Results);
         }
      }

      TOCRShutdown( JobNo );
      hb_tocrJob.JobNo = 0 ;

      LogData("TOCR JobNo has been shutdown.");

   }

   //harbour VM function to return wide strings wchar_t
   hb_retstr_u16( HB_CDP_ENDIAN_NATIVE, Msg );
   

   /*int length = wcslen( Msg );
   char * buffer = ( char* ) hb_xgrab( length + 1 * sizeof(wchar_t) );

   if ( buffer != NULL ) {

      wcstombs( buffer, Msg, length + 1 );   //wide char string  multy byte string
      hb_retc( buffer );
      hb_xfree( buffer );

   } else {
      hb_retc_null();
   }*/

   //fw_retWide( Msg ) ;
}


//-----------------------------------------------------------------------------------------------------
// V4 Version
bool OCRWait(long JobNo, TOCRJOBINFO2 JobInfo2)
{
    long                Status;
    long                JobStatus;
    //long              ErrorMode;
    char                Msg[TOCRJOBMSGLENGTH];

    Status = TOCRDoJob2(JobNo, &JobInfo2);
    if (Status == TOCR_OK) {
        Status = TOCRWaitForJob(JobNo, &JobStatus);
    }

    if (Status == TOCR_OK && JobStatus == TOCRJOBSTATUS_DONE)
    {
        return true;
    }
    else {
        // If something has gone wrong log the error message
        TOCRGetJobStatusMsg(JobNo, Msg);
      sprintf( Msg, "OCRWait failed with: %s", Msg );
      LogData( Msg ) ;

        return false;
    }
} // OCRWait()



//-----------------------------------------------------------------------------------------------------
// V5 version
bool OCRWait(long JobNo, TOCRJOBINFO_EG JobInfoEg)
{
    long                Status;
    long                JobStatus;
    //long              ErrorMode;
    char                Msg[TOCRJOBMSGLENGTH];

    Status = TOCRDoJob_EG(JobNo, &JobInfoEg);
    if (Status == TOCR_OK) {
      LogData("Now Waiting for Job");
      Status = TOCRWaitForJob(JobNo, &JobStatus);
      LogData("Done Waiting for Job");
    }

    if (Status == TOCR_OK && JobStatus == TOCRJOBSTATUS_DONE)
    {
        return true;
    }
    else {
        // If something has gone wrong display a message
        // (Check that the OCR engine hasn't already displayed a message)
        TOCRGetJobStatusMsg(JobNo, Msg);
      sprintf( Msg, "OCRWait failed with: %s", Msg );
      LogData( Msg ) ;

        return false;
    }
} // OCRWait()



//-----------------------------------------------------------------------------------------------------
bool OCRWait_PDF(long JobNo, TOCRJOBINFO_EG& JobInfoEg, char* filename, TOCRPROCESSPDFOPTIONS_EG& PdfOpts)
{
    long                Status;
    long                JobStatus;
    //long              ErrorMode;
    char                Msg[TOCRJOBMSGLENGTH];

    Status = TOCRDoJobPDF_EG(JobNo, &JobInfoEg, filename, &PdfOpts);
    if (Status == TOCR_OK) {
        Status = TOCRWaitForJob(JobNo, &JobStatus);
    }
   
    if (Status == TOCR_OK && JobStatus == TOCRJOBSTATUS_DONE)
    {
        return true;
    } else {
        // If something has gone wrong display a message
        // (Check that the OCR engine hasn't already displayed a message)
        TOCRGetJobStatusMsg(JobNo, Msg);
      sprintf( Msg, "OCRWait failed with: %s", Msg );
      LogData( Msg ) ;
        return false;

    }
} // OCRWait_PDF()



//-----------------------------------------------------------------------------------------------------
bool OCRPoll(long JobNo, TOCRJOBINFO2 JobInfo2)
{
    long                Status;
    long                Jobstatus;
    //long              ErrorMode;
    char                Msg[TOCRJOBMSGLENGTH];

    Status = TOCRDoJob2(JobNo, &JobInfo2);
    if (Status == TOCR_OK) {
        do {
            Sleep(100);
            Status = TOCRGetJobStatus(JobNo, &Jobstatus);
            if ( Status != TOCR_OK ) {
                sprintf(Msg, "OCRPoll failed - %d", Status);
            LogData(Msg);
            return false;
            }

        } while ( !Jobstatus );
    }
   
    if (Status == TOCR_OK && Jobstatus == TOCRJOBSTATUS_DONE)
    {
        return true;
    } else {
        // If something hass gone wrong display a message
        // (Check that the OCR engine hasn't already displayed a message)
        TOCRGetJobStatusMsg(JobNo, Msg);
      sprintf( Msg, "OCRWait failed with: %s", Msg );
      LogData( Msg ) ;

        return false;
    }
} // OCRPoll()




//-----------------------------------------------------------------------------------------------------
// Get the results from TOCR
bool getresults(long JobNo, long mode, void **Results)
{
    long                Status;
    long                ResultsInf;
    char                Msg[TOCRJOBMSGLENGTH];

    if ( mode == TOCRGETRESULTS_NORMAL_EG || mode == TOCRGETRESULTS_EXTENDED_EG ) {
        Status = TOCRGetJobResultsEx_EG(JobNo, mode, &ResultsInf, 0);
        if ( Status != TOCR_OK ) {
            sprintf(Msg, "TOCRGetJobResultsEx_EG failed - %d", Status);
        }
    } else {
        Status = TOCRGetJobResultsEx(JobNo, mode, &ResultsInf, 0);
        if ( Status != TOCR_OK ) {
            sprintf(Msg, "TOCRGetJobResultsEx failed - %d", Status);
        }
    }
    if ( Status != TOCR_OK ) {
      LogData(Msg);
      return false;
    }
    if ( ResultsInf > 0 ) {
        // Allocate memory for results

      *Results = ( char * ) hb_xgrab( ResultsInf + 1 );
        //*Results = (void *)malloc(ResultsInf * sizeof(unsigned char));
        if ( *Results ) {

            // Retrieve the results
            if ( mode == TOCRGETRESULTS_NORMAL_EG || mode == TOCRGETRESULTS_EXTENDED_EG ) {
                Status = TOCRGetJobResultsEx_EG(JobNo, mode, &ResultsInf, *Results);
                if ( Status != TOCR_OK ) {
                    sprintf(Msg, "TOCRGetJobResultsEx_EG failed - %d", Status);
                }
            } else {
                Status = TOCRGetJobResultsEx(JobNo, mode, &ResultsInf, *Results);
                if ( Status != TOCR_OK ) {
                    sprintf(Msg, "TOCRGetJobResultsEx failed - %d", Status);
                }
            }
            if ( Status != TOCR_OK ) {
            LogData(Msg);
                free(*Results);
                *Results = 0;
                return false;
            }
        } else {
         LogData("Failed to allocate memory for results");
            return false;
        }
    } else {
        //MessageBox(NULL, (LPCWSTR) "No results found\n", (LPCWSTR) "getresults", MB_TASKMODAL | MB_TOPMOST | MB_ICONSTOP);
      LogData("No results found");
   }
   
    return true;
} // getresults()




//-----------------------------------------------------------------------------------------------------
// Get normal results
bool GetResults(long JobNo, TOCRRESULTS **Results)
{
    return getresults(JobNo, TOCRGETRESULTS_NORMAL, (void **)Results);
} // GetResults()

// Get extended results
bool GetResults(long JobNo, TOCRRESULTSEX **Results)
{
    return getresults(JobNo, TOCRGETRESULTS_EXTENDED, (void **)Results);
} // GetResults()

// Get extended eg results
bool GetResults(long JobNo, TOCRRESULTSEX_EG **Results)
{
    return getresults(JobNo, TOCRGETRESULTS_EXTENDED_EG, (void **)Results);
} // GetResults()




//-----------------------------------------------------------------------------------------------------
// Convert results to a string
bool FormatResults(TOCRRESULTS *Results, char *Msg)
{
    long            ItemNo;
    long            APos = 0;
    bool            Status = false;

    if ( Results->Hdr.NumItems > 0 ) {
        for (ItemNo = 0; ItemNo < Results->Hdr.NumItems; ItemNo ++ ) {
            if ( Results->Item[ItemNo].OCRCha == '\r' )
                Msg[APos] = '\n';
            else
                Msg[APos] = (char)Results->Item[ItemNo].OCRCha;
            APos ++;
        }
        Msg[APos] = 0;
        Status = true;
    }

    return Status;

} // FormatResults()



//-----------------------------------------------------------------------------------------------------
// Convert extended results to a string
bool FormatResults(TOCRRESULTSEX *Results, char *Msg)
{
    long            ItemNo;
    long            APos = 0;
    bool            Status = false;

    if ( Results->Hdr.NumItems > 0 ) {
        for (ItemNo = 0; ItemNo < Results->Hdr.NumItems; ItemNo ++ ) {
            if ( Results->Item[ItemNo].OCRCha == '\r' )
                Msg[APos] = '\n';
            else
                Msg[APos] = (char)Results->Item[ItemNo].OCRCha;
            APos ++;
        }
        Msg[APos] = 0;
        Status = true;
    }

    return Status;
} // FormatResults()





//-----------------------------------------------------------------------------------------------------
// Convert extended results to a string
bool FormatResults(TOCRRESULTSEX_EG *Results, wchar_t *Msg)
{
    long            ItemNo;
    long            APos = 0;
    bool            Status = false;

    if ( Results->Hdr.NumItems > 0 ) {
        for (ItemNo = 0; ItemNo < Results->Hdr.NumItems; ItemNo ++ ) {
            if ( Results->Item[ItemNo].OCRCharWUnicode == L'\r' )
                Msg[APos] = L'\n';
            else
                Msg[APos] = Results->Item[ItemNo].OCRCharWUnicode;
            APos ++;
        }
        Msg[APos] = 0;
        Status = true;
    }

    return Status;
} // FormatResults()



//-------------------------------------------------------------------------------
HB_FUNC( TOCRFROMSCANNER )
{
   TOCRJOBINFO_EG    JobInfo_EG;
   TOCRRESULTSEX_EG* Results = 0;
    long                JobNo;
    long                Status;
    long                NumImages = 0;
    long                CntImages = 0;
    HANDLE          *hMems;
    HANDLE          hMMF ;
   HB_WCHAR       Msg[10240];  
   wchar_t       FullText[10240] = L"\n"; //final returning string
   char           LogMsg[ TOCRJOBMSGLENGTH ] ;
   //HB_WCHAR       pageBreak = L'\u000C';

   memset( Msg, 0, sizeof( Msg ) );  //make sure Msg is empty
   trimTrailing(FullText);

   //Sets Transym to log errors on tocrerr.log or screen dialog according to ErrorMode sent parameter
   TOCRSetConfig( TOCRCONFIG_DEFAULTJOB, TOCRCONFIG_DLL_ERRORMODE, hb_tocrJob.ErrorMode) ;
   //TOCRSetConfigStr(TOCRCONFIG_DEFAULTJOB, TOCRCONFIG_LOGFILE, hb_tocrJob.LogFileName);
   memset( &JobInfo_EG, 0, sizeof( TOCRJOBINFO_EG ) );

   JobInfo_EG.ProcessOptions.CCThreshold = 0.75; //try to darken colored font if in shades of grey

   Status = TOCRInitialise( &JobNo );
   hb_tocrJob.JobNo = JobNo ;

   if ( Status == TOCR_OK ) {
     
      Status = TOCRTWAINSelectDS(); // select the TWAIN device

      if ( Status == TOCR_OK) {

         TOCRTWAINShowUI(1);
         TOCRTWAINAcquire(&NumImages);

         if ( NumImages > 0 ) {

            hMems = (HANDLE *) hb_xgrab( NumImages * sizeof (HANDLE) );
            //hMems = (HANDLE *)malloc(NumImages * sizeof (HANDLE));
            if ( hMems ) {

               sprintf( LogMsg, "Number of images processing %ld. ", NumImages );
               LogData( LogMsg ) ;

               memset(hMems, 0, NumImages * sizeof(HANDLE));
               TOCRTWAINGetImages(hMems);

               for (long ImgNo = 0; ImgNo < NumImages; ImgNo++) {

                  //show progress using a callback func here
                  //callback func registered from harbour calling func.

                  if ( hMems[ImgNo] ) {

                     sprintf( LogMsg, "Processing image # %ld.", ImgNo );
                     LogData( LogMsg ) ;

                     // convert the memory block to a Memory Mapped File
                     hMMF = ConvertGlobalMemoryToMMF( hMems[ ImgNo ] );
                     // free the global memory block to save space
                     GlobalFree(hMems[ImgNo]);  //?????

                     if ( hMMF ) {

                        JobInfo_EG.JobType = TOCRJOBTYPE_MMFILEHANDLE;
                        JobInfo_EG.hMMF = hMMF;

                        if ( OCRWait(JobNo, JobInfo_EG) ) {

                           if ( GetResults(JobNo, &Results) ) {

                              if ( FormatResults( Results, Msg ) ) {

                                 FormatResults( Results, Msg );
                                 trimTrailing( Msg );
                                 wcscat( FullText, Msg );

                                 //MessageBoxW(NULL, Msg, L"Example 7", MB_TASKMODAL | MB_TOPMOST);
                                 LogData( FullText );
                                 hb_xfree( Results );

                              }
                           }
                        }
                        //place a page break between images
                        //wcscat(FullText, ( const wchar_t ) pageBreak );

                        CntImages ++;
                        CloseHandle(hMMF);
                     }
                  }
               }

               hb_xfree( hMems );

            }
         }
      }
   }

   TOCRShutdown( JobNo );
   hb_tocrJob.JobNo = 0 ;
    sprintf(LogMsg, "%d images successfully acquired\n", CntImages);
   LogData(LogMsg);

   hb_retstr_u16( HB_CDP_ENDIAN_NATIVE, FullText );
}


//-------------------------------------------------------------------------------
HANDLE ConvertGlobalMemoryToMMF(HANDLE hMem)
{
    void                *View ;
    HANDLE              hMMF ;
    void                *ucImg ;
    long                Status;

    Status = TOCR_OK + 1;

    hMMF = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)GlobalSize((void *)hMem), 0);
    if ( hMMF != NULL ) {
        View = (void *)MapViewOfFile(hMMF, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if ( View != NULL ) {
            ucImg = GlobalLock((void *)hMem);
            if ( ucImg ) {
                memcpy(View, ucImg, GlobalSize((void *)hMem) * sizeof(unsigned char));
                GlobalUnlock((void *)hMem);
                Status = TOCR_OK;
            }
            UnmapViewOfFile(View);
        }
    }

    if ( Status == TOCR_OK )
        return hMMF;
    else {
        if ( hMMF )
            CloseHandle(hMMF);
        return NULL;
    }

} // ConvertGlbalMemoryToMMF()



//------------------------------------------------------------------------------
// Remove trailing white space characters from string
//

void trimTrailing(wchar_t * str)
{
    int index, i;

    // Set default index
    index = -1;

    //Find last index of non-white space character
    i = 0;
    while(str[i] != '\0')
    {
        if(str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
        {
            index= i;
        }

        i++;
    }

    // Mark next character to last non-white space character as NULL
    str[index + 1] = '\0';
}


//This code uses std::wstring instead of a wide character array (wchar_t*) to represent
//the string, as it provides more convenient and safer string manipulation operations.
//It finds the last index of a non-white space character using the find_last_not_of
//member function, and then erases the trailing characters if found.
//If the string is empty or contains only white space, it clears the string.

void trimTrailing(std::wstring& str)
{
    // Find the last index of a non-white space character
    std::size_t index = str.find_last_not_of(L" \t\n");

    if (index != std::wstring::npos)
    {
        // Erase all characters from the index+1 position to the end
        str.erase(index + 1);
    }
    else
    {
        // If the string is empty or contains only white space, clear it
        str.clear();
    }
}




/*
//-------------------------------------------------------------------------------
void LogData( const char *Msg )
{
   time_t         currentTime;
   char timeString[ 100 ];
   size_t fileSize;
   FILE *file ;
   size_t maxFileSize = MAX_LOG_SIZE; // hb_tocrJob.MaxLogSize;
   const char *logfileName = "tocr.log";

   time( &currentTime );
   strftime( timeString, sizeof( timeString ), "%Y-%m-%d %H:%M:%S", localtime( &currentTime ) );



   // Open TOCR log file in append mode
   file = fopen( logfileName, "a");

   if ( file != NULL ) {  

      // Check the current file size and place pointer at eof
      fseek( file, 0L, SEEK_END );
      fileSize = ftell( file ) + strlen( Msg );

      if ( fileSize > maxFileSize ) {

            int bytesToKeep = maxFileSize - strlen(Msg);

            fseek(file, bytesToKeep, SEEK_SET);
            char *buffer = (char *)hb_xgrab(bytesToKeep);
            size_t elementsRead = fread(buffer, sizeof(char), bytesToKeep, file);
            fprintf(file, "\n%s", buffer, elementsRead);
            hb_xfree(buffer);
      }

      fprintf( file, "\n[%s] %s", timeString, Msg );
      fclose( file );
   }

} */


//-------------------------------------------------------------------------------
void LogData( const char *Msg )
{
   size_t len = mbstowcs( NULL, Msg, 0 );
   wchar_t *wideText = (wchar_t *)hb_xgrab((len + 1) * sizeof(wchar_t));
   mbstowcs(wideText, Msg, len + 1);
   LogData(wideText);

}

//-------------------------------------------------------------------------------
/*void LogData( const wchar_t *Msg )
{
   time_t         currentTime;
   char timeString[ 100 ];
   size_t fileSize;
   FILE *file ;
   int length = wcslen( Msg );
   const char* logfileName  = "tocr.log";
   size_t maxFileSize =  hb_tocrJob.MaxLogSize;  // MAX_LOG_SIZE;  //

   time( &currentTime );
   strftime( timeString, sizeof( timeString ), "%Y-%m-%d %H:%M:%S", localtime( &currentTime ) );

   // Open TOCR log file in append mode
   file = fopen( logfileName, "r+");

   if ( file != NULL ) {  

      fseek( file, 0, SEEK_END );
     
      // Check the current file size and place pointer at eof
      fileSize = ftell( file ) + length ;

      if ( fileSize > maxFileSize ) {

         //move file pointer to where data will be kept from.
         fseek(file, length - maxFileSize, SEEK_END);

         // Allocate memory for the buffer
         //need to allocate space for the wide characters, not just the length of the string
         wchar_t* buffer = (wchar_t*)hb_xgrab( maxFileSize * sizeof(wchar_t) );

         // Read the data from the file.
         //buffer is allocated with a size in bytes, you need to divide maxFileSize - length
         //by sizeof(wchar_t) to read the correct number of wide characters.
         fread(buffer, sizeof(wchar_t), (maxFileSize - length), file);

         //move file pointer back to the top to write old data being kept.
         //fseek(file, 0, SEEK_SET);
         fwprintf(file, L"\n:%ls", buffer );
         hb_xfree(buffer);
         //fseek( file, 0, SEEK_END );
      }

      fprintf(file, "\nlength %i. maxFileSize %ld.  fileSize %ld", length, maxFileSize, fileSize);
      fprintf(file, "\n[%s]", timeString);
      fwprintf( file, L"wchar_t:%ls", Msg );
      fclose( file );
   }

} */


#include <iostream>
#include <fstream>
#include <ctime>
#include <string>

void LogData(const wchar_t* Msg) {
   std::time_t currentTime = std::time(NULL);
   char timeString[100];
   size_t fileSize;
   std::string logfileName = hb_tocrJob.LogFileName;  //"tocr.log";
   size_t maxFileSize = hb_tocrJob.MaxLogSize ; // Maximum file size in bytes

   std::strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", std::localtime(&currentTime));

   // Open TOCR log file in append mode
   std::wifstream readFile(logfileName.c_str());

   if (readFile.is_open() ) {
      // Check the current file size
      readFile.seekg(0, std::ios::end);
      fileSize = readFile.tellg();

      if (fileSize > maxFileSize) {
         // Calculate the excess size to be removed
         size_t excessSize = fileSize - maxFileSize;
           
         //move file pointer to excessSize
         readFile.seekg(excessSize, std::ios::beg);

         // Create a temporary file to hold the truncated content
         std::string tempFileName = logfileName + ".tmp";
         std::wofstream tempFile(tempFileName.c_str());

         // Copy the remaining content to the temporary file
         tempFile << readFile.rdbuf() << "\n";
         tempFile.close();
         readFile.close();

         // Remove the original file
         std::remove(logfileName.c_str());

         // Rename the temporary file as the original file
         std::rename(tempFileName.c_str(), logfileName.c_str());


      } else {
         readFile.close() ;
      }

      std::wofstream writeFile(logfileName.c_str(), std::ios::app);
      if ( writeFile.is_open() ) {
         // Move the file pointer back to the end
         writeFile.seekp(0, std::ios::end);
         writeFile << "[" << timeString << "]";
         writeFile << Msg << "\n";
         writeFile.close();
      }
   }

}
 
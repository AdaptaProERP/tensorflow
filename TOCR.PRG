//-----------------------------------------------------------------------------------
//Author:  Reinaldo Crespo
//reinaldo.crespo@gmail.com
//reinaldocrespo@structuredsystems.com
//reinaldo.crespo@icloud.com
//-----------------------------------------------------------------------------------

#include "hbclass.ch"
#include "TOCRUser.h"

#xcommand DEFAULT <uVar1> := <uVal1> ;
   [, <uVarN> := <uValN> ] => ;
      If( <uVar1> == nil, <uVar1> := <uVal1>, ) ;;
    [ If( <uVarN> == nil, <uVarN> := <uValN>, ); ]

#define CRLF chr( 13 )  + chr( 10 )
#define _MAX_LOG_SIZE 8192

//-----------------------------------------------------------------------------------
//TOCRv51_functions.cpp
//Class to use Transym OCR software from a harbour application.
//you will also need TOCRv51_functions.cpp source file that includes
//the actual Harbour VM wrapper functions
//to use Transym TOCRDLL.dll.
//
//-----------------------------------------------------------------------------------
//Steps to follow to use this class:
//1. Instanciate an object using TOcr():New()
//2. Assign nJobType the appropriate values from tocrUser.h TOCRJOBTYPEs
//3. Assign the file name to be OCRed to cFileName and optionally assign nErrorMode a value
//4. run SetupJob() method
//5. run OCRFromFile that returns the OCRed text.
//
//if OCRing from scanneer, then these would be the steps to follow
//1. instanciate the object using Tocr():New()
//2. optionally assign nErrorMode a value from TOCRERRORMODEs from tocrUser.h
//3. run OCRFromScanner() method that returns OCRed text.
//
//optionally you may call SetupJob() sending up to 4 parameters.
//1 Jobtype = one of TOCRJOBTYPE_s from tocruser.h
//2. nErrorMode = one of TOCRERRORMODE_s from tocrUser.h
//4. cLogfile name is file where logmessages are to be sent to
//5. nMaxLogSize for maximum log file size before space is recycled.
//Or you can set these data and then call SetupJob() with no parameters.
//
//-----------------------------------------------------------------------------------
CLASS TOCR

   DATA nJobType     INIT TOCRJOBTYPE_PDFFILE
   DATA nNumPages    INIT 0
   DATA nMaxLogSize  INIT _MAX_LOG_SIZE
   DATA nErrorMode   INIT TOCRERRORMODE_LOG

   DATA cLogFile     INIT 'TOCR.log'
   DATA cFileName    INIT ''
   DATA cOCRText     INIT ''

   DATA bProgress

   METHOD New() INLINE SELF
   METHOD SetupJob( nJobtype, nErrorMode, cLogFile, nMaxLogSize )
   METHOD End(lAlsoKillAnyPendingJobs )

   METHOD OCRFromFile( cFileName )
   METHOD GetNumPages()

   METHOD OCRFromScanner()

   METHOD Reset( lAlsoKillAnyPendingJobs )

END CLASS


//-----------------------------------------------------------------------------------
METHOD End(lAlsoKillAnyPendingJobs )

   ::Reset( lAlsoKillAnyPendingJobs )
   SELF := NIL

RETURN NIL



//-----------------------------------------------------------------------------------
METHOD SetupJob( nJobtype, nErrorMode, cLogFile, nMaxLogSize )
   LOCAL cFileExt

   IF nJobtype != Nil ; ::nJobtype := nJobtype; ENDIF
   IF nErrorMode != Nil ; ::nErrorMode := nErrorMode ; ENDIF
   IF cLogFile != Nil ; ::cLogFile := cLogFile ; ENDIF
   IF nMaxLogSize != NIL; ::nMaxLogSize := nMaxLogSize ; ENDIF

   DEFAULT ::nJobType := TOCRJOBTYPE_PDFFILE
   DEFAULT ::nErrorMode := TOCRERRORMODE_LOG
   DEFAULT ::cLogFile := "tocr.log"
   DEFAULT ::nMaxLogSize := _MAX_LOG_SIZE

   IF file( ::cFileName )
      hb_FNameSplit( LOWER( ::cFileName ),,, @cFileExt )
      ::nJobType := iif( cFileExt = '.pdf', TOCRJOBTYPE_PDFFILE, TOCRJOBTYPE_TIFFFILE )
   ENDIF

   #ifdef TRANSYM
      TOCRSetup( ::nJobType, ::nErrorMode, ::cLogFile, ::nMaxLogSize )
   #endif

RETURN NIL



//-----------------------------------------------------------------------------------
METHOD GetNumPages()

   ::nNumPages := 0

   IF File( ::cFileName )

      #ifdef TRANSYM
         ::nNumPages := TocrGetNumPages( ::cFileName )
      #endif

   ENDIF

RETURN ::nNumPages


//-----------------------------------------------------------------------------------
METHOD OCRFromFile( cFileName )
   LOCAL nPage

   IF cFileName != NIL ; ::cFileName := cFileName ; ENDIF

   IF file( ::cFileName )

      IF ::nNumPages == 0
         #IFDEF TRANSYM
            ::GetNumPages()
         #ENDIF
      ENDIF

      IF ::nNumPages > 0

         FOR nPage := 1 TO ::nNumPages

            IF ::bProgress != NIL
               EVAL( ::bProgress, nPage )
            ENDIF
            #ifdef TRANSYM
               ::cOCRText += TOCRFromFile( ::cFileName, nPage ) + CRLF
            #ENDIF

         NEXT

      ENDIF

   ENDIF

RETURN ::cOCRText

//-----------------------------------------------------------------------------------
METHOD Reset( lAlsoKillAnyPendingJobs )

   DEFAULT lAlsoKillAnyPendingJobs := .F.
   #ifdef TRANSYM
      IF lAlsoKillAnyPendingJobs ; TOCRReset()  ;ENDIF
   #endif

   ::nNumPages := 0
   ::cOCRText  := ''
   ::cFileName := ''

RETURN NIL


//-----------------------------------------------------------------------------------
METHOD OCRFromScanner()
   #IFDEF TRANSYM
      ::cOCRText := TOCRFromScanner()
   #ENDIF
RETURN ::cOCRText

/*
METHOD ProcessEOBImages() CLASS TService
   LOCAL aDir := Directory( ::cEOBImagesPath + "*.pdf" )
   LOCAL cHistoryPath := ::cEOBImagesPath + "\history\"
   LOCAL cFileName, cExt, oOcr, cFile
   LOCAL cText, cCheckNum, dCheckDate
   LOCAL aClaimKeys, aKeys

   LogData( ::cLogFile, "ProcessEOBImages", ::nMaxLogSize )

   FOR EACH cFileName IN aDir

      cFileName := ::cEOBImagesPath + cFileName[ 1 ]

      hb_fNameSplit( LOWER( cFileName ),,@cfile, @cExt )

      oOcr := TOCR():New()

      oOcr:nJobtype     := iif( cExt = '.pdf', TOCRJOBTYPE_PDFFILE, TOCRJOBTYPE_TIFFFILE )
      oOcr:nErrorMode   := TOCRERRORMODE_LOG
      oOcr:cLogFile     := ::cLogFile
      oOcr:cFileName    := cFileName
      oOcr:SetupJob()
      oOcr:GetNumPages()

      oOcr:bProgress    := { |p| LogData( ::cLogFile, {cFileName, "Page", p }, ::nMaxLogSize )  }

      cText := oOcr:OCRFromFile()

      oOcr:Reset()
      oOcr:End()

*/

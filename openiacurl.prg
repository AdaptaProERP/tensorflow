#INCLUDE "hbClass.ch"
#INCLUDE "hbcurl.ch"

#DEFINE CRLF CHR(13)+CHR(10)

CLASS TOpenAI

   DATA hCurl
   DATA httpcode
   DATA nError             INIT 0

   DATA Response
   DATA Prompt

   DATA model              INIT "text-davinci-003"
   DATA cUrl               INIT "https://api.openai.com/v1/completions"
   DATA max_tokens         INIT 2048
   DATA temperature        INIT 0
   DATA top_p              INIT 1
   DATA frequency_penalty  INIT 0.00
   DATA presence_penalty   INIT 0.00

   DATA cLogFile        INIT "TOpenAI.log"
   DATA nMaxLogSize     INIT 32768
   DATA lDebug          INIT .F.

   METHOD New()
   METHOD End()
   METHOD Send()
   METHOD Reset()

ENDCLASS


//------------------------------------------------------------------------------------------------
METHOD New() CLASS TOpenAI

   ::hCurl := curl_easy_init()

RETURN Self


//------------------------------------------------------------------------------------------------
METHOD Send() CLASS TOpenAI
   LOCAL aheaders
   Local httpcode
   Local cJson
   Local h := { => }

   curl_easy_setopt( ::hCurl, HB_CURLOPT_POST, .T. )
   curl_easy_setopt( ::hCurl, HB_CURLOPT_URL, ::cUrl )

   //curl_easy_setopt( ::hCurl, HB_CURLOPT_PASSWORD, OPENAI_API_KEY )
   //http header could also be an array of headers
   aheaders := { "Content-Type: application/json", ;
                   "Authorization: Bearer " + OPENAI_API_KEY }

   curl_easy_setopt( ::hCurl, HB_CURLOPT_HTTPHEADER, aheaders )

   curl_easy_setopt( ::hCurl, HB_CURLOPT_USERNAME, '' )
   curl_easy_setopt( ::hCurl, HB_CURLOPT_DL_BUFF_SETUP )
   curl_easy_setopt( ::hCurl, HB_CURLOPT_SSL_VERIFYPEER, .F. )

   hb_HSet( h, "model", ::model )
   hb_HSet( h, "prompt", ::Prompt )
   hb_HSet( h, "max_tokens", ::max_tokens )
   hb_HSet( h, "temperature", ::temperature )
   hb_HSet( h, "frequency_penalty", ::frequency_penalty )
   hb_HSet( h, "presence_penalty", ::presence_penalty )

   cJson := hb_jsonEncode( h )
   curl_easy_setopt( ::hcurl, HB_CURLOPT_POSTFIELDS, cJson ) //cPostFields )

   ::nError := curl_easy_perform( ::hCurl )
   curl_easy_getinfo( ::hCurl, HB_CURLINFO_RESPONSE_CODE, @httpcode )

   ::httpcode := httpcode

   IF ::nError == HB_CURLE_OK

      ::Response = curl_easy_dl_buff_get( ::hCurl )

      IF ::lDebug
         LogData( ::cLogFile, { "prompt", ::Prompt, "model", ::model }, ::nMaxLogSize )
         LogData( ::cLogFile, ::Response, ::nMaxLogSize )
      ENDIF
   Else

      LogData( ::cLogFile, { "TTOpenAI prompt was:", ::Prompt }, ::nMaxLogSize )
      LogData( ::cLogFile, { "Error Num:", ::nError, "Httpcode:", ::httpcode }, ::nMaxLogSize )
      LogData( ::cLogFile, { "Response:", ::Response }, ::nMaxLogSize )
      LogData( ::cLogFile, curl_easy_strerror( ::nError ), ::nMaxLogSize )

   ENDIF

return ::Response

//------------------------------------------------------------------------------------------------
METHOD Reset() CLASS TOpenAI

   curl_easy_reset( ::hCurl )
   ::nError := HB_CURLE_OK
   ::Response := Nil

return NIL



//------------------------------------------------------------------------------------------------
METHOD End() CLASS TOpenAI

   curl_easy_cleanup( ::hCurl )
   ::hCurl := Nil

return NIL


/*------------------------------------------------------------------------------------------------
typeical response:
{
    "id": "cmpl-7EOhp7sTGKvQv0hsntGbg41BdadX9",
    "object": "text_completion",
    "created": 1683665917,
    "model": "text-davinci-003",
    "choices": [
        {
            "text": "\r\nICD-10 Diagnosis Codes: \r\n1. S52.521A - Fracture of lower end of right radius, initial encounter for closed fracture \r\n2. S52.522A - Fracture of lower end of right ulna, initial encounter for closed fracture \r\n3. S52.531A - Fracture of shaft of right radius, initial encounter for closed fracture \r\n4. S52.532A - Fracture of shaft of right ulna, initial encounter for closed fracture",
            "index": 0,
            "logprobs": null,
            "finish_reason": "stop"
        }
    ],
    "usage": {
        "prompt_tokens": 417,
        "completion_tokens": 113,
        "total_tokens": 530
    }
}
-------------------------------------------------------------------------------------------------*/
/*
CLASS TCodeICD10sWOpenAI FROM TOpenAI
   METHOD GetICD10sFromResponse()
END CLASS

//#DEFINE _ONLY_ICD10S "\b[A-TV-Z]\d{2}\.\d{1,4}[A-Z]?\b\s*-\s*.+"
#DEFINE _ONLY_ICD10S "\s*[A-TV-Z]\d{2}\.\d{1,4}[A-Z]*\b\s*-\s.+"
METHOD GetICD10sFromResponse() CLASS TCodeICD10sWOpenAI
   LOCAL aICD10s := {}
   LOCAL aParsed, iter, acodelines, CodeLine
   LOCAL a := {}
   LOCAL aKeys, aCleanLn
   LOCAL hResponse := hb_jsonDecode( ::Response )

   if ::lDebug

      LogData( ::cLogFile, { "hb_isHash( hResponse )", hb_isHash( hResponse ), ;
                           "hb_HHasKey( hResponse, 'choices' )", hb_HHasKey( hResponse, "choices" ) } )
      aKeys := HGetKeys( hResponse )
      LogData( ::cLogFile, aKeys, ::nMaxLogSize )

   ENDIF


   IF hb_isHash( hResponse ) .AND. ;
      hb_HHasKey( hResponse, "choices" )

      if HB_ISARRAY( hResponse[ "choices" ] )
         a := hResponse[ "choices" ]
      endif

      FOR iter := 1 TO LEN( a )

         IF hb_HHasKey( a[ iter ], "text" )

            acodelines := hb_ATokens( a[ iter ]["text"], CRLF )

            FOR EACH CodeLine IN acodelines
               aCleanLn := hb_RegEx( _ONLY_ICD10S, CodeLine )

               IF Len( aCleanLn ) > 0

                  aParsed := hb_ATokens( aCleanLn[ 1 ], "-" )
                  AADD( aIcd10s, aParsed )

               ENDIF

            NEXT

         ENDIF

      NEXT iter

   ENDIF

RETURN aICD10s
*/

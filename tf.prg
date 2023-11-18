#include "FiveWin.ch"

#define TF_INT32   3
#define TF_STRING  7
#define TF_INT64   9

static hDLL

function Main()

   local oTF := TensorFlow():New()

   MsgInfo( oTF:Version() )

   oTF:Run( oTF:Add( oTF:Constant( 5, "Five" ), oTF:Constant( 3, "three" ) ) )

   MsgInfo( oTF:Output() )

   oTF:End()

return nil

CLASS TensorFlow

   DATA hGraph
   DATA hOptions
   DATA hStatus
   DATA hSession
   DATA hTensorOutput

   METHOD New()
   
   METHOD Add( hOperation1, hOperation1, cName )
   
   METHOD MatMul( hOperation1, hOperation2, cName )

   METHOD Version() INLINE TF_Version()

   METHOD ImportGraph( cFileName )

   METHOD TensorNum( nValue )

   METHOD TensorString( cString )

   METHOD End()
   
   METHOD StatusCode() INLINE TF_GetCode( ::hStatus )

   METHOD Variable( cVarName, nDataType )

   METHOD Constant( uValue, cName )

   METHOD Run( hOperation )

   METHOD Output()

ENDCLASS

METHOD New() CLASS TensorFlow

   hDLL = LoadLibrary( "tensorflow.dll" )

   ::hGraph = TF_NewGraph()
   ::hOptions = TF_NewSessionOptions()
   ::hStatus = TF_NewStatus()
   ::hSession = TF_NewSession( ::hGraph, ::hOptions, ::hStatus )
     
return Self    

METHOD End() CLASS TensorFlow

   TF_CloseSession( ::hSession, ::hStatus )
   TF_DeleteSession( ::hSession, ::hStatus )
   TF_DeleteStatus( ::hStatus )
   TF_DeleteSessionOptions( ::hOptions )

   FreeLibrary( hDLL )

return nil

METHOD Add( hOperation1, hOperation2, cName ) CLASS TensorFlow

   local hOperationDescription := TF_NewOperation( ::hGraph, "AddN", If( cName == nil, "AddN", cName ) )
   local hOperation, hOutput 
   
   TF_AddInputList( hOperationDescription, hOutput := TF_Output2( hOperation1, hOperation2 ), 2 )

   hOperation = TF_FinishOperation( hOperationDescription, ::hStatus )
   
   hb_xfree( hOutput )
   
   if TF_GetCode( ::hStatus ) != 0
      MsgAlert( TF_Message( ::hStatus ), "Error creating AddN operator: " + Str( TF_GetCode( ::hStatus ) ))
   endif
   
return hOperation

METHOD MatMul( hOperation1, hOperation2, cName ) CLASS TensorFlow

   local hOperationDescription := TF_NewOperation( ::hGraph, "MatMul", If( cName == nil, "MatMul", cName ) )
   local hOperation, hOutput1, hOutput2
   
   TF_AddInput( hOperationDescription, hOutput1 := TF_Output( hOperation1 ) )
   TF_AddInput( hOperationDescription, hOutput2 := TF_Output( hOperation2 ) )

   hOperation = TF_FinishOperation( hOperationDescription, ::hStatus )

   hb_xfree( hOutput1 )
   hb_xfree( hOutput2 )

   if TF_GetCode( ::hStatus ) != 0
      MsgAlert( TF_Message( ::hStatus ), "Error creating 0perator: " + Str( TF_GetCode( ::hStatus ) ))
   endif
   
return hOperation

METHOD ImportGraph( cFileName ) CLASS TensorFlow

   local cGraph := MemoRead( cFileName )
   local hGraphDefOptions := TF_NewImportGraphDefOptions()
   
   TF_GraphImportGraphDef( ::hGraph, TF_NewBufferFromString( cGraph, Len( cGraph ) ), hGraphDefOptions, ::hStatus )
   
   if TF_GetCode( ::hStatus ) != 0
      MsgAlert( TF_Message( ::hStatus ), "Error importing a Graph file" )
   endif
   
   TF_DeleteImportGraphDefOptions( hGraphDefOptions )
   
return nil   

METHOD TensorNum( nValue ) CLASS TensorFlow

   local hTensor := TF_AllocateTensor( TF_INT32, 0, 0, 8 )
   
   Memset( TF_TensorData( hTensor ), 0, 8 )
   Memcpy( TF_TensorData( hTensor ), L2Bin( nValue ), Len( L2Bin( nValue ) ) )
   
return hTensor

METHOD TensorString( cString ) CLASS TensorFlow

   local hTensor := TF_AllocateTensor( TF_STRING, 0, 0, 8 + TF_StringEncodedSize( Len( cString ) ) )
   
   Memset( TF_TensorData( hTensor ), 0, 8 )
   
   TF_StringEncode( cString, Len( cString ), 8 + TF_TensorData( hTensor ), TF_StringEncodedSize( Len( cString ) ), ::hStatus )

   if TF_GetCode( ::hStatus ) != 0
      MsgAlert( TF_Message( ::hStatus ), "Error creating a Tensor string" )
   endif
   
return hTensor   

METHOD Variable( cVarName, nDataType ) CLASS TensorFlow

   local hOperationDescription := TF_NewOperation( ::hGraph, "Variable", cVarName )

   TF_SetAttrType( hOperationDescription, "dtype", nDataType )

return TF_FinishOperation( hOperationDescription, ::hStatus )

METHOD Constant( uValue, cName ) CLASS TensorFlow

   local hOperationDescription, hTensor
   
   do case
      case ValType( uValue ) == "C"
         hTensor = ::TensorString( uValue )
         
      case ValType( uValue ) == "N"
         hTensor = ::TensorNum( uValue )
   endcase

   hOperationDescription = TF_NewOperation( ::hGraph, "Const", cName )
   TF_SetAttrTensor( hOperationDescription, "value", hTensor, ::hStatus )
   TF_SetAttrType( hOperationDescription, "dtype", TF_TensorType( hTensor ) )

return TF_FinishOperation( hOperationDescription, ::hStatus )

METHOD Run( hOperation ) CLASS TensorFlow

   local hTensorOutput := 0, hOutput

   TF_SessionRun( ::hSession, 0,;
                  0, 0, 0,;  // Inputs     
                  hOutput := TF_Output( hOperation, 0 ), @hTensorOutput, 1,;  // Outputs
                  hOperation, 1,;  // Operations
                  0, ::hStatus )

   hb_xfree( hOutput )
   
   ::hTensorOutput = hTensorOutput

return nil

METHOD Output() CLASS TensorFlow

   local nType := TF_TensorType( ::hTensorOutput )
   local uValue
   
   do case
      case nType == TF_STRING 
         uValue = TF_TensorString( TF_TensorData( ::hTensorOutput ) )
         
      case nType == TF_INT64 .or. nType == TF_INT32  
         uValue = TF_TensorNum( TF_TensorData( ::hTensorOutput ) )
         
      otherwise
         MsgAlert( "type not supported in Method Output yet", nType )
   endcase
   
return uValue

DLL FUNCTION TF_Version() AS LPSTR LIB hDLL

DLL FUNCTION TF_NewGraph() AS LONG LIB hDLL

DLL FUNCTION TF_NewSessionOptions() AS LONG LIB hDLL

DLL FUNCTION TF_NewStatus() AS LONG LIB hDLL

DLL FUNCTION TF_NewSession( hGraph AS LONG, hOptions AS LONG, hStatus AS LONG ) AS LONG LIB hDLL

DLL FUNCTION TF_CloseSession( hSession AS LONG, hStatus AS LONG ) AS VOID LIB hDLL

DLL FUNCTION TF_DeleteSession( hSession AS LONG, hStatus AS LONG ) AS VOID LIB hDLL

DLL FUNCTION TF_DeleteStatus( hStatus AS LONG ) AS VOID LIB hDLL

DLL FUNCTION TF_DeleteSessionOptions( hOptions AS LONG ) AS VOID LIB hDLL

DLL FUNCTION TF_NewImportGraphDefOptions() AS LONG LIB hDLL

DLL FUNCTION TF_GraphImportGraphDef( hGraph AS LONG, hBuffer AS LONG, hGraphDefOptions AS LONG, hStatus AS LONG ) AS LONG LIB hDLL

DLL FUNCTION TF_NewBufferFromString( cString AS LPSTR, nLegth AS LONG ) AS LONG LIB hDLL

DLL FUNCTION TF_GetCode( hStatus AS LONG ) AS LONG LIB hDLL

DLL FUNCTION TF_DeleteImportGraphDefOptions( hGraphDefOptions AS LONG ) AS LONG LIB hDLL

DLL FUNCTION TF_NewTensor( nType AS LONG, @pDims AS LONG, nDims AS LONG, @pData AS LONG, nLength AS LONG, pDeallocator AS LONG,;
                           pDeallocatorArgs AS LONG ) AS LONG LIB hDLL
                           
DLL FUNCTION TF_AllocateTensor( nType AS LONG, pDims AS LONG, nDims AS LONG, nLegth AS LONG ) AS LONG LIB hDLL 
                           
DLL FUNCTION TF_StringEncodedSize( nLength AS LONG ) AS LONG LIB hDLL     

DLL FUNCTION TF_TensorData( hTensor AS LONG ) AS LONG LIB hDLL

DLL FUNCTION TF_StringEncode( cString AS LPSTR, nLength AS LONG, pDest AS LONG, nDestLength AS LONG, hStatus AS LONG ) AS LONG LIB hDLL

DLL FUNCTION TF_SessionRun( hSession AS LONG, hRunOptions AS LONG, hInputs AS LONG, @hInputValues AS LONG, nInputs AS LONG,;
                            hOutputs AS LONG, @hOutputValues AS LONG, nOutputs AS LONG, @hTargetOperations AS LONG, nTargets AS LONG,;
                            hRunMetadata AS LONG, hStatus AS LONG ) AS VOID LIB hDLL

DLL FUNCTION TF_Message( hStatus AS LONG ) AS LPSTR LIB hDLL

DLL FUNCTION TF_NewOperation( hGraph AS LONG, cOperationType AS LPSTR, cOperationName AS LPSTR ) AS LONG LIB hDLL

DLL FUNCTION TF_SetAttrTensor( hOperationDescription AS LONG, cAttributeName AS LPSTR, hTensor AS LONG, hStatus AS LONG ) AS VOID LIB hDLL

DLL FUNCTION TF_TensorType( hTensor AS LONG ) AS LONG LIB hDLL

DLL FUNCTION TF_SetAttrType( hOperationDescription AS LONG, cAttributeName AS LPSTR, nDataType AS LONG ) AS VOID LIB hDLL

DLL FUNCTION TF_FinishOperation( hOperationDescription AS LONG, hStatus AS LONG ) AS LONG LIB hDLL

DLL FUNCTION TF_AddInput( hOperationDescription AS LONG, nInput AS LONG ) AS VOID LIB hDLL

DLL FUNCTION TF_AddInputList( hOperationDescription AS LONG, hInputs AS LONG, nInputs AS LONG ) AS VOID LIB hDLL

DLL FUNCTION TF_GraphSetTensorShape( hGraph AS LONG, hOutput AS LONG, @pDims AS LONG, nDims AS LONG, hStatus AS LONG ) AS VOID LIB hDLL

DLL FUNCTION TF_SetAttrShape( hOperationDescription AS LONG, cAttributeName AS LPSTR, @pDims AS LONG, nDims AS LONG ) AS VOID LIB hDLL

#pragma BEGINDUMP

#include <hbapi.h>

typedef struct TF_Output {
  void * oper;
  int index;
} TF_Output;

HB_FUNC( TF_OUTPUT )
{
   TF_Output * hOutput = ( TF_Output * ) hb_xgrab( sizeof( TF_Output ) );
   
   hOutput->oper = ( void * ) hb_parnll( 1 );
   hOutput->index = hb_parnl( 2 );
   
   hb_retnll( ( HB_LONGLONG ) hOutput );
}   

HB_FUNC( TF_OUTPUT2 )
{
   TF_Output * hOutput = ( TF_Output * ) hb_xgrab( sizeof( TF_Output ) * 2 );
   
   hOutput[ 0 ].oper = ( void * ) hb_parnll( 1 );
   hOutput[ 0 ].index = 0;
   hOutput[ 1 ].oper = ( void * ) hb_parnll( 2 );
   hOutput[ 1 ].index = 0;
   
   hb_retnll( ( HB_LONGLONG ) hOutput );
}   

HB_FUNC( TF_TENSORSTRING )
{
   hb_retc( ( ( char * ) hb_parnll( 1 ) ) + 9 );
}   

HB_FUNC( TF_TENSORNUM )
{
   hb_retnl( * ( int * ) hb_parnll( 1 ) );
}   

HB_FUNC( MEMSET )
{
   hb_retnll( ( HB_LONGLONG ) memset( ( void * ) hb_parnll( 1 ), hb_parnl( 2 ), hb_parnll( 3 ) ) );
}   

HB_FUNC( MEMCPY )
{
   hb_retnll( ( HB_LONGLONG ) memcpy( ( void * ) hb_parnll( 1 ), ( void * ) hb_parc( 2 ), hb_parnll( 3 ) ) );
}   

HB_FUNC( HB_XFREE )
{
   hb_xfree( ( void * ) hb_parnll( 1 ) );
}   

#pragma ENDDUMP
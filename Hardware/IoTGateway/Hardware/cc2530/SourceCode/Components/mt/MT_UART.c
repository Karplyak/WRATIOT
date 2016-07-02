/***************************************************************************************************
  Filename:       MT_UART.c
  Revised:        $Date: 2009-03-12 16:25:22 -0700 (Thu, 12 Mar 2009) $
  Revision:       $Revision: 19404 $

  Description:  This module handles anything dealing with the serial port.

  Copyright 2007 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED �AS IS� WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.

***************************************************************************************************/

/***************************************************************************************************
 * INCLUDES
 ***************************************************************************************************/
#include "ZComDef.h"
#include "OSAL.h"
#include "hal_uart.h"
#include "MT.h"
#include "MT_UART.h"
#include "OSAL_Memory.h"

#include "string.h"
#include "DebugTrace.h"
#include "ZDProfile.h"
#include "stdlib.h"
#include "stdio.h"
#include "zcl_general.h"


/***************************************************************************************************
 * MACROS
 ***************************************************************************************************/

/***************************************************************************************************
 * CONSTANTS
 ***************************************************************************************************/
/* State values for ZTool protocal */
#define SOP_STATE      0x00
#define CMD_STATE1     0x01
#define CMD_STATE2     0x02
#define LEN_STATE      0x03
#define DATA_STATE     0x04
#define FCS_STATE      0x05

#define PROCESS_GW_UART_SUCESSS "CMDSUCCESS"
#define PROCESS_GW_UART_WRONGTYPE "CMDWRONGTYPE"

/***************************************************************************************************
 *                                         GLOBAL VARIABLES
 ***************************************************************************************************/
/* Used to indentify the application ID for osal task */
byte App_TaskID;

/* ZTool protocal parameters */
uint8 state;
uint8  CMD_Token[2];
uint8  LEN_Token;
uint8  FSC_Token;
mtOSALSerialData_t  *pMsg;
uint8  tempDataLen;

char * args[10];

#if defined (ZAPP_P1) || defined (ZAPP_P2)
uint16  MT_UartMaxZAppBufLen;
bool    MT_UartZAppRxStatus;
#endif


//void cicmdOnOff(uint16 destAddr,,char * name,char * strCommand,char * addrType);

/***************************************************************************************************
 *                                          LOCAL FUNCTIONS
 ***************************************************************************************************/

/***************************************************************************************************
 * @fn      MT_UartInit
 *
 * @brief   Initialize MT with UART support
 *
 * @param   None
 *
 * @return  None
***************************************************************************************************/
void MT_UartInit ()
{
  halUARTCfg_t uartConfig;

  /* Initialize APP ID */
  App_TaskID = 0;

  /* UART Configuration */
  uartConfig.configured           = TRUE;
  //uartConfig.baudRate             = MT_UART_DEFAULT_BAUDRATE;
  //uartConfig.baudRate             = HAL_UART_BR_9600;
  uartConfig.baudRate             = HAL_UART_BR_115200;
  uartConfig.flowControl          = FALSE;
  uartConfig.flowControlThreshold = 0;
  uartConfig.rx.maxBufSize        = MT_UART_DEFAULT_MAX_RX_BUFF;
  uartConfig.tx.maxBufSize        = MT_UART_DEFAULT_MAX_TX_BUFF;
  uartConfig.idleTimeout          = MT_UART_DEFAULT_IDLE_TIMEOUT;
  uartConfig.intEnable            = TRUE;
  /*
#if defined (ZTOOL_P1) || defined (ZTOOL_P2)
  uartConfig.callBackFunc         = MT_UartProcessZToolData;
#elif defined (ZAPP_P1) || defined (ZAPP_P2)
  uartConfig.callBackFunc         = MT_UartProcessZAppData;
#else
  uartConfig.callBackFunc         = NULL;
#endif
  */
  
  uartConfig.callBackFunc         = uartHandleCommand;
  
  /* Start UART */
#if defined (MT_UART_DEFAULT_PORT)
  HalUARTOpen (MT_UART_DEFAULT_PORT, &uartConfig);
#else
  /* Silence IAR compiler warning */
  (void)uartConfig;
#endif

  /* Initialize for ZApp */
#if defined (ZAPP_P1) || defined (ZAPP_P2)
  /* Default max bytes that ZAPP can take */
  MT_UartMaxZAppBufLen  = 1;
  MT_UartZAppRxStatus   = MT_UART_ZAPP_RX_READY;
#endif

}

/***************************************************************************************************
 * @fn      MT_SerialRegisterTaskID
 *
 * @brief   This function registers the taskID of the application so it knows
 *          where to send the messages whent they come in.
 *
 * @param   void
 *
 * @return  void
 ***************************************************************************************************/
void MT_UartRegisterTaskID( byte taskID )
{
  App_TaskID = taskID;
}

void uartHandleCommand( uint8 port, uint8 event ){
  uint8 *bufferInRx;
  uint8 argBuffer[20];
  int argcount = 0;
  uint8 countArgByteInBufferRx = 0;
  uint16 countByteInRxBuffer = Hal_UART_RxBufLen(port);
  bufferInRx = osal_mem_alloc(countByteInRxBuffer);
  HalUARTRead (port, bufferInRx, countByteInRxBuffer);
  
  afStatus_t zdpCmdStatus;
  char test[30];
  
  
  
  //sprintf(bufferSize, "%d", countByteInRxBuffer);
  //_itoa(countByteInRxBuffer,bufferSize,10);
//  uint8 i;
//  for (i=0;*bufferInRx+i!='\r';i++)
//  {
//    
//    //HalUARTWrite(MT_UART_DEFAULT_PORT, tempThis, strlen(tempThis));
//    HalUARTWrite(port, bufferInRx+i, 1);
//    //debug_str("rx was got somethings");
//  }
//  for(uint8 i = 0;i<countByteInRxBuffer;i++){
//    HalUARTWrite(port, bufferInRx+i, 1);
//  }
  
  /*
  
  switch(event) {
   case HAL_UART_RX_FULL:
     break;
   case HAL_UART_RX_ABOUT_FULL:
     break;
   case HAL_UART_RX_TIMEOUT:
  
  */
     
    for(uint8 i = 0; i<countByteInRxBuffer; i++){
      if(bufferInRx[i] != 0x0A) {
       if(bufferInRx[i] == ' '){
         argBuffer[countArgByteInBufferRx] = '\0';
         args[argcount] = osal_mem_alloc(countArgByteInBufferRx+1);
         strcpy(args[argcount],argBuffer);
         argcount++;
         countArgByteInBufferRx = 0;
       }else{
         argBuffer[countArgByteInBufferRx] = bufferInRx[i];
         countArgByteInBufferRx++;
       }
     }
      else {
        
       argBuffer[countArgByteInBufferRx] = '\0';
       args[argcount] = osal_mem_alloc(countArgByteInBufferRx+1);
       strcpy(args[argcount],argBuffer);
       argcount++;
       countArgByteInBufferRx = 0;
        
      
       /*
        for(int k = 0;k < argcount;k++){
           debug_str(args[k]);
         }
       */
        
        
        /*IEEEREQ [REQType,DestAddr,StartIndex]
        REQType 0 : Single
                1 : Extend (For Topology)
        StartIndex may not be required when REQType is Assigned to 0.     
         */
        if(!strcmp(args[0],"IEEEREQ")){
          if(!strcmp(args[1],"0")){
            zdpCmdStatus = ZDP_IEEEAddrReq( (uint16)atoi(args[2]), 0, 0, 0);
          }else if(!strcmp(args[1],"1")){
            ZDP_IEEEAddrReq( (uint16)atoi(args[2]), 1, (byte)atoi(args[3]), 0);
          }else{
            HalUARTWrite(MT_UART_DEFAULT_PORT, PROCESS_GW_UART_WRONGTYPE, strlen(PROCESS_GW_UART_WRONGTYPE));
          }
          //sprintf(test,"zdpCmdStatus : %d",zdpCmdStatus);
          //debug_str(test);
          //debug_str(PROCESS_GW_UART_SUCESSS);
        }
        
        else if(!strcmp(args[0],"IDENTIFY")){
          afAddrType_t addr;
            if(atoi(args[2]) == 0){
              addr.addrMode = (afAddrMode_t)Addr16Bit;
            }else if(atoi(args[2]) == 1){
              addr.addrMode = (afAddrMode_t)AddrGroup;
            }else{
              addr.addrMode = (afAddrMode_t)AddrBroadcast;
            }
          addr.endPoint = (uint8)atoi(args[1]);
          addr.addr.shortAddr = (uint16)atoi(args[3]);
          zclGeneral_SendIdentify( 8, &addr,(uint16)atoi(args[4]), FALSE, 0);
        }
        else if(!strcmp(args[0],"IDENTIFYQ")){
          afAddrType_t addr;
            if(atoi(args[2]) == 0){
              addr.addrMode = (afAddrMode_t)Addr16Bit;
            }else if(atoi(args[2]) == 1){
              addr.addrMode = (afAddrMode_t)AddrGroup;
            }else{
              addr.addrMode = (afAddrMode_t)AddrBroadcast;
            }
          addr.endPoint = (uint8)atoi(args[1]);
          addr.addr.shortAddr = (uint16)atoi(args[3]);
          zclGeneral_SendIdentifyQuery( 8, &addr,FALSE, 0);
        }else if(!strcmp(args[0],"ACTIVEEP")){
          zAddrType_t destAddr;
          //uint8 retValue;
          destAddr.addrMode = Addr16Bit;
          destAddr.addr.shortAddr = (uint16)atoi(args[1]);
          ZDP_ActiveEPReq( &destAddr, (uint16)atoi(args[1]), 0);
          //HalUARTWrite(MT_UART_DEFAULT_PORT, &retValue , 1);
        }else if(!strcmp(args[0],"ONOFF")){
          afAddrType_t addr;
          addr.endPoint = (uint8)atoi(args[1]);
          if(atoi(args[2]) == 0){
              addr.addrMode = (afAddrMode_t)Addr16Bit;
            }else if(atoi(args[2]) == 1){
              addr.addrMode = (afAddrMode_t)AddrGroup;
            }else{
              addr.addrMode = (afAddrMode_t)AddrBroadcast;
            }
          addr.addr.shortAddr = (uint16)atoi(args[3]);  // assign the destination address
          if(!strcmp(args[4],"1")){
            zclGeneral_SendOnOff_CmdOn(8,&addr,FALSE,0);
          }else{
            zclGeneral_SendOnOff_CmdOff(8,&addr,FALSE,0);
          }
        }
        
        
        
        //reset argcount
        argcount = 0;
        //free memory
        for(int i = 0; i < argcount; i++){
          osal_mem_free(args[i]);
        }
        
      }
    
  }
  
  //free memory
  osal_mem_free(bufferInRx);
    
   /* for(uint8 i = 0;*(bufferInRx+i)!='\n';i++){
      HalUARTWrite(port, bufferInRx+i, 1);
      
      
      
      
    }*/
     
//     if(*(bufferInRx+1)=='\r'){
//      HalUARTWrite(port, bufferInRx+0, 1);
//      }
//      else{
//        debug_str("in there");
//      }
   
  
  /*
    break;
  }
  
  */
  
  
}





/***************************************************************************************************
 * @fn      SPIMgr_CalcFCS
 *
 * @brief   Calculate the FCS of a message buffer by XOR'ing each byte.
 *          Remember to NOT include SOP and FCS fields, so start at the CMD field.
 *
 * @param   byte *msg_ptr - message pointer
 * @param   byte len - length (in bytes) of message
 *
 * @return  result byte
 ***************************************************************************************************/
byte MT_UartCalcFCS( uint8 *msg_ptr, uint8 len )
{
  byte x;
  byte xorResult;

  xorResult = 0;

  for ( x = 0; x < len; x++, msg_ptr++ )
    xorResult = xorResult ^ *msg_ptr;

  return ( xorResult );
}


/***************************************************************************************************
 * @fn      MT_UartProcessZToolData
 *
 * @brief   | SOP | Data Length  |   CMD   |   Data   |  FCS  |
 *          |  1  |     1        |    2    |  0-Len   |   1   |
 *
 *          Parses the data and determine either is SPI or just simply serial data
 *          then send the data to correct place (MT or APP)
 *
 * @param   port     - UART port
 *          event    - Event that causes the callback
 *
 *
 * @return  None
 ***************************************************************************************************/
void MT_UartProcessZToolData ( uint8 port, uint8 event )
{
  uint8  ch;
  uint8  bytesInRxBuffer;
  
  (void)event;  // Intentionally unreferenced parameter

  while (Hal_UART_RxBufLen(port))
  {
    HalUARTRead (port, &ch, 1);

    switch (state)
    {
      case SOP_STATE:
        if (ch == MT_UART_SOF)
          state = LEN_STATE;
        break;

      case LEN_STATE:
        LEN_Token = ch;

        tempDataLen = 0;

        /* Allocate memory for the data */
        pMsg = (mtOSALSerialData_t *)osal_msg_allocate( sizeof ( mtOSALSerialData_t ) +
                                                        MT_RPC_FRAME_HDR_SZ + LEN_Token );

        if (pMsg)
        {
          /* Fill up what we can */
          pMsg->hdr.event = CMD_SERIAL_MSG;
          pMsg->msg = (uint8*)(pMsg+1);
          pMsg->msg[MT_RPC_POS_LEN] = LEN_Token;
          state = CMD_STATE1;
        }
        else
        {
          state = SOP_STATE;
          return;
        }
        break;

      case CMD_STATE1:
        pMsg->msg[MT_RPC_POS_CMD0] = ch;
        state = CMD_STATE2;
        break;

      case CMD_STATE2:
        pMsg->msg[MT_RPC_POS_CMD1] = ch;
        /* If there is no data, skip to FCS state */
        if (LEN_Token)
        {
          state = DATA_STATE;
        }
        else
        {
          state = FCS_STATE;
        }
        break;

      case DATA_STATE:

        /* Fill in the buffer the first byte of the data */
        pMsg->msg[MT_RPC_FRAME_HDR_SZ + tempDataLen++] = ch;

        /* Check number of bytes left in the Rx buffer */
        bytesInRxBuffer = Hal_UART_RxBufLen(port);

        /* If the remain of the data is there, read them all, otherwise, just read enough */
        if (bytesInRxBuffer <= LEN_Token - tempDataLen)
        {
          HalUARTRead (port, &pMsg->msg[MT_RPC_FRAME_HDR_SZ + tempDataLen], bytesInRxBuffer);
          tempDataLen += bytesInRxBuffer;
        }
        else
        {
          HalUARTRead (port, &pMsg->msg[MT_RPC_FRAME_HDR_SZ + tempDataLen], LEN_Token - tempDataLen);
          tempDataLen += (LEN_Token - tempDataLen);
        }

        /* If number of bytes read is equal to data length, time to move on to FCS */
        if ( tempDataLen == LEN_Token )
            state = FCS_STATE;

        break;

      case FCS_STATE:

        FSC_Token = ch;

        /* Make sure it's correct */
        if ((MT_UartCalcFCS ((uint8*)&pMsg->msg[0], MT_RPC_FRAME_HDR_SZ + LEN_Token) == FSC_Token))
        {
          osal_msg_send( App_TaskID, (byte *)pMsg );
        }
        else
        {
          /* deallocate the msg */
          osal_msg_deallocate ( (uint8 *)pMsg );
        }

        /* Reset the state, send or discard the buffers at this point */
        state = SOP_STATE;

        break;

      default:
       break;
    }
  }
}

#if defined (ZAPP_P1) || defined (ZAPP_P2)
/***************************************************************************************************
 * @fn      MT_UartProcessZAppData
 *
 * @brief   | SOP | CMD  |   Data Length   | FSC  |
 *          |  1  |  2   |       1         |  1   |
 *
 *          Parses the data and determine either is SPI or just simply serial data
 *          then send the data to correct place (MT or APP)
 *
 * @param   port    - UART port
 *          event   - Event that causes the callback
 *
 *
 * @return  None
 ***************************************************************************************************/
void MT_UartProcessZAppData ( uint8 port, uint8 event )
{

  osal_event_hdr_t  *msg_ptr;
  uint16 length = 0;
  uint16 rxBufLen  = Hal_UART_RxBufLen(MT_UART_DEFAULT_PORT);

  /*
     If maxZAppBufferLength is 0 or larger than current length
     the entire length of the current buffer is returned.
  */
  if ((MT_UartMaxZAppBufLen != 0) && (MT_UartMaxZAppBufLen <= rxBufLen))
  {
    length = MT_UartMaxZAppBufLen;
  }
  else
  {
    length = rxBufLen;
  }

  /* Verify events */
  if (event == HAL_UART_TX_FULL)
  {
    // Do something when TX if full
    return;
  }

  if (event & ( HAL_UART_RX_FULL | HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_TIMEOUT))
  {
    if ( App_TaskID )
    {
      /*
         If Application is ready to receive and there is something
         in the Rx buffer then send it up
      */
      if ((MT_UartZAppRxStatus == MT_UART_ZAPP_RX_READY ) && (length != 0))
      {
        /* Disable App flow control until it processes the current data */
         MT_UartAppFlowControl (MT_UART_ZAPP_RX_NOT_READY);

        /* 2 more bytes are added, 1 for CMD type, other for length */
        msg_ptr = (osal_event_hdr_t *)osal_msg_allocate( length + sizeof(osal_event_hdr_t) );
        if ( msg_ptr )
        {
          msg_ptr->event = SPI_INCOMING_ZAPP_DATA;
          msg_ptr->status = length;

          /* Read the data of Rx buffer */
          HalUARTRead( MT_UART_DEFAULT_PORT, (uint8 *)(msg_ptr + 1), length );

          /* Send the raw data to application...or where ever */
          osal_msg_send( App_TaskID, (uint8 *)msg_ptr );
        }
      }
    }
  }
}

/***************************************************************************************************
 * @fn      SPIMgr_ZAppBufferLengthRegister
 *
 * @brief
 *
 * @param   maxLen - Max Length that the application wants at a time
 *
 * @return  None
 *
 ***************************************************************************************************/
void MT_UartZAppBufferLengthRegister ( uint16 maxLen )
{
  /* If the maxLen is larger than the RX buff, something is not right */
  if (maxLen <= MT_UART_DEFAULT_MAX_RX_BUFF)
    MT_UartMaxZAppBufLen = maxLen;
  else
    MT_UartMaxZAppBufLen = 1; /* default is 1 byte */
}

/***************************************************************************************************
 * @fn      SPIMgr_AppFlowControl
 *
 * @brief
 *
 * @param   status - ready to send or not
 *
 * @return  None
 *
 ***************************************************************************************************/
void MT_UartAppFlowControl ( bool status )
{

  /* Make sure only update if needed */
  if (status != MT_UartZAppRxStatus )
  {
    MT_UartZAppRxStatus = status;
  }

  /* App is ready to read again, ProcessZAppData have to be triggered too */
  if (status == MT_UART_ZAPP_RX_READY)
  {
    MT_UartProcessZAppData (MT_UART_DEFAULT_PORT, HAL_UART_RX_TIMEOUT );
  }

}

#endif //ZAPP

/***************************************************************************************************
***************************************************************************************************/

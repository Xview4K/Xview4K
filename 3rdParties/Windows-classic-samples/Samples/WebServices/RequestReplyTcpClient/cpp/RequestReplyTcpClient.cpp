// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#ifndef UNICODE
#define UNICODE
#endif
#include "WebServices.h"
#include "process.h"
#include "stdio.h"
#include "string.h"
#include "PurchaseOrder.wsdl.h"

// Print out rich error info
void PrintError(
    _In_ HRESULT errorCode, 
    _In_opt_ WS_ERROR* error)
{
    wprintf(L"Failure: errorCode=0x%lx\n", errorCode);

    if (errorCode == E_INVALIDARG || errorCode == WS_E_INVALID_OPERATION)
    {
        // Correct use of the APIs should never generate these errors
        wprintf(L"The error was due to an invalid use of an API.  This is likely due to a bug in the program.\n");
    }

    HRESULT hr = S_OK;
    if (error != NULL)
    {
        ULONG errorCount;
        hr = WsGetErrorProperty(error, WS_ERROR_PROPERTY_STRING_COUNT, &errorCount, sizeof(errorCount));
        if (FAILED(hr))
        {
            goto Exit;
        }
        for (ULONG i = 0; i < errorCount; i++)
        {
            WS_STRING string;
            hr = WsGetErrorString(error, i, &string);
            if (FAILED(hr))
            {
                goto Exit;
            }
            wprintf(L"%.*s\n", string.length, string.chars);
        }
    }
Exit:
    if (FAILED(hr))
    {
        wprintf(L"Could not get error string (errorCode=0x%lx)\n", hr);
    }
}

// Main entry point
int __cdecl wmain()
{
    
    HRESULT hr = S_OK;
    WS_ERROR* error = NULL;
    WS_CHANNEL* channel = NULL;
    WS_MESSAGE* requestMessage = NULL;
    WS_MESSAGE* replyMessage = NULL;
    WS_HEAP* heap = NULL;
    
    // Create an error object for storing rich error information
    hr = WsCreateError(
        NULL, 
        0, 
        &error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    
    // Create a heap to store deserialized data
    hr = WsCreateHeap(
        /*maxSize*/ 2048, 
        /*trimSize*/ 512, 
        NULL, 
        0, 
        &heap, 
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    
    
    
    // Create a TCP duplex session channel
    hr = WsCreateChannel(
        WS_CHANNEL_TYPE_DUPLEX_SESSION, 
        WS_TCP_CHANNEL_BINDING, 
        NULL, 
        0, 
        NULL, 
        &channel, 
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    
    // Initialize address of service
    WS_ENDPOINT_ADDRESS address;
    address.url.chars = L"net.tcp://localhost/example";
    address.url.length = (ULONG)::wcslen(address.url.chars);
    address.headers = NULL;
    address.extensions = NULL;
    address.identity = NULL;
    
    // Open channel to address
    hr = WsOpenChannel(
        channel, 
        &address, 
        NULL, 
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    
    hr = WsCreateMessageForChannel(
        channel,
        NULL, 
        0, 
        &requestMessage, 
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    
    hr = WsCreateMessageForChannel(
        channel,
        NULL, 
        0, 
        &replyMessage, 
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    
    // Send some request-replies
    for (int i = 0; i < 100; i++)
    {
        // Initialize purchase order
        _PurchaseOrderType purchaseOrder;
        purchaseOrder.quantity = 100;
        purchaseOrder.productName = L"Pencil";
        
        _OrderConfirmationType orderConfirmation;
        
        // Send purchase order, get order confirmation
        hr = WsRequestReply(
            channel,
            requestMessage, 
            &PurchaseOrder_wsdl.messages.PurchaseOrder, 
            WS_WRITE_REQUIRED_VALUE,
            &purchaseOrder,
            sizeof(purchaseOrder),
            replyMessage, 
            &PurchaseOrder_wsdl.messages.OrderConfirmation, 
            WS_READ_REQUIRED_VALUE, 
            heap, 
            &orderConfirmation, 
            sizeof(orderConfirmation), 
            NULL, 
            error);
        
        if (FAILED(hr))
        {
            goto Exit;
        }
        
        // Print out confirmation contents
        wprintf(L"Expected ship date for order %lu is %s\n",
            orderConfirmation.orderID,
            orderConfirmation.expectedShipDate);
        
        // Reset the message so it can be used again
        hr = WsResetMessage(requestMessage, error);
        if (FAILED(hr))
        {
            goto Exit;
        }
        
        // Reset the message so it can be used again
        hr = WsResetMessage(replyMessage, error);
        if (FAILED(hr))
        {
            goto Exit;
        }
        
        // Initialize request for order status
        _GetOrderStatusType getOrderStatus;
        getOrderStatus.orderID = orderConfirmation.orderID;
        
        _GetOrderStatusResponseType getOrderStatusResponse;
        
        // Send order status request, get order status reply
        hr = WsRequestReply(
            channel,
            requestMessage, 
            &PurchaseOrder_wsdl.messages.GetOrderStatus, 
            WS_WRITE_REQUIRED_VALUE,
            &getOrderStatus,
            sizeof(getOrderStatus),
            replyMessage, 
            &PurchaseOrder_wsdl.messages.GetOrderStatusResponse, 
            WS_READ_REQUIRED_VALUE, 
            heap, 
            &getOrderStatusResponse, 
            sizeof(getOrderStatusResponse), 
            NULL, 
            error);
        
        if (FAILED(hr))
        {
            goto Exit;
        }
        
        // Print out order status
        wprintf(L"Order status for order %lu is: %s\n",
            getOrderStatusResponse.orderID,
            getOrderStatusResponse.status);
        
        // Reset the message so it can be used again
        hr = WsResetMessage(requestMessage, error);
        if (FAILED(hr))
        {
            goto Exit;
        }
        
        // Reset the message so it can be used again
        hr = WsResetMessage(replyMessage, error);
        if (FAILED(hr))
        {
            goto Exit;
        }
        
        // Make same request, but this time with an invalid order ID
        getOrderStatus.orderID = 321;
        hr = WsRequestReply(
            channel,
            requestMessage, 
            &PurchaseOrder_wsdl.messages.GetOrderStatus, 
            WS_WRITE_REQUIRED_VALUE,
            &getOrderStatus,
            sizeof(getOrderStatus),
            replyMessage, 
            &PurchaseOrder_wsdl.messages.GetOrderStatusResponse, 
            WS_READ_REQUIRED_VALUE, 
            heap, 
            &getOrderStatusResponse, 
            sizeof(getOrderStatusResponse), 
            NULL, 
            error);
        
        // Check to see if we got a fault
        if (hr == WS_E_ENDPOINT_FAULT_RECEIVED)
        {
            // Print the strings in the error object
            PrintError(hr, error);
        
            static const WS_XML_STRING _faultDetailName = WS_XML_STRING_VALUE("OrderNotFound");
            static const WS_XML_STRING _faultDetailNs = WS_XML_STRING_VALUE("http://example.com");
            static const WS_XML_STRING _faultAction = WS_XML_STRING_VALUE("http://example.com/fault");
            static const WS_ELEMENT_DESCRIPTION _faultElementDescription = 
            { 
                (WS_XML_STRING*)&_faultDetailName, 
                (WS_XML_STRING*)&_faultDetailNs, 
                WS_UINT32_TYPE, 
                NULL 
            };
            static const WS_FAULT_DETAIL_DESCRIPTION orderNotFoundFaultTypeDescription = 
            { 
                (WS_XML_STRING*)&_faultAction, 
                (WS_ELEMENT_DESCRIPTION*)&_faultElementDescription 
            };
        
            // Try to get the fault detail from the error object
            _OrderNotFoundFaultType* orderNotFound;
            hr = WsGetFaultErrorDetail(
                error,
                &orderNotFoundFaultTypeDescription,
                WS_READ_OPTIONAL_POINTER,
                heap,
                &orderNotFound,
                sizeof(orderNotFound));
                
            if (FAILED(hr))
            {
                goto Exit;
            }
        
            if (orderNotFound != NULL)
            {
                // Print out the fault detail
                wprintf(L"Order %lu was not found\n", orderNotFound->orderID);
            }
        
            // Reset error so it can be used again
            hr = WsResetError(error);
            if (FAILED(hr))
            {
                goto Exit;
            }
        }
        
        if (FAILED(hr))
        {
            goto Exit;
        }
        
        // Reset the message so it can be used again
        hr = WsResetMessage(requestMessage, error);
        if (FAILED(hr))
        {
            goto Exit;
        }
        
        // Reset the message so it can be used again
        hr = WsResetMessage(replyMessage, error);
        if (FAILED(hr))
        {
            goto Exit;
        }
        
        wprintf(L"\n");
        
        // Reset the heap
        hr = WsResetHeap(heap, error);
        if (FAILED(hr))
        {
            goto Exit;
        }
    }
    
Exit:
    if (FAILED(hr))
    {
        // Print out the error
        PrintError(hr, error);
    }
    
    if (channel != NULL)
    {
        // Close the channel
        WsCloseChannel(channel, NULL, error);
    }
    if (requestMessage != NULL)
    {
        WsFreeMessage(requestMessage);
    }
    if (replyMessage != NULL)
    {
        WsFreeMessage(replyMessage);
    }
    if (channel != NULL)
    {
        WsFreeChannel(channel);
    }
    
    
    if (error != NULL)
    {
        WsFreeError(error);
    }
    if (heap != NULL)
    {
        WsFreeHeap(heap);
    }
    fflush(stdout);
    return SUCCEEDED(hr) ? 0 : -1;
}

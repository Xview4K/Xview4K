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
    WS_HEAP* heap = NULL;
    WS_SERVICE_PROXY* serviceProxy = NULL;
    static const WS_STRING serviceUrl = WS_STRING_VALUE(L"http://localhost/example");
    
    
    // In this sample, wsutil is used with the /string:WS_STRING command line option 
    // to compile the schema files. When /string:WS_STRING is used, wsutil generates stubs
    // using WS_STRING (instead of WCHAR*) type for strings.
    WS_STRING productName;
    WS_STRING expectedShipDate;
    WS_STRING orderStatus;
    WS_ENDPOINT_ADDRESS address = {};
    address.url = serviceUrl;
    
    
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
    hr = WsCreateServiceProxy(
        WS_CHANNEL_TYPE_REQUEST, 
        WS_HTTP_CHANNEL_BINDING, 
        NULL, 
        NULL, 
        0, 
        NULL, 
        0, 
        &serviceProxy, 
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    
    
    hr = WsOpenServiceProxy(
        serviceProxy, 
        &address, 
        NULL, 
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    productName.chars = L"Pencil";
    productName.length = 6;
    
    for (int i = 0; i < 100; i++)
    {
        unsigned int orderID;
    
        // Submit an order, and get expected ship date
        hr = PurchaseOrderBinding_Order(
            serviceProxy, 
            100, 
            productName, 
            &orderID, 
            &expectedShipDate, 
            heap, 
            NULL, 
            0, 
            NULL, 
            error);
    
        if (FAILED(hr))
        {
            goto Exit;
        }
    
        // Print out confirmation contents
        wprintf(L"Expected ship date for order %lu is %.*s\n",
            orderID,
            expectedShipDate.length,
            expectedShipDate.chars);
        
        WsResetHeap(heap, NULL);
        
        // Get the current status of the order
        hr = PurchaseOrderBinding_OrderStatus(
            serviceProxy, 
            &orderID, 
            &orderStatus, 
            heap, 
            NULL, 
            0, 
            NULL, 
            error);
    
        if (FAILED(hr))
        {
            goto Exit;
        }
    
        // Print out order status
        wprintf(L"Order status for order %lu is: %.*s\n",
            orderID,
            orderStatus.length,
            orderStatus.chars);
        
        WsResetHeap(
            heap, 
            NULL);
    
        // Get the current status of the order using an invalid order ID
        orderID = 321;
        hr = PurchaseOrderBinding_OrderStatus(
            serviceProxy, 
            &orderID, 
            &orderStatus, 
            heap, 
            NULL, 
            0, 
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
    
        WsResetHeap(heap, NULL);
    
        wprintf(L"\n");
    }
                   
Exit:
    if (FAILED(hr))
    {
        // Print out the error
        PrintError(hr, error);
    }
    if (serviceProxy != NULL)
    {
        WsCloseServiceProxy(serviceProxy, NULL, NULL);
        WsFreeServiceProxy(serviceProxy);
    }
    
    
    if (heap != NULL)
    {
        WsFreeHeap(heap);
    }
    if (error != NULL)
    {
        WsFreeError(error);
    }
    fflush(stdout);
    return SUCCEEDED(hr) ? 0 : -1;
}

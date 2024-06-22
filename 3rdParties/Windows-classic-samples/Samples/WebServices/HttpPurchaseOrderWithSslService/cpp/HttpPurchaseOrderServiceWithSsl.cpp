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
#include "wincrypt.h"
#include <strsafe.h>
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

HANDLE closeServer = NULL;  


static const WCHAR ExpectedShipDate [] = L"1/1/2006";
static const WCHAR OrderStatusString [] = L"Pending";

volatile long numberOfOrders = 0;


HRESULT CALLBACK PurchaseOrderImpl(
    _In_ const WS_OPERATION_CONTEXT* context,
    _In_ int quantity,
    _In_z_ WCHAR* productName,
    _Out_ unsigned int* orderID,
    _Outptr_result_z_ WCHAR** expectedShipDate,
    _In_ const WS_ASYNC_CONTEXT* asyncContext,
    _In_ WS_ERROR* error)
{
    UNREFERENCED_PARAMETER(asyncContext);

    WS_HEAP* heap = NULL;
    HRESULT hr = S_OK;

    wprintf(L"%ld, %s\n", quantity, productName);
    fflush(stdout);

    hr = WsGetOperationContextProperty(
        context,
        WS_OPERATION_CONTEXT_PROPERTY_HEAP,
        &heap,
        sizeof(heap),
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = WsAlloc(
        heap,
        sizeof(ExpectedShipDate),
        (void**)expectedShipDate,
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = StringCbCopyW(
        *expectedShipDate,
        sizeof(ExpectedShipDate),
        ExpectedShipDate);
    if (FAILED(hr))
    {
        goto Exit;
    }

    *orderID = 123;

Exit:
    return hr;
}


HRESULT CALLBACK GetOrderStatusImpl(
    _In_ const WS_OPERATION_CONTEXT* context,
    _Inout_ unsigned int* orderID,
    _Outptr_result_z_ WCHAR** status,
    _In_ const WS_ASYNC_CONTEXT* asyncContext,
    _In_ WS_ERROR* error)
{
    UNREFERENCED_PARAMETER(asyncContext);

    WS_HEAP* heap = NULL;
    HRESULT hr = S_OK;

    // Generate a fault if we don't recognize the order ID
    if (*orderID != 123)
    {
        // Fill out details about the fault
        _OrderNotFoundFaultType orderNotFound;
        orderNotFound.orderID = *orderID;
        
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
        
        // Set fault detail information in the error object
        hr = WsSetFaultErrorDetail(
            error,
            &orderNotFoundFaultTypeDescription,
            WS_WRITE_REQUIRED_VALUE,
            &orderNotFound,
            sizeof(orderNotFound));
        
        if (FAILED(hr))
        {
            goto Exit;
        }
        
        // Add an error string to the error object.  This string will
        // be included in the fault that is sent.
        static const WS_STRING errorMessage = WS_STRING_VALUE(L"Invalid order ID");
        hr = WsAddErrorString(error, &errorMessage);
        
        if (FAILED(hr))
        {
            goto Exit;
        }

        hr = E_FAIL;
        goto Exit;
    }

    *orderID = *orderID;

    hr = WsGetOperationContextProperty(
        context,
        WS_OPERATION_CONTEXT_PROPERTY_HEAP,
        &heap,
        sizeof(heap),
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = WsAlloc(
        heap,
        sizeof(OrderStatusString),
        (void**)status,
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = StringCbCopyW(
        *status,
        sizeof(OrderStatusString),
        OrderStatusString);
    if (FAILED(hr))
    {
        goto Exit;
    }

Exit:
    return hr;
}

HRESULT CALLBACK CloseChannelCallback(
    _In_ const WS_OPERATION_CONTEXT* context,
    _In_ const WS_ASYNC_CONTEXT* asyncContext)
{
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(asyncContext);

    ULONG orderCount = InterlockedIncrement(&numberOfOrders);
    if (orderCount == 300)
    {
        SetEvent(closeServer);
    }
    return S_OK;
}

static const PurchaseOrderBindingFunctionTable purchaseOrderFunctions = {PurchaseOrderImpl, GetOrderStatusImpl};

// Method contract for the service
static const WS_SERVICE_CONTRACT purchaseOrderContract = 
{
    &PurchaseOrder_wsdl.contracts.PurchaseOrderBinding, // comes from the generated header.
    NULL, // for not specifying the default contract
    &purchaseOrderFunctions // specified by the user
};


// Main entry point
int __cdecl wmain()
{
    
    HRESULT hr = S_OK;
    WS_SERVICE_HOST* host = NULL;
    WS_SERVICE_ENDPOINT serviceEndpoint = {};
    const WS_SERVICE_ENDPOINT* serviceEndpoints[1];
    serviceEndpoints[0] = &serviceEndpoint;
    WS_ERROR* error = NULL;
    
    // declare and initialize an SSL transport security binding
    WS_SSL_TRANSPORT_SECURITY_BINDING sslBinding = {}; // zero out the struct
    sslBinding.binding.bindingType = WS_SSL_TRANSPORT_SECURITY_BINDING_TYPE; // set the binding type
    // NOTE: At the server, the SSL certificate for the listen URI must be
    // registered with http.sys using a tool such as httpcfg.exe.
    
    // declare and initialize the array of all security bindings
    WS_SECURITY_BINDING* securityBindings[1] = { &sslBinding.binding };
    
    // declare and initialize the security description
    WS_SECURITY_DESCRIPTION securityDescription = {}; // zero out the struct
    securityDescription.securityBindings = securityBindings;
    securityDescription.securityBindingCount = WsCountOf(securityBindings);
    WS_SERVICE_ENDPOINT_PROPERTY serviceProperties[1];
    WS_SERVICE_PROPERTY_CLOSE_CALLBACK closeCallbackProperty = {CloseChannelCallback};
    serviceProperties[0].id = WS_SERVICE_ENDPOINT_PROPERTY_CLOSE_CHANNEL_CALLBACK;
    serviceProperties[0].value = &closeCallbackProperty;
    serviceProperties[0].valueSize = sizeof(closeCallbackProperty);
    
    
    // Initialize service endpoint
    serviceEndpoint.address.url.chars = L"https://localhost:8443/example"; // address given as uri
    serviceEndpoint.address.url.length = (ULONG)wcslen(serviceEndpoint.address.url.chars);
    serviceEndpoint.channelBinding = WS_HTTP_CHANNEL_BINDING; // channel binding for the endpoint
    serviceEndpoint.channelType = WS_CHANNEL_TYPE_REPLY; // the channel type
    serviceEndpoint.securityDescription = &securityDescription; // security description
    serviceEndpoint.contract = &purchaseOrderContract;  // the contract
    serviceEndpoint.properties = serviceProperties;
    serviceEndpoint.propertyCount = WsCountOf(serviceProperties);
    
    // Create an error object for storing rich error information
    hr = WsCreateError(
        NULL, 
        0, 
        &error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    // Create Event object for closing the server
    closeServer = CreateEvent(
        NULL, 
        TRUE, 
        FALSE, 
        NULL);
    if (closeServer == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }   
    // Creating a service host
    hr = WsCreateServiceHost(
        serviceEndpoints, 
        1, 
        NULL, 
        0, 
        &host, 
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    // WsOpenServiceHost to start the listeners in the service host 
    hr = WsOpenServiceHost(
        host, 
        NULL, 
        error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    WaitForSingleObject(closeServer, INFINITE);
    
    // Close the service host
    hr = WsCloseServiceHost(host, NULL, error);
    if (FAILED(hr))
    {
        goto Exit;
    }
    
Exit:
    if (FAILED(hr))
    {
        // Print out the error
        PrintError(hr, error);
    }
    if (host != NULL)
    {
        WsFreeServiceHost(host);
    }
    
    
    if (error != NULL)
    {
        WsFreeError(error);
    }
    if (closeServer != NULL)
    {
        CloseHandle(closeServer);
    }
    fflush(stdout);
    return SUCCEEDED(hr) ? 0 : -1;
}


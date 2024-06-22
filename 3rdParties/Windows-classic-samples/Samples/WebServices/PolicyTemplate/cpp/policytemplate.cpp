
#include <webservices.h>
struct CalculatorBinding1_policies
{
    WS_ENCODING encoding;
    WS_CHANNEL_PROPERTY channelPropertiesArray[1];
    WS_PROTECTION_LEVEL protectionLevel;
    WS_SECURITY_PROPERTY securityPropertiesArray[1];
    struct // headerAuthBinding
    {
        ULONG headerAuthentication;
        WS_SECURITY_BINDING_PROPERTY securityBindingProperties[1];
    } headerAuthBinding;
    WS_HTTP_HEADER_AUTH_POLICY_DESCRIPTION policyTemplate;
};

// uses WS_HTTP_SSL_USERNAME_POLICY_TEMPLATE  
CalculatorBinding1_policies _calculatorBinding1 =
{
    WS_ENCODING_XML_UTF8,
    {
        {
            WS_CHANNEL_PROPERTY_ENCODING,
                &_calculatorBinding1.encoding,
                sizeof(_calculatorBinding1.encoding),
        },
    },
    WS_PROTECTION_LEVEL_NONE,
    { // securityPropertiesArray
        {
            WS_SECURITY_PROPERTY_TRANSPORT_PROTECTION_LEVEL,
                (void*)&_calculatorBinding1.protectionLevel,
                sizeof(&_calculatorBinding1.protectionLevel),
        },
    },
    {  // headerAuthBinding
        WS_HTTP_HEADER_AUTH_SCHEME_NTLM,
        {  // security binding properties
            {
                WS_SECURITY_BINDING_PROPERTY_HTTP_HEADER_AUTH_SCHEME,
                    (void*)&_calculatorBinding1.headerAuthBinding.headerAuthentication,
                    sizeof(&_calculatorBinding1.headerAuthBinding.headerAuthentication),
            },
        },  // security binding properties
    },  // headerAuthBinding
    {
        {
            _calculatorBinding1.channelPropertiesArray,
            1,
        },
        {
            _calculatorBinding1.securityPropertiesArray,
            1,
        },
        {
            {
                _calculatorBinding1.headerAuthBinding.securityBindingProperties,
                1,
            },
        },
    },
};



HRESULT CalculatorBinding1_CreateServiceProxy(
    _In_reads_opt_(proxyPropertyCount) const WS_PROXY_PROPERTY* proxyProperties,
    _In_ const ULONG proxyPropertyCount,
    _In_opt_ WS_HTTP_HEADER_AUTH_BINDING_TEMPLATE* templateValue,
    _Outptr_ WS_SERVICE_PROXY** serviceProxy,
    _In_opt_ WS_ERROR* error)
{
    return WsCreateServiceProxyFromTemplate(
        WS_CHANNEL_TYPE_REQUEST,  // this is fixed for http, requires input for tcp
        proxyProperties,   
        proxyPropertyCount, 
        WS_HTTP_HEADER_AUTH_BINDING_TEMPLATE_TYPE,
        templateValue,
        templateValue == NULL ? 0 : sizeof(WS_HTTP_HEADER_AUTH_BINDING_TEMPLATE),
        &_calculatorBinding1.policyTemplate,   // template description as generated in the stub file
        sizeof(WS_HTTP_HEADER_AUTH_POLICY_DESCRIPTION),
        serviceProxy,   
        error);   
}

HRESULT CalculatorBinding1_CreateServiceEndpoint(
    _In_ const WS_SERVICE_ENDPOINT_PROPERTY* endpointProperties,
    _In_ ULONG endpointPropertiesCount,
    _In_opt_ const WS_STRING* address,
    _In_ WS_SERVICE_CONTRACT* contract,
    _In_ WS_SERVICE_SECURITY_CALLBACK authorizationCallback,
    _In_ WS_HEAP* heap,
    _In_opt_ WS_HTTP_HEADER_AUTH_BINDING_TEMPLATE* templateValue,
    _Outptr_ WS_SERVICE_ENDPOINT** serviceEndpoint,
    _In_opt_ WS_ERROR* error)
{
    return WsCreateServiceEndpointFromTemplate(
        WS_CHANNEL_TYPE_REPLY,  // this is fixed for http, requires input for tcp
        endpointProperties,
        endpointPropertiesCount,
        address,  // service endpoint address
        contract,
        authorizationCallback,
        heap,
        WS_HTTP_HEADER_AUTH_BINDING_TEMPLATE_TYPE,
        templateValue,
        templateValue == NULL ? 0 : sizeof(WS_HTTP_HEADER_AUTH_BINDING_TEMPLATE),
        &_calculatorBinding1.policyTemplate,   // template description as generated in the stub file
        sizeof(WS_HTTP_HEADER_AUTH_POLICY_DESCRIPTION),
        serviceEndpoint,
        error);
}

// All the structure/methods etc. above are generated by the tool 
int __cdecl wmain()
{   
    ULONG receiveTimeout = 5;
    HRESULT hr;
    WS_SERVICE_PROXY* proxy = NULL;

    // declare and initialize a Windows credential
    WS_STRING_WINDOWS_INTEGRATED_AUTH_CREDENTIAL windowsCredential = {}; // zero out the struct
    windowsCredential.credential.credentialType = WS_STRING_WINDOWS_INTEGRATED_AUTH_CREDENTIAL_TYPE; // set the credential type
    // for illustration only; usernames and passwords should never be included in source files
    windowsCredential.username.chars = L".\\userName";
    windowsCredential.username.length = (ULONG)wcslen(windowsCredential.username.chars);
    windowsCredential.password.chars = L"Password";
    windowsCredential.password.length = (ULONG)wcslen(windowsCredential.password.chars);

    WS_CHANNEL_PROPERTY userChannelProperties[1];
    userChannelProperties[0].id = WS_CHANNEL_PROPERTY_RECEIVE_TIMEOUT;
    userChannelProperties[0].value = &receiveTimeout;
    userChannelProperties[0].valueSize = sizeof(receiveTimeout);

    WS_HTTP_HEADER_AUTH_BINDING_TEMPLATE templateStruct = {};
    templateStruct.httpHeaderAuthSecurityBinding.clientCredential = &windowsCredential.credential;
    templateStruct.channelProperties.properties = userChannelProperties;
    templateStruct.channelProperties.propertyCount = WsCountOf(userChannelProperties);

    WS_ERROR* error = NULL;
    hr = WsCreateError(NULL, 0, &error);
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = CalculatorBinding1_CreateServiceProxy(
        NULL,
        0,
        &templateStruct, 
        &proxy,
        error);
    // do real work here.
    if (FAILED(hr))
    {
        goto Exit;
    }

Exit:
    if (proxy != NULL)
    {
        WsFreeServiceProxy(proxy);
    }

    WsFreeError(error);

    return 0;
}

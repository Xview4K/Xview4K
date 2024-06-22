// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//
// DimmerDeviceDCO.cpp : Implementation code for Dimmer Device

#include <atlbase.h>
#include "resource.h"
#include "DeviceDll.h"
#include <atlcom.h>
#include <upnphost.h>
#include <initguid.h>
#include "dimmerdevice.h"
#include "dimmerdevice_i.c"
#include "DimmerService.h"
#include "DimmerDeviceDCO.h"


/////////////////////////////////////////////////////////////////////////////
//  UPNPDimmerDevice : Implementation
////////////////////////////////////////////////////////////////////////////
UPNPDimmerDevice:: UPNPDimmerDevice()
{
   // initialise variables
   INT i,j;

   // initialise the table mapping the template UDN's to the UDN's used by the device host.
   // there is only one entry in this table since the device has only a RootDevice and no 
   // embedded devices
   UDNmapping[0][0] = SysAllocString(L"uuid:RootDevice");
   UDNmapping[0][1] = NULL;


   // initialise the table mapping between the devices and the services ID's of the services it hosts
   // i.e. Row 0 contains services IDs for the RootDevice and Row1 for embedded device 1 if present etc.
   // The sample device just has a Rootdevice and no embedded devices
   serviceIDDeviceMap[0][0] = SysAllocString( L"urn:microsoft-com:serviceId:DimmerService1.0" );

   // array pDisPtrs holds the IDispatch Interface pointers for the services supported by the device
   for (i = 0; i < NUM_TEMPLATE_UDNS_IN_DESCDOC; i++)
   {
      for (j = 0; j < MAX_NOSERVICE_PER_DEVICE; j++)
      {
         pDisPtrs[i][j] = NULL;
      }
   }
}



UPNPDimmerDevice::~ UPNPDimmerDevice()
{
   INT i,j;
   for (i = 0; i < NUM_TEMPLATE_UDNS_IN_DESCDOC; i++)
   {
      for (j = 0; j < MAX_NOSERVICE_PER_DEVICE; j++)
      {
         if (NULL != pDisPtrs[i][j])
         {
            pDisPtrs[i][j]->Release();
            pDisPtrs[i][j]=NULL;
         }
      }
   }

   //free the strings allocated for the table mapping the template UDN to real UDN
   if(UDNmapping[0][0])
   {
      SysFreeString(UDNmapping[0][0]);
   }

   //free the strings allocated for the table mapping devices to Service ID's
   if(serviceIDDeviceMap[0][0])
   {
      SysFreeString(serviceIDDeviceMap[0][0]);
   } 
} 



STDMETHODIMP  UPNPDimmerDevice::Initialize(_In_ BSTR bstrXMLDesc, _In_ BSTR deviceID, _In_ BSTR bstrInitString)
{
   UNREFERENCED_PARAMETER(bstrInitString);

   //We create an instance of our service and store it for future use
   HRESULT hr = S_OK;
   IUPnPRegistrar *pReg = NULL;
   WCHAR cwszDebugString[DEBUG_STRING_LENGTH];
   INT iLen;

   OutputDebugString(TEXT("UPNPDimmerDeviceDCO: Entering Initialize\n"));
   CComObject<UPNPDimmerService> *pUPNPDimmerService;

   //if the description doc passed to the Initialize function is NULL return an error
   if (NULL == bstrXMLDesc)
   {
      OutputDebugString(TEXT("UPNPDimmerDeviceDCO: bstrXMLDesc is null\n"));
      return E_FAIL;
   }


   // Now create Service Objects for the services hosted by the device and store 
   // it in the pDispPtrs table. We get the Service Object IDispatch pointer for 
   // the RootDevice's dimming service and store it in Location 0,0
   pUPNPDimmerService = NULL;
   OutputDebugString(TEXT("UPNPDimmerDeviceDCO: Creating the service object\n"));
   hr = CComObject<UPNPDimmerService>::CreateInstance(&pUPNPDimmerService);

   if (FAILED(hr))
   {
      iLen = swprintf_s( cwszDebugString, 100, L"UPNPDimmerDeviceDCO: Cannot create the service object. HRESULT: 0x%x\n", hr );
      OutputDebugString(cwszDebugString);
      return E_FAIL;
   }

   // Now get the pointer to the IDispatch interface using the QueryInterface function

   OutputDebugString(TEXT("UPNPDimmerDeviceDCO: Querying the IDispatch for the service object\n"));
   hr = pUPNPDimmerService->QueryInterface(IID_IDispatch,(LPVOID *) &pDisPtrs[0][0]);

   if (FAILED(hr))
   {
      iLen = swprintf_s( cwszDebugString, 100, L"UPNPDimmerDeviceDCO: Can't get IDispatch interface from the service object. HRESULT: 0x%x\n", hr );
      OutputDebugString(cwszDebugString);
      pUPNPDimmerService->Release();
      return hr;
   }


   // Now create a table mapping the template UDN's to the actual published UDN's we 
   // use BSTR's since the function GetUniqueDeviceName() takes in BSTR's as parameters
   OutputDebugString(TEXT("UPNPDimmerDeviceDCO: Creating the registrar object\n"));
   hr = CoCreateInstance(
      CLSID_UPnPRegistrar,
      NULL,
      CLSCTX_SERVER,
      IID_IUPnPRegistrar,
      (LPVOID *) &pReg
      );

   if (FAILED(hr))
   {
      iLen = swprintf_s( cwszDebugString, 100, L"UPNPDimmerDeviceDCO: Can't create the registrar object. HRESULT: 0x%x\n", hr );
      OutputDebugString(cwszDebugString);
      pUPNPDimmerService->Release(); 
      return hr;
   }

   for (INT i = 0; i < NUM_TEMPLATE_UDNS_IN_DESCDOC; i++)
   {
      hr = pReg->GetUniqueDeviceName(deviceID, UDNmapping[i][0], &UDNmapping[i][1]);
      if (FAILED(hr))
      {
         pReg->Release();
         pUPNPDimmerService->Release(); 
         iLen = swprintf_s( cwszDebugString, 100, L"UPNPDimmerDeviceDCO: Can't get the unique device names. HRESULT: 0x%x\n", hr );
         OutputDebugString(cwszDebugString);
         return hr;
      }
   }

   pReg->Release();
   OutputDebugString(TEXT("UPNPDimmerDeviceDCO: Exiting the Initialize\n"));
   return hr;

}

STDMETHODIMP  UPNPDimmerDevice::GetServiceObject(_In_ BSTR bstrUDN, _In_ BSTR bstrServiceId, _Outptr_ IDispatch **ppdispService)
{ 
   // Look at the Service for which the pointer is asked and return it in the IDispatch pointer
   INT i;
   INT foundDevice = -1;
   INT foundService = -1;

   for (i = 0; i < NUM_TEMPLATE_UDNS_IN_DESCDOC; i++)
   {
       if (0 == _tcscmp(bstrUDN,UDNmapping[i][1]))
      {
         foundDevice = i;
      }
   }

   if (-1 == foundDevice)
   {
      return E_FAIL;
   }

   // else find the service in this particular device
   for (i = 0; i < MAX_NOSERVICE_PER_DEVICE; i++)
   {
       if (0 == _tcscmp(serviceIDDeviceMap[foundDevice][i],bstrServiceId))
      {
         foundService=i;
      }
   }

   if (-1 == foundService)
   {
      return E_FAIL;
   }

   if (NULL == pDisPtrs[foundDevice][foundService])
   {
      return E_FAIL;
   }

   pDisPtrs[foundDevice][foundService]->AddRef();
   *ppdispService = pDisPtrs[foundDevice][foundService];
   return S_OK;
}

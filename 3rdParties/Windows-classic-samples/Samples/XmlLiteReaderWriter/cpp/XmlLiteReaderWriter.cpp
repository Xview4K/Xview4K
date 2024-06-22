//-----------------------------------------------------------------------
// This file is part of the Windows SDK Code Samples.
// 
// Copyright (C) Microsoft Corporation.  All rights reserved.
// 
// This source code is intended only as a supplement to Microsoft
// Development Tools and/or on-line documentation.  See these other
// materials for detailed information regarding Microsoft code samples.
// 
// THIS CODE AND INFORMATION ARE PROVIDED AS IS WITHOUT WARRANTY OF ANY
// KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//-----------------------------------------------------------------------

#include <ole2.h>
#include <xmllite.h>
#include <stdio.h>
#include <shlwapi.h>

#pragma warning(push)
#pragma warning(disable : 28196)
#pragma warning(disable: 6001)
#pragma warning(disable: 6054)
#pragma warning(disable: 26018)
#pragma warning(disable: 26036)
#pragma warning(disable: 26061)
#include <strsafe.h>
#pragma warning(pop)

#pragma warning(disable : 4127)  // conditional expression is constant
#define CHKHR(stmt)             do { hr = (stmt); if (FAILED(hr)) goto CleanUp; } while(0)
#define HR(stmt)                do { hr = (stmt); goto CleanUp; } while(0)
#define SAFE_RELEASE(I)         do { if (I){ I->Release(); } I = NULL; } while(0)

//implement filestream that derives from IStream
class FileStream : public IStream
{
    FileStream(HANDLE hFile) 
    { 
        _refcount = 1; 
        _hFile = hFile;
    }

    ~FileStream() 
    { 
        if (_hFile != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(_hFile);
        }
    }

public:
    HRESULT static OpenFile(LPCWSTR pName, IStream ** ppStream, bool fWrite)
    {
        HANDLE hFile = ::CreateFileW(pName, fWrite ? GENERIC_WRITE : GENERIC_READ, FILE_SHARE_READ,
            NULL, fWrite ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
            return HRESULT_FROM_WIN32(GetLastError());
        
        *ppStream = new FileStream(hFile);
        
        if(*ppStream == NULL)
            CloseHandle(hFile);
            
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void __RPC_FAR *__RPC_FAR *ppvObject)
    {
        if (!ppvObject)
            return E_INVALIDARG;
        (*ppvObject) = NULL;

        if (iid == __uuidof(IUnknown)
            || iid == __uuidof(IStream)
            || iid == __uuidof(ISequentialStream))
        {
            *ppvObject = static_cast<IStream*>(this);
            AddRef();
            return S_OK;
        } else
            return E_NOINTERFACE; 
    }

    virtual ULONG STDMETHODCALLTYPE AddRef(void) 
    { 
        return (ULONG)InterlockedIncrement(&_refcount); 
    }

    virtual ULONG STDMETHODCALLTYPE Release(void) 
    {
        ULONG res = (ULONG) InterlockedDecrement(&_refcount);
        if (res == 0) 
            delete this;
        return res;
    }

    // ISequentialStream Interface
public:
    virtual HRESULT STDMETHODCALLTYPE Read(_Out_writes_bytes_to_(cb, *pcbRead) void* pv, _In_ ULONG cb, _Out_opt_ ULONG* pcbRead)
    {
        BOOL rc = ReadFile(_hFile, pv, cb, pcbRead, NULL);
        return (rc) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    }

    virtual HRESULT STDMETHODCALLTYPE Write(_In_reads_bytes_(cb) const void* pv, _In_ ULONG cb, _Out_opt_ ULONG* pcbWritten)
    {
        BOOL rc = WriteFile(_hFile, pv, cb, pcbWritten, NULL);
        return rc ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    }

    // IStream Interface
public:
    virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER)
    { 
        return E_NOTIMPL;   
    }
    
    virtual HRESULT STDMETHODCALLTYPE CopyTo(_In_ IStream*, ULARGE_INTEGER, _Out_opt_ ULARGE_INTEGER*, _Out_opt_ ULARGE_INTEGER*)
    { 
        return E_NOTIMPL;   
    }
    
    virtual HRESULT STDMETHODCALLTYPE Commit(DWORD)                                      
    { 
        return E_NOTIMPL;   
    }
    
    virtual HRESULT STDMETHODCALLTYPE Revert(void)                                       
    { 
        return E_NOTIMPL;   
    }
    
    virtual HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)              
    { 
        return E_NOTIMPL;   
    }
    
    virtual HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)            
    { 
        return E_NOTIMPL;   
    }
    
    virtual HRESULT STDMETHODCALLTYPE Clone(__RPC__deref_out_opt IStream **)
    { 
        return E_NOTIMPL;   
    }

    virtual HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, _Out_opt_ ULARGE_INTEGER* lpNewFilePointer)
    { 
        DWORD dwMoveMethod;

        switch(dwOrigin)
        {
        case STREAM_SEEK_SET:
            dwMoveMethod = FILE_BEGIN;
            break;
        case STREAM_SEEK_CUR:
            dwMoveMethod = FILE_CURRENT;
            break;
        case STREAM_SEEK_END:
            dwMoveMethod = FILE_END;
            break;
        default:   
            return STG_E_INVALIDFUNCTION;
            break;
        }

        if (SetFilePointerEx(_hFile, liDistanceToMove, (PLARGE_INTEGER) lpNewFilePointer,
                             dwMoveMethod) == 0)
            return HRESULT_FROM_WIN32(GetLastError());
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Stat(__RPC__out STATSTG* pStatstg, DWORD )
    {
        if (GetFileSizeEx(_hFile, (PLARGE_INTEGER) &pStatstg->cbSize) == 0)
            return HRESULT_FROM_WIN32(GetLastError());
        return S_OK;
    }

private:
    HANDLE _hFile;
    LONG _refcount;
};

int __cdecl wmain(int argc, _In_reads_(argc) WCHAR* argv[])
{
    HRESULT hr = S_OK;
    IStream *pFileStream = NULL;
    IStream *pOutFileStream = NULL;
    IXmlReader *pReader = NULL;
    IXmlWriter *pWriter = NULL;
    IXmlReaderInput *pReaderInput = NULL;
    IXmlWriterOutput *pWriterOutput = NULL;

    if (argc != 3)
    {
        wprintf( L"Usage: XmlReaderWriter.exe name-of-input-file name-of-output-file\n");
        return 0;
    }

    //Open read-only input stream
    if (FAILED(hr = FileStream::OpenFile(argv[1], &pFileStream, FALSE)))
    {
        wprintf(L"Error creating file reader, error is %08.8lx", hr);
        HR(hr);
    }

    //Open writeable output stream
    if (FAILED(hr = FileStream::OpenFile(argv[2], &pOutFileStream, TRUE)))
    {
        wprintf(L"Error creating file writer, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = CreateXmlReader(__uuidof(IXmlReader), (void**) &pReader, NULL)))
    {
        wprintf(L"Error creating xml reader, error is %08.8lx", hr);
        HR(hr);
    }

    pReader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);

    if (FAILED(hr = CreateXmlReaderInputWithEncodingCodePage(pFileStream, NULL, 65001, FALSE,
                                                             L"c:\temp", &pReaderInput)))
    {
        wprintf(L"Error creating xml reader with encoding code page, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = CreateXmlWriter(__uuidof(IXmlWriter),(void**) &pWriter, NULL)))
    {
        wprintf(L"Error creating xml writer, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = CreateXmlWriterOutputWithEncodingCodePage(pOutFileStream, NULL, 65001,
                                                              &pWriterOutput)))
    {
        wprintf(L"Error creating xml reader with encoding code page, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pReader->SetInput(pReaderInput)))
    {
        wprintf(L"Error setting input for reader, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->SetOutput(pWriterOutput)))
    {
        wprintf(L"Error setting output for writer, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->SetProperty(XmlWriterProperty_Indent, TRUE)))
    {
        wprintf(L"Error setting indent property in writer, error is %08.8lx", hr);
        HR(hr);
    }

    XmlNodeType nodeType;
    const WCHAR* pQName;
    const WCHAR* pValue;
    double originalPrice = 0.0;
    double newPrice = 0.0;
    WCHAR priceString[100];
    bool inPrice = FALSE;

    /*
     * This quick start reads an XML of the form
     * <books>
     *   <book name="name of book1">
     *    <price>price-of-book1</price>
     *   </book>
     *   <book name="name of book2">
     *    <price>price-of-book2</price>
     *   </book>
     *  </books>
     *
     * and applies a 25% discount to the price of the book. It also adds originalPrice and discount
     * as two attributes to the element price. If the price is empty or an empty element, it does
     * not make any changes. So the transformed XML will be:
     *
     * <books>
     *   <book name="name of book1">
     *    <price originalPrice="price-of-book1" discount="25%">
     *           discounted-price-of-book1
     *       </price>
     *   </book>
     *   <book name="name of book2">
     *    <price originalPrice="price-of-book2" discount="25%">
     *           discounted-price-of-book2
     *       </price>
     *   </book>
     *  </books>
     *
     */

    //read until there are no more nodes
    while (S_OK == (hr = pReader->Read(&nodeType)))
    {

        // WriteNode will move the reader to the next node, so inside the While Read loop, it is
        // essential that we use WriteNodeShallow, which will not move the reader. 
        // If not, a node will be skipped every time you use WriteNode in the while loop.

        switch (nodeType)
        {
        case XmlNodeType_Element:
            {
                if (pReader->IsEmptyElement())
                {
                    if (FAILED(hr = pWriter->WriteNodeShallow(pReader, FALSE)))
                    {
                        wprintf(L"Error writing WriteNodeShallow, error is %08.8lx", hr);
                        HR(hr);
                    }
                }
                else
                {
                    // if you are not interested in the length it may be faster to use 
                    // NULL for the length parameter
                    if (FAILED(hr = pReader->GetQualifiedName(&pQName, NULL)))
                    {
                        wprintf(L"Error reading element name, error is %08.8lx", hr);
                        HR(hr);
                    }
                    
                    //if the element is price, then discount price by 25%   
                    if (pQName && wcscmp(pQName, L"price") == 0)
                    {
                        inPrice = TRUE;
                        if (FAILED(hr = pWriter->WriteNodeShallow(pReader, FALSE)))
                        {
                            wprintf(L"Error writing WriteNodeShallow, error is %08.8lx", hr);
                            HR(hr);
                        }


                    }
                    else
                    {
                        inPrice = FALSE;
                        if (FAILED(hr = pWriter->WriteNodeShallow(pReader, FALSE)))
                        {
                            wprintf(L"Error writing WriteNodeShallow, error is %08.8lx", hr);
                            HR(hr);
                        }

                    }
                }
            }
            break;
        case XmlNodeType_Text:
            if (inPrice == TRUE)
            {
                if (FAILED(hr = pReader->GetValue(&pValue, NULL)))
                {
                    wprintf(L"Error reading value, error is %08.8lx", hr);
                    HR(hr);
                }
                if (!pValue || L'\0' == (*pValue))
                    HR(E_UNEXPECTED);
                
                //apply discount to the node
                originalPrice = _wtof(pValue);
                newPrice = originalPrice * 0.75;

                if (FAILED(hr = StringCbPrintfW(priceString, sizeof (priceString), L"%f",
                                                newPrice)))
                {
                    wprintf(L"Error using StringCchPrintfW, error is %08.8lx", hr);
                    HR(hr);
                }

                //write attributes if any
                if (FAILED(hr = pWriter->WriteAttributeString(NULL, L"originalPrice", NULL,
                                                              pValue)))
                {
                    wprintf(L"Error writing WriteAttributeString, error is %08.8lx", hr);
                    HR(hr);
                }

                if (FAILED(hr = pWriter->WriteAttributeString(NULL, L"discount", NULL, L"25%")))
                {
                    wprintf(L"Error writing WriteAttributeString, error is %08.8lx", hr);
                    HR(hr);
                }
                if (FAILED(hr = pWriter->WriteString(priceString)))
                {
                    wprintf(L"Error writing WriteString, error is %08.8lx", hr);
                    HR(hr);
                }
                inPrice = FALSE;
            }
            else
            {
                if (FAILED(hr = pWriter->WriteNodeShallow(pReader, FALSE)))
                {
                    wprintf(L"Error writing WriteNodeShallow, error is %08.8lx", hr);
                    HR(hr);
                }
            }

            break;
        case XmlNodeType_EndElement:
            if (FAILED(hr = pReader->GetQualifiedName(&pQName, NULL)))
            {
                wprintf(L"Error reading element name, error is %08.8lx", hr);
                HR(hr);
            }
            
            if (pQName && wcscmp(pQName, L"price") == 0)  //if the end element is price, then reset inPrice
                inPrice = FALSE;
            
            if (FAILED(hr = pWriter->WriteFullEndElement()))
            {
                wprintf(L"Error writing WriteFullEndElement, error is %08.8lx", hr);
                HR(hr);
            }
            break;
        default:
            {
                if (FAILED(hr = pWriter->WriteNodeShallow(pReader, FALSE)))
                {
                    wprintf(L"Error writing WriteNodeShallow, error is %08.8lx", hr);
                    HR(hr);
                }
            }
            break;
        }

    }

CleanUp:
    SAFE_RELEASE(pFileStream);
    SAFE_RELEASE(pOutFileStream);
    SAFE_RELEASE(pReader);
    SAFE_RELEASE(pWriter);
    SAFE_RELEASE(pReaderInput);
    SAFE_RELEASE(pWriterOutput);
    return hr;
}

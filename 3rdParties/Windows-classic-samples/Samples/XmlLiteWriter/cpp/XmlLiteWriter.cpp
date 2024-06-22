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

#pragma warning(disable : 4127)  // conditional expression is constant
#define CHKHR(stmt)             do { hr = (stmt); if (FAILED(hr)) goto CleanUp; } while(0)
#define HR(stmt)                do { hr = (stmt); goto CleanUp; } while(0)
#define SAFE_RELEASE(I)         do { if (I){ I->Release(); } I = NULL; } while(0)

int __cdecl wmain(int argc, _In_reads_(argc) WCHAR* argv[])
{
    HRESULT hr = S_OK;
    IStream *pOutFileStream = NULL;
    IXmlWriter *pWriter = NULL;

    if (argc != 2)
    {
        wprintf(L"Usage: XmlLiteWriter.exe name-of-output-file\n");
        return 0;
    }

    //Open writeable output stream
    if (FAILED(hr = SHCreateStreamOnFile(argv[1], STGM_CREATE | STGM_WRITE, &pOutFileStream)))
    {
        wprintf(L"Error creating file writer, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = CreateXmlWriter(__uuidof(IXmlWriter), (void**) &pWriter, NULL)))
    {
        wprintf(L"Error creating xml writer, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->SetOutput(pOutFileStream)))
    {
        wprintf(L"Error, Method: SetOutput, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->SetProperty(XmlWriterProperty_Indent, TRUE)))
    {
        wprintf(L"Error, Method: SetProperty XmlWriterProperty_Indent, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteStartDocument(XmlStandalone_Omit)))
    {
        wprintf(L"Error, Method: WriteStartDocument, error is %08.8lx", hr);
        HR(hr);
    }

    // if you want to use a DTD using either the SYSTEM or PUBLIC identifiers,
    // or if you want to use an internal DTD subset, you can modify the following
    // call to WriteDocType.
    if (FAILED(hr = pWriter->WriteDocType(L"root", NULL, NULL, NULL)))
    {
        wprintf(L"Error, Method: WriteDocType, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteProcessingInstruction(L"xml-stylesheet",
        L"href=\"mystyle.css\" title=\"Compact\" type=\"text/css\"")))
    {
        wprintf(L"Error, Method: WriteProcessingInstruction, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"root", NULL)))
    {
        wprintf(L"Error, Method: WriteStartElement, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"sub", NULL)))
    {
        wprintf(L"Error, Method: WriteStartElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteAttributeString(NULL, L"myAttr", NULL,
                                                  L"1234")))
    {
        wprintf(L"Error, Method: WriteAttributeString, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteString(
               L"Markup is <escaped> for this string")))
    {
        wprintf(L"Error, Method: WriteString, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteFullEndElement()))
    {
        wprintf(L"Error, Method: WriteFullEndElement, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"anotherChild", NULL)))
    {
        wprintf(L"Error, Method: WriteStartElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteString(L"some text")))
    {
        wprintf(L"Error, Method: WriteString, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteFullEndElement()))
    {
        wprintf(L"Error, Method: WriteFullEndElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteCData(L"This is CDATA text.")))
    {
        wprintf(L"Error, Method: WriteCData, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"containsCharacterEntity", NULL)))
    {
        wprintf(L"Error, Method: WriteStartElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteCharEntity(L'a')))
    {
        wprintf(L"Error, Method: WriteCharEntity, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteFullEndElement()))
    {
        wprintf(L"Error, Method: WriteFullEndElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"containsChars", NULL)))
    {
        wprintf(L"Error, Method: WriteStartElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteChars(L"abcdefghijklm", 5)))
    {
        wprintf(L"Error, Method: WriteChars, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteFullEndElement()))
    {
        wprintf(L"Error, Method: WriteFullEndElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"containsEntity", NULL)))
    {
        wprintf(L"Error, Method: WriteStartElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteEntityRef(L"myEntity")))
    {
        wprintf(L"Error, Method: WriteEntityRef, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteEndElement()))
    {
        wprintf(L"Error, Method: WriteEndElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"containsName", NULL)))
    {
        wprintf(L"Error, Method: WriteStartElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteName(L"myName")))
    {
        wprintf(L"Error, Method: WriteName, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteEndElement()))
    {
        wprintf(L"Error, Method: WriteEndElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"containsNmToken", NULL)))
    {
        wprintf(L"Error, Method: WriteStartElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteNmToken(L"myNmToken")))
    {
        wprintf(L"Error, Method: WriteNmToken, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteEndElement()))
    {
        wprintf(L"Error, Method: WriteEndElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteComment(L"This is a comment")))
    {
        wprintf(L"Error, Method: WriteComment, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteRaw(L"<elementWrittenRaw/>")))
    {
        wprintf(L"Error, Method: WriteRaw, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteRawChars(L"<rawCharacters/>", 16)))
    {
        wprintf(L"Error, Method: WriteRawChars, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    if (FAILED(hr = pWriter->WriteElementString(NULL, L"myElement", NULL, L"myValue")))
    {
        wprintf(L"Error, Method: WriteElementString, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteFullEndElement()))
    {
        wprintf(L"Error, Method: WriteFullEndElement, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->WriteWhitespace(L"\n")))
    {
        wprintf(L"Error, Method: WriteWhitespace, error is %08.8lx", hr);
        HR(hr);
    }

    // WriteEndDocument closes any open elements or attributes
    if (FAILED(hr = pWriter->WriteEndDocument()))
    {
        wprintf(L"Error, Method: WriteEndDocument, error is %08.8lx", hr);
        HR(hr);
    }
    if (FAILED(hr = pWriter->Flush()))
    {
        wprintf(L"Error, Method: Flush, error is %08.8lx", hr);
        HR(hr);
    }

CleanUp:
    SAFE_RELEASE(pOutFileStream);
    SAFE_RELEASE(pWriter);
    return hr;
}

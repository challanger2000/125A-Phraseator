function(phraseator_patch_vstgui_windows_drop source_file)
    if(NOT WIN32)
        return()
    endif()

    if(NOT EXISTS "${source_file}")
        message(FATAL_ERROR "Phraseator: VSTGUI Win32 drop source not found: ${source_file}")
    endif()

    file(READ "${source_file}" source_text)

    set(old_block [=[
	STGMEDIUM medium = {};
	HRESULT hr = platformDataObject->QueryGetData (&formatTEXTDrop);
	if (hr == S_OK) // text
	{
		hr = platformDataObject->GetData (&formatTEXTDrop, &medium);
		if (hr == S_OK)
		{
			void* data = GlobalLock (medium.hGlobal);
			uint32_t dataSize = static_cast<uint32_t> (GlobalSize (medium.hGlobal));
			if (data && dataSize)
			{
				UTF8StringHelper wideString (static_cast<const WCHAR*> (data), dataSize / 2);
				strings.emplace_back (wideString);
				nbItems = 1;
			}
			GlobalUnlock (medium.hGlobal);
			if (medium.pUnkForRelease)
				medium.pUnkForRelease->Release ();
			else
				GlobalFree (medium.hGlobal);
		}
	}
	else if (hr != S_OK)
	{
		hr = platformDataObject->QueryGetData (&formatHDrop);
		if (hr == S_OK)
		{
			hr = platformDataObject->GetData (&formatHDrop, &medium);
			if (hr == S_OK)
			{
				nbItems = DragQueryFile ((HDROP)medium.hGlobal, 0xFFFFFFFFL, nullptr, 0);
				stringsAreFiles = true;

				TCHAR fileDropped[1024];
				for (uint32_t index = 0; index < nbItems; index++)
				{
					if (DragQueryFile ((HDROP)medium.hGlobal, index, fileDropped, sizeof (fileDropped) / 2)) 
					{
						// resolve link
						checkResolveLink (fileDropped, fileDropped);
						UTF8StringHelper path (fileDropped);
						strings.emplace_back (path);
					}
				}
			}
		}
		else if (platformDataObject->QueryGetData (&formatBinaryDrop) == S_OK)
		{
			if (platformDataObject->GetData (&formatBinaryDrop, &medium) == S_OK)
			{
				const void* blob = GlobalLock (medium.hGlobal);
				dataSize = static_cast<uint32_t> (GlobalSize (medium.hGlobal));
				if (blob && dataSize)
				{
					data = std::malloc (dataSize);
					if (data)
					{
						memcpy (data, blob, dataSize);
						nbItems = 1;
					}
				}
				GlobalUnlock (medium.hGlobal);
				if (medium.pUnkForRelease)
					medium.pUnkForRelease->Release ();
				else
					GlobalFree (medium.hGlobal);
			}
		}
	}
]=])

    set(new_block [=[
	STGMEDIUM medium = {};

	// Phraseator host-compatibility patch:
	// Prefer CF_HDROP over CF_UNICODETEXT. Some DAW browsers expose both.
	// Upstream VSTGUI checks text first, which hides the real file payload and
	// turns the drag into kText. Explorer usually exposes CF_HDROP directly,
	// which is why Explorer drops work while some host-browser drops do not.
	HRESULT hr = platformDataObject->QueryGetData (&formatHDrop);
	if (hr == S_OK)
	{
		hr = platformDataObject->GetData (&formatHDrop, &medium);
		if (hr == S_OK)
		{
			nbItems = DragQueryFile ((HDROP)medium.hGlobal, 0xFFFFFFFFL, nullptr, 0);
			stringsAreFiles = true;

			TCHAR fileDropped[1024];
			for (uint32_t index = 0; index < nbItems; index++)
			{
				if (DragQueryFile ((HDROP)medium.hGlobal, index, fileDropped, sizeof (fileDropped) / 2))
				{
					checkResolveLink (fileDropped, fileDropped);
					UTF8StringHelper path (fileDropped);
					strings.emplace_back (path);
				}
			}
			if (medium.pUnkForRelease)
				medium.pUnkForRelease->Release ();
			else
				GlobalFree (medium.hGlobal);
		}
	}
	else
	{
		hr = platformDataObject->QueryGetData (&formatTEXTDrop);
		if (hr == S_OK)
		{
			hr = platformDataObject->GetData (&formatTEXTDrop, &medium);
			if (hr == S_OK)
			{
				void* textData = GlobalLock (medium.hGlobal);
				uint32_t textDataSize = static_cast<uint32_t> (GlobalSize (medium.hGlobal));
				if (textData && textDataSize)
				{
					UTF8StringHelper wideString (static_cast<const WCHAR*> (textData), textDataSize / 2);
					strings.emplace_back (wideString);
					nbItems = 1;
				}
				GlobalUnlock (medium.hGlobal);
				if (medium.pUnkForRelease)
					medium.pUnkForRelease->Release ();
				else
					GlobalFree (medium.hGlobal);
			}
		}
		else if (platformDataObject->QueryGetData (&formatBinaryDrop) == S_OK)
		{
			if (platformDataObject->GetData (&formatBinaryDrop, &medium) == S_OK)
			{
				const void* blob = GlobalLock (medium.hGlobal);
				dataSize = static_cast<uint32_t> (GlobalSize (medium.hGlobal));
				if (blob && dataSize)
				{
					data = std::malloc (dataSize);
					if (data)
					{
						memcpy (data, blob, dataSize);
						nbItems = 1;
					}
				}
				GlobalUnlock (medium.hGlobal);
				if (medium.pUnkForRelease)
					medium.pUnkForRelease->Release ();
				else
					GlobalFree (medium.hGlobal);
			}
		}
	}
]=])

    string(FIND "${source_text}" "${old_block}" block_pos)
    if(block_pos EQUAL -1)
        string(FIND "${source_text}" "Phraseator host-compatibility patch" already_patched)
        if(already_patched EQUAL -1)
            message(FATAL_ERROR "Phraseator: expected VSTGUI Win32 drop block not found; refusing unverified patch")
        endif()
        return()
    endif()

    string(REPLACE "${old_block}" "${new_block}" source_text "${source_text}")
    file(WRITE "${source_file}" "${source_text}")
    message(STATUS "Phraseator: patched VSTGUI Win32 drop priority (CF_HDROP before CF_UNICODETEXT)")
endfunction()

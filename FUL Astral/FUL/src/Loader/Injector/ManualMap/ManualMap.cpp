#include "ManualMap.h"
#include "Native/Native.h"
#include "../../../Utils/FULVM.h"

#include <tlhelp32.h>
#include <cstdio>
#include <format>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <vector>

#pragma warning(push)
#pragma warning(disable: 28160)
#pragma warning(disable: 6250)
// This is CANCER, why did I decide to rewrite all the stealth options from guided hacking
using TLoadLibraryA = decltype(LoadLibraryA);
using TGetProcAddress = decltype(GetProcAddress);
using TDllMain = BOOL(WINAPI*)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

enum class MapResult : short
{
	Success = 0,
	NoData = 1,
	Pending = 2,
	Running = 3
};

struct ManualMapData
{
	BYTE* ImageBase;
	MapResult Result = MapResult::Success;

	TLoadLibraryA* FnLoadLibraryA;
	TGetProcAddress* FnGetProcAddress;
	TRtlInsertInvertedFunctionTable* FnIIFT;
};

// Options
constexpr bool HANDLE_TLS = true;
constexpr bool ENABLE_EXCEPTIONS = true;
constexpr bool ERASE_PEH = true;
constexpr bool CLEAR_SECTIONS = true;
constexpr bool ADJUST_PROTECTION = true;

// GH parity additions:
// RANDOMIZE_HEADER mimics GH -random: the TimeDateStamp/CheckSum of the image
// written into the target are randomized so a memory dump of the mapped image
// can't be signature-matched against the on-disk DLL header.
constexpr bool RANDOMIZE_HEADER = true;
// TIGHTEN_LOADER_PERMS: the loader stub page is never executable+writeable at
// the same time (RW during the write, then flipped to RX before it runs and
// stays RX). Removes the classic "private RWX region" scanner heuristic.
constexpr bool TIGHTEN_LOADER_PERMS = true;
// HIJACK_MAIN_THREAD supersedes GH -cloak: instead of creating a remote thread
// with a floating start address, a target thread is hijacked to run the loader
// and then NtContinue'd straight back to its suspended state. Fails over to
// the classic CreateRemoteThread path when hijacking isn't possible.
constexpr bool HIJACK_MAIN_THREAD = true;

// Timeout tuning:
// HIJACK_ARM_TIMEOUT_MS: stage-1 budget - how long a hijacked thread gets to
// prove the stub is executing (the stub writes 'Running' before it calls the
// loader, so this only measures "did the hijack take", never loader speed).
constexpr DWORD HIJACK_ARM_TIMEOUT_MS = 2000;
// LOADER_TIMEOUT_MS: stage-2 budget - once the stub is confirmed running, how
// long the loader (imports/TLS/DllMain) gets to finish.
constexpr DWORD LOADER_TIMEOUT_MS = 20000;

// ---------------------------------------------------------------------------
// Themida VM regions. Single straight-line blocks, one family per part, only
// unmarked calls inside. NO markers in Shellcode::LibraryLoader or the hijack
// stub bytes - that code runs inside the target process and must remain raw x64.
//
// NOTE: Shellcode::LibraryLoader performs guarded indirect calls (pDllMain). If
// this TU is ever compiled with /guard:cf, those calls will fast-fail in the
// target because manually mapped code is not in the target's CFG bitmap.
// ---------------------------------------------------------------------------
namespace
{
	struct MmImageInfo
	{
		const BYTE* src = nullptr;
		const IMAGE_NT_HEADERS* nt = nullptr;
	};

	// EAGLE_BLACK: validates the PE header chain of a candidate image.
	static __declspec(noinline) MmImageInfo image_parse_core(const Binary& binary)
	{
		VM_EAGLE_BLACK_START;
		MmImageInfo info;
		if (binary.size() >= sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS))
		{
			const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(binary.data());
			if (dos->e_magic == IMAGE_DOS_SIGNATURE)
			{
				if (dos->e_lfanew >= 0 && dos->e_lfanew + sizeof(IMAGE_NT_HEADERS) <= static_cast<int>(binary.size()))
				{
					const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(binary.data() + dos->e_lfanew);
					if (nt->Signature == IMAGE_NT_SIGNATURE)
					{
						info.src = binary.data();
						info.nt = nt;
					}
				}
			}
		}
		VM_EAGLE_BLACK_END;
		return info;
	}

	// LION_WHITE: maps section characteristics to a Windows memory protection.
	static __declspec(noinline) DWORD section_protect_core(DWORD characteristics)
	{
		VM_LION_WHITE_START;
		constexpr int protectionFlags[2][2][2] = {
			{
				// Not executable
				{ PAGE_NOACCESS, PAGE_WRITECOPY },
				{ PAGE_READONLY, PAGE_READWRITE }
			},
			{
				// Executable
				{ PAGE_EXECUTE, PAGE_EXECUTE_WRITECOPY },
				{ PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE }
			}
		};
		const bool bExecute = (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
		const bool bRead = (characteristics & IMAGE_SCN_MEM_READ) != 0;
		const bool bWrite = (characteristics & IMAGE_SCN_MEM_WRITE) != 0;
		const DWORD protection = protectionFlags[bExecute][bRead][bWrite];
		VM_LION_WHITE_END;
		return protection;
	}

	// LION_RED: zeroes a mapped region in the target and decommits it.
	static __declspec(noinline) bool erase_memory_core(HANDLE hTarget, LPVOID dest, SIZE_T size)
	{
		VM_LION_RED_START;
		bool ok = size != 0;
		if (ok)
		{
			const std::unique_ptr<BYTE> nullBuffer(new BYTE[size]{});
			DWORD flOldProtect = 0;
			if (!WriteProcessMemory(hTarget, dest, nullBuffer.get(), size, nullptr)) { ok = false; }
			if (ok && !VirtualProtectEx(hTarget, dest, size, PAGE_NOACCESS, &flOldProtect)) { ok = false; }
			if (ok && !VirtualFreeEx(hTarget, dest, size, MEM_DECOMMIT)) { ok = false; }
		}
		VM_LION_RED_END;
		return ok;
	}

	// FISH_WHITE: copies the image header and randomizes TimeDateStamp/CheckSum
	// (GH -random parity). The runtime never reads either field, so the mapped
	// image behaves identically while no longer hashing identical to the file.
	static __declspec(noinline) Binary header_randomize_core(const BYTE* src, DWORD size)
	{
		VM_FISH_WHITE_START;
		Binary out(src, src + size);
		UINT32 seed = static_cast<UINT32>(GetTickCount64()) ^ GetCurrentProcessId();
		seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
		const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(out.data());
		if (dos->e_magic == IMAGE_DOS_SIGNATURE)
		{
			const auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(out.data() + dos->e_lfanew);
			if (nt->Signature == IMAGE_NT_SIGNATURE)
			{
				nt->FileHeader.TimeDateStamp = seed;
				nt->OptionalHeader.CheckSum = 0;
			}
		}
		VM_FISH_WHITE_END;
		return out;
	}
}

#pragma runtime_checks( "", off )
namespace Shellcode
{
	void __stdcall LibraryLoader(ManualMapData* pData)
	{
		if (!pData)
		{
			// Note: cannot report NoData through a null pData - just bail out.
			return;
		}

		// Retrieve file headers
		BYTE* pBase = pData->ImageBase;
		const auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(pBase);
		const auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + dosHeader->e_lfanew);
		const IMAGE_OPTIONAL_HEADER* optHeader = &ntHeaders->OptionalHeader;

		// Retrieve our function pointers
		const auto pLoadLibraryA = pData->FnLoadLibraryA;
		const auto pGetProcAddress = pData->FnGetProcAddress;
		const auto pRtlInsertInvertedFunctionTable = pData->FnIIFT;
		const auto pDllMain = reinterpret_cast<TDllMain>(pBase + optHeader->AddressOfEntryPoint);

		// Base address relocation
		BYTE* locationDelta = pBase - optHeader->ImageBase;
		if (locationDelta)
		{
			const auto relocData = optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
			if (relocData.Size)
			{
				auto* pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(pBase + relocData.VirtualAddress);
				const auto* pRelocEnd = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<uintptr_t>(pRelocData) + relocData.Size);
				while (pRelocData < pRelocEnd && pRelocData->SizeOfBlock)
				{
					const UINT nEntries = (pRelocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
					auto pRelativeInfo = reinterpret_cast<WORD*>(pRelocData + 1);

					for (UINT i = 0; i < nEntries; i++, pRelativeInfo++)
					{
						const int type = *pRelativeInfo >> 0xC;
						const int offset = *pRelativeInfo & 0xFFF;

						if (type == IMAGE_REL_BASED_ABSOLUTE)
						{
							continue;
						}
						if (type == IMAGE_REL_BASED_DIR64)
						{
							const auto pPatch = reinterpret_cast<ULONG_PTR*>(pBase + pRelocData->VirtualAddress + offset);
							*pPatch += reinterpret_cast<ULONG_PTR>(locationDelta);
						}
						else if (type == IMAGE_REL_BASED_HIGHLOW)
						{
							const auto pPatch = reinterpret_cast<UINT_PTR*>(pBase + pRelocData->VirtualAddress + offset);
							*pPatch += reinterpret_cast<UINT_PTR>(locationDelta);
						}
					}

					pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(pRelocData) + pRelocData->SizeOfBlock);
				}
			}
		}

		// Resolve imports
		const auto importData = optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (importData.Size)
		{
			const auto* pImportDescr = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(pBase + importData.VirtualAddress);
			while (pImportDescr->Name)
			{
				const auto szMod = reinterpret_cast<char*>(pBase + pImportDescr->Name);
				const HINSTANCE hDll = pLoadLibraryA(szMod);

				auto pThunkRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->OriginalFirstThunk);
				auto pFuncRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->FirstThunk);
				if (!pThunkRef) { pThunkRef = pFuncRef; }

				for (; *pThunkRef; pThunkRef++, pFuncRef++)
				{
					if (IMAGE_SNAP_BY_ORDINAL(*pThunkRef))
					{
						*pFuncRef = reinterpret_cast<ULONG_PTR>(pGetProcAddress(hDll, reinterpret_cast<char*>(*pThunkRef & 0xFFFF)));
					}
					else
					{
						const auto* pImport = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + (*pThunkRef));
						*pFuncRef = reinterpret_cast<ULONG_PTR>(pGetProcAddress(hDll, pImport->Name));
					}
				}
				++pImportDescr;
			}
		}

		// Handle TLS callbacks
		if (HANDLE_TLS)
		{
			const auto tlsData = optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
			if (tlsData.Size)
			{
				const auto* pTLS = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(pBase + tlsData.VirtualAddress);
				const auto* pCallback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(pTLS->AddressOfCallBacks);

				while (pCallback && *pCallback)
				{
					(*pCallback)(pBase, DLL_PROCESS_ATTACH, nullptr);
					pCallback++;
				}
			}
		}

		// Exception support
		if (ENABLE_EXCEPTIONS)
		{
			if (pRtlInsertInvertedFunctionTable)
			{
				pRtlInsertInvertedFunctionTable(pBase, optHeader->SizeOfImage);
			}
		}

		// Invoke original entry point
		pDllMain(reinterpret_cast<HINSTANCE>(pBase), DLL_PROCESS_ATTACH, nullptr);

		pData->Result = MapResult::Success;
	}

	DWORD __stdcall Stub() { return 0; }
}
#pragma runtime_checks( "", restore )

DWORD GetSectionProtection(DWORD characteristics)
{
	return section_protect_core(characteristics);
}

bool EraseMemory(HANDLE hTarget, LPVOID dest, SIZE_T size = 0)
{
	return erase_memory_core(hTarget, dest, size);
}

// ---------------------------------------------------------------------------
// Hijack support (injector-side, unmarked - pure setup/query work).
// ---------------------------------------------------------------------------

// Outcome of polling the remote ManualMapData.
enum class PollOutcome { Completed, Timeout, ThreadGone, ProcessGone };

// Access mask for hijack candidate threads. THREAD_QUERY_LIMITED_INFORMATION
// is required for the GetExitCodeThread liveness checks during polling.
constexpr DWORD kHijackThreadAccess =
THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_LIMITED_INFORMATION;

// Collects hijack candidate handles for the target, ordered by thread ID
// ascending (lowest TID is by convention the process's first / main thread).
// Threads the caller already gave us are excluded via excludeTid.
static std::vector<HANDLE> FindHijackCandidates(DWORD pid, DWORD excludeTid)
{
	std::vector<DWORD> tids;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snapshot != INVALID_HANDLE_VALUE)
	{
		THREADENTRY32 entry = {};
		entry.dwSize = sizeof(entry);
		if (Thread32First(snapshot, &entry))
		{
			do
			{
				if (entry.th32OwnerProcessID == pid &&
					entry.th32ThreadID != 0 &&
					entry.th32ThreadID != excludeTid)
				{
					tids.push_back(entry.th32ThreadID);
				}
			} while (Thread32Next(snapshot, &entry));
		}
		CloseHandle(snapshot);
	}

	std::sort(tids.begin(), tids.end());

	std::vector<HANDLE> handles;
	handles.reserve(tids.size());
	for (DWORD tid : tids)
	{
		HANDLE h = OpenThread(kHijackThreadAccess, FALSE, tid);
		if (h) { handles.push_back(h); }
	}
	return handles;
}

static const void* ResolveNtContinue()
{
	const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (!ntdll) { return nullptr; }
	return reinterpret_cast<const void*>(GetProcAddress(ntdll, "NtContinue"));
}

// Restores a hijack candidate's original context and lets it run again. Only
// used when the stub provably never executed (Result still Pending), so the
// saved CONTEXT is exactly the state the thread was in when we suspended it.
static bool RestoreAndResume(HANDLE hThread, const CONTEXT& saved)
{
	if (SuspendThread(hThread) == 0xFFFFFFFF)
	{
		// Thread terminated (or handle stale); nothing to restore.
		return false;
	}

	CONTEXT restore = saved;
	const bool ok = SetThreadContext(hThread, &restore) != 0;
	ResumeThread(hThread);
	return ok;
}

// Layout of the hijack block: [0, 96) stub code, [96, 96+sizeof(CONTEXT)) the
// saved CONTEXT. 96 gives room for the full stub and keeps pSavedCtx 16-byte
// aligned inside the page-aligned allocation. The code is position-independent
// raw x64:
//
//   and  rsp, -16              ; align (hijack entry RSP is arbitrary)
//   sub  rsp, 0x20             ; shadow space
//   mov  rax, <pMapData>
//   mov  rcx, rax              ; rcx = pMapData (loader argument)
//   mov  word ptr [rax+8], 3   ; mapData->Result = Running (liveness signal;
//                               ; written *before* the loader runs)
//   mov  rax, <pLoader>
//   call rax                   ; LibraryLoader(pMapData)
//   mov  rax, <pContext>
//   mov  rcx, rax
//   xor  edx, edx
//   mov  rax, <NtContinue>
//   call rax                   ; NtContinue(&ctx, FALSE) - never returns,
//                               ; thread resumes exactly where it was suspended
//   int3                       ; safety net
constexpr SIZE_T kHijackCodeSize = 96;

static Binary BuildHijackStub(BYTE* pMapData, BYTE* pLoader, BYTE* pContext, const void* pNtContinue)
{
	Binary stub;
	stub.reserve(kHijackCodeSize);

	BYTE runningLo = 0;
	BYTE runningHi = 0;
	{
		const auto running = static_cast<short>(MapResult::Running);
		runningLo = static_cast<BYTE>(running & 0xFF);
		runningHi = static_cast<BYTE>((running >> 8) & 0xFF);
	}

	const auto byte = [&](BYTE b) { stub.push_back(b); };
	const auto imm64 = [&](uintptr_t v) { for (int i = 0; i < 8; ++i) byte(static_cast<BYTE>((v >> (i * 8)) & 0xFF)); };
	const auto movRax = [&](uintptr_t v) { byte(0x48); byte(0xB8); imm64(v); };

	byte(0x48); byte(0x83); byte(0xE4); byte(0xF0); // and rsp, -16
	byte(0x48); byte(0x83); byte(0xEC); byte(0x20); // sub rsp, 0x20

	movRax(reinterpret_cast<uintptr_t>(pMapData));      // mov rax, pMapData
	byte(0x48); byte(0x89); byte(0xC1);                 // mov rcx, rax

	// mov word ptr [rax+8], MapResult::Running
	// (ManualMapData::Result is the 'short' at offset 8 of ManualMapData)
	byte(0x66); byte(0xC7); byte(0x40); byte(0x08);
	byte(runningLo);
	byte(runningHi);

	movRax(reinterpret_cast<uintptr_t>(pLoader));       // mov rax, pLoader
	byte(0xFF); byte(0xD0);                             // call rax

	movRax(reinterpret_cast<uintptr_t>(pContext));      // mov rax, pContext
	byte(0x48); byte(0x89); byte(0xC1);                 // mov rcx, rax
	byte(0x31); byte(0xD2);                             // xor edx, edx
	movRax(reinterpret_cast<uintptr_t>(pNtContinue));  // mov rax, NtContinue
	byte(0xFF); byte(0xD0);                             // call rax

	byte(0xCC);                                         // int3
	while (stub.size() < kHijackCodeSize) { byte(0xCC); }
	return stub;
}

bool MM::Inject(HANDLE hTarget, const Binary& binary, HANDLE mainThread)
{
	DWORD flOldProtect = 0;

	// Retrieve the file headers (VM'd PE parse, unmarked checks around it)
	const MmImageInfo imageInfo = image_parse_core(binary);
	if (!imageInfo.nt)
	{
		throw std::invalid_argument("The file is not a valid PE");
	}
	const BYTE* pSrcData = imageInfo.src;
	const auto ntHeaders = imageInfo.nt;
	const IMAGE_FILE_HEADER* fileHeader = &ntHeaders->FileHeader;
	const IMAGE_OPTIONAL_HEADER* optHeader = &ntHeaders->OptionalHeader;

	// Allocate memory for the binary file
	const auto pTargetBase = static_cast<BYTE*>(VirtualAllocEx(hTarget, nullptr, optHeader->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (!pTargetBase)
	{
		throw std::runtime_error("Failed to allocate memory in the target process");
	}

	VirtualProtectEx(hTarget, pTargetBase, optHeader->SizeOfImage, PAGE_EXECUTE_READWRITE, &flOldProtect);

	// Write file header (randomized when enabled so the in-memory image is no
	// longer byte-identical to the on-disk DLL header)
	if (RANDOMIZE_HEADER)
	{
		Log::Info("MM: Randomizing PE header artifacts...");
		const Binary headerCopy = header_randomize_core(pSrcData, optHeader->SizeOfHeaders);
		if (!WriteProcessMemory(hTarget, pTargetBase, headerCopy.data(), headerCopy.size(), nullptr))
		{
			VirtualFreeEx(hTarget, pTargetBase, 0, MEM_RELEASE);
			throw std::runtime_error("Failed to write PE header to target");
		}
	}
	else if (!WriteProcessMemory(hTarget, pTargetBase, pSrcData, optHeader->SizeOfHeaders, nullptr))
	{
		VirtualFreeEx(hTarget, pTargetBase, 0, MEM_RELEASE);
		throw std::runtime_error("Failed to write PE header to target");
	}

	Log::Debug("MM: PE header @ {:p}", static_cast<void*>(pTargetBase));

	// Init manual map data. Result starts as Pending: it is flipped to Running
	// by the hijack stub (proving the hijack took), and to Success/NoData by
	// the target-side loader.
	const ManualMapData mapData{
		.ImageBase = pTargetBase,
		.Result = MapResult::Pending,
		.FnLoadLibraryA = LoadLibraryA,
		.FnGetProcAddress = GetProcAddress,
		.FnIIFT = Native::GetRtlInsertInvertedFunctionTable()
	};

	// Write sections
	auto pSectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
	for (UINT i = 0; i < fileHeader->NumberOfSections; i++, pSectionHeader++)
	{
		if (pSectionHeader->SizeOfRawData == 0) { continue; }

		if (pSectionHeader->Characteristics & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE))
		{
			const auto sectionName = reinterpret_cast<LPCSTR>(pSectionHeader->Name);
			Log::Debug("MM: Writing section '{}'...", sectionName);
			if (!WriteProcessMemory(hTarget, pTargetBase + pSectionHeader->VirtualAddress, pSrcData + pSectionHeader->PointerToRawData, pSectionHeader->SizeOfRawData, nullptr))
			{
				VirtualFreeEx(hTarget, pTargetBase, 0, MEM_RELEASE);
				throw std::runtime_error(std::format("Failed to map section: {}", sectionName));
			}
		}
	}

	// Write manual map data
	const auto pMapData = static_cast<BYTE*>(VirtualAllocEx(hTarget, nullptr, sizeof(ManualMapData), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (!pMapData)
	{
		VirtualFreeEx(hTarget, pTargetBase, 0, MEM_RELEASE);
		throw std::runtime_error("Failed to allocate mapping data memory");
	}

	if (!WriteProcessMemory(hTarget, pMapData, &mapData, sizeof(mapData), nullptr))
	{
		VirtualFreeEx(hTarget, pMapData, 0, MEM_RELEASE);
		VirtualFreeEx(hTarget, pTargetBase, 0, MEM_RELEASE);
		throw std::runtime_error("Failed to write mapping data to target");
	}

	Log::Debug("MM: Manual Map Data @ {:p}", static_cast<void*>(pMapData));

	// Write library loader. Allocated RW and flipped to RX before execution so
	// the region is never simultaneously writable and executable.
	const SIZE_T loaderSize = reinterpret_cast<uintptr_t>(Shellcode::Stub) - reinterpret_cast<uintptr_t>(Shellcode::LibraryLoader);
	const LPVOID pLoader = VirtualAllocEx(hTarget, nullptr,
		loaderSize, MEM_COMMIT | MEM_RESERVE, TIGHTEN_LOADER_PERMS ? PAGE_READWRITE : PAGE_EXECUTE_READWRITE);
	if (!pLoader)
	{
		VirtualFreeEx(hTarget, pMapData, 0, MEM_RELEASE);
		VirtualFreeEx(hTarget, pTargetBase, 0, MEM_RELEASE);
		throw std::runtime_error("Failed to allocate library loader memory");
	}

	if (!WriteProcessMemory(hTarget, pLoader, &Shellcode::LibraryLoader, loaderSize, nullptr))
	{
		VirtualFreeEx(hTarget, pLoader, 0, MEM_RELEASE);
		VirtualFreeEx(hTarget, pMapData, 0, MEM_RELEASE);
		VirtualFreeEx(hTarget, pTargetBase, 0, MEM_RELEASE);
		throw std::runtime_error("Failed to write library loader to target");
	}

	if (TIGHTEN_LOADER_PERMS)
	{
		DWORD unused = 0;
		VirtualProtectEx(hTarget, pLoader, loaderSize, PAGE_EXECUTE_READ, &unused);
	}

	Log::Debug("MM: Library Loader @ {:p}", static_cast<void*>(pLoader));

	MapResult result = MapResult::Pending;

	// Polls the remote map data until it leaves Pending, the budget expires, or
	// the thread/process is observed to have died. Reads every 10ms.
	auto pollMapResult = [&](HANDLE hThread, DWORD timeoutMs) -> std::pair<PollOutcome, MapResult>
		{
			const DWORD start = GetTickCount();
			for (;;)
			{
				MapResult current = MapResult::Pending;
				ManualMapData remote = {};
				if (ReadProcessMemory(hTarget, pMapData, &remote, sizeof(remote), nullptr))
				{
					current = remote.Result;
				}
				if (current != MapResult::Pending)
				{
					return { PollOutcome::Completed, current };
				}

				DWORD code = 0;
				if (!GetExitCodeProcess(hTarget, &code) || code != STILL_ACTIVE)
				{
					return { PollOutcome::ProcessGone, current };
				}

				// If the handle lacks query access GetExitCodeThread fails and the
				// liveness check is simply skipped.
				if (hThread && GetExitCodeThread(hThread, &code) && code != STILL_ACTIVE)
				{
					return { PollOutcome::ThreadGone, current };
				}

				if (GetTickCount() - start > timeoutMs)
				{
					return { PollOutcome::Timeout, current };
				}

				Sleep(10);
			}
		};

	bool hijacked = false;
	BYTE* pHijackBlock = nullptr;
	HANDLE hLoaderThread = nullptr; // thread that ran (is running) the loader
	bool ownsLoaderThread = false;
	bool callerThreadTouched = false; // set if mainThread was ever suspended by the candidate loop

	if (HIJACK_MAIN_THREAD && pLoader)
	{
		const void* pNtContinue = ResolveNtContinue();

		std::vector<HANDLE> attemptOrder;
		if (mainThread)
		{
			attemptOrder.push_back(mainThread); // caller's pick first (not owned)
		}
		{
			auto candidates = FindHijackCandidates(GetProcessId(hTarget), mainThread ? GetThreadId(mainThread) : 0);
			attemptOrder.insert(attemptOrder.end(), candidates.begin(), candidates.end());
		}


		if (pNtContinue && !attemptOrder.empty())
		{
			constexpr SIZE_T hijackTotal = kHijackCodeSize + sizeof(CONTEXT);
			BYTE* hBlock = static_cast<BYTE*>(VirtualAllocEx(hTarget, nullptr, hijackTotal, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
			if (hBlock)
			{
				BYTE* pSavedCtx = hBlock + kHijackCodeSize;
				// The stub is identical for every candidate (it only references
				// fixed remote addresses); it is re-written per attempt below
				// together with the candidate-specific saved CONTEXT.
				const Binary stub = BuildHijackStub(pMapData, static_cast<BYTE*>(pLoader), pSavedCtx, pNtContinue);

				for (HANDLE candidate : attemptOrder)
				{
					const bool owns = (candidate != mainThread);
					if (candidate == mainThread) { callerThreadTouched = true; }

					// -- Suspend & sanity checks --------------------------------
					const DWORD prevSuspend = SuspendThread(candidate);
					if (prevSuspend == 0xFFFFFFFF)
					{
						Log::Warn("MM: Hijack candidate could not be suspended, skipping");
						if (owns) { CloseHandle(candidate); }
						continue;
					}
					if (prevSuspend != 0)
					{
						// The game itself had this thread suspended; a single
						// resume from us leaves it suspended and our stub would
						// never execute. Undo and skip it.
						ResumeThread(candidate);
						Log::Debug("MM: Hijack candidate is game-suspended, skipping");
						if (owns) { CloseHandle(candidate); }
						continue;
					}

					CONTEXT savedCtx = {};
					savedCtx.ContextFlags = CONTEXT_FULL;
					if (!GetThreadContext(candidate, &savedCtx))
					{
						ResumeThread(candidate);
						if (owns) { CloseHandle(candidate); }
						continue;
					}

					// -- Arm the block for this candidate ----------------------
					DWORD unused = 0;
					VirtualProtectEx(hTarget, hBlock, hijackTotal, PAGE_READWRITE, &unused); // ensure writable for (re)arming
					bool armed =
						WriteProcessMemory(hTarget, hBlock, stub.data(), stub.size(), nullptr) &&
						WriteProcessMemory(hTarget, pSavedCtx, &savedCtx, sizeof(savedCtx), nullptr) != 0;
					if (armed && TIGHTEN_LOADER_PERMS)
					{
						// The stub and saved context are read-only once armed;
						// only pMapData (the loader's result slot) stays RW.
						armed = VirtualProtectEx(hTarget, hBlock, hijackTotal, PAGE_EXECUTE_READ, &unused) != 0;
					}

					// -- Redirect RIP and verify it took ------------------------
					if (armed)
					{
						CONTEXT redirect = savedCtx;
						redirect.Rip = reinterpret_cast<DWORD64>(hBlock);
						redirect.ContextFlags = CONTEXT_CONTROL;
						armed = SetThreadContext(candidate, &redirect) != 0;

						if (armed)
						{
							// Readback: under some hooks/ACs SetThreadContext
							// can fail silently; verify before committing.
							CONTEXT verify = {};
							verify.ContextFlags = CONTEXT_CONTROL;
							armed = GetThreadContext(candidate, &verify) &&
								verify.Rip == reinterpret_cast<DWORD64>(hBlock);
						}
					}

					if (!armed)
					{
						CONTEXT restore = savedCtx;
						SetThreadContext(candidate, &restore);
						ResumeThread(candidate);
						Log::Warn("MM: Hijack arming failed on a candidate, trying the next");
						if (owns) { CloseHandle(candidate); }
						continue;
					}

					// -- Armed: resume, the stub writes 'Running' immediately --
					ResumeThread(candidate);

					// Stage 1 (short budget): did the stub execute?
					auto [stage1Outcome, stage1Result] = pollMapResult(candidate, HIJACK_ARM_TIMEOUT_MS);

					if (stage1Outcome == PollOutcome::ProcessGone)
					{
						if (owns) { CloseHandle(candidate); }
						DWORD exitCode = 0;
						GetExitCodeProcess(hTarget, &exitCode);
						throw std::runtime_error(std::format("Process crashed with exit code: {:X}", exitCode));
					}

					if (stage1Outcome != PollOutcome::Completed)
					{
						// Stub never ran (timeout) or the candidate died before
						// reaching user mode. Either way this thread is a dead
						// end: restore its original context and move on.
						if (stage1Outcome == PollOutcome::Timeout)
						{
							RestoreAndResume(candidate, savedCtx);
							Log::Warn("MM: Hijack candidate never executed the stub, restored and trying the next");
						}
						else
						{
							Log::Warn("MM: Hijack candidate died during arming, trying the next");
						}
						if (owns) { CloseHandle(candidate); }
						continue;
					}

					// Stage 1 completed. If the loader already finished (fast
					// target + slow poll) stage1Result is final; otherwise it
					// is Running and stage 2 gives the loader its full budget.
					if (stage1Result == MapResult::Running)
					{
						Log::Info("MM: Hijack armed, waiting for the hijacked thread loader...");
						auto [stage2Outcome, stage2Result] = pollMapResult(candidate, LOADER_TIMEOUT_MS);

						if (stage2Outcome != PollOutcome::Completed)
						{
							// The loader owns this thread now; restoring its
							// context would be pointless. Leak the hijack
							// block, map data and image rather than freeing
							// memory the target is executing out of.
							if (owns) { CloseHandle(candidate); }
							if (stage2Outcome == PollOutcome::ProcessGone)
							{
								DWORD exitCode = 0;
								GetExitCodeProcess(hTarget, &exitCode);
								throw std::runtime_error(std::format("Process crashed with exit code: {:X}", exitCode));
							}
							if (stage2Outcome == PollOutcome::ThreadGone)
							{
								throw std::runtime_error("Hijacked thread died while running the loader");
							}
							throw std::runtime_error("Timeout in the loader after it acquired the hijacked thread (DllMain/load deadlock?)");
						}
						result = stage2Result;
					}
					else
					{
						result = stage1Result;
					}

					hijacked = true;
					pHijackBlock = hBlock;
					hLoaderThread = candidate;
					ownsLoaderThread = owns;
					Log::Info("MM: Hijacked a target thread @ {:p}", static_cast<void*>(hBlock));
					break;
				}

				if (!hijacked)
				{
					VirtualFreeEx(hTarget, hBlock, 0, MEM_RELEASE);
				}
			}
		}

		// Any candidate handles left over after a successful hijack of an
		// enumerated thread... none exist here: success Break-locked paths close
		// or transfer every other handle above (this comment is a placeholder
		// for the reader; nothing to do).
	}

	if (hijacked)
	{
		// Wait for the thread to leave the stub (NtContinue done) before the
		// block is freed; the Result flag is set before NtContinue is reached.
		CONTEXT check = {};
		check.ContextFlags = CONTEXT_CONTROL;
		for (int i = 0; i < 100; ++i)
		{
			if (!GetThreadContext(hLoaderThread, &check)) { break; }
			if (check.Rip < reinterpret_cast<DWORD64>(pHijackBlock) ||
				check.Rip >= reinterpret_cast<DWORD64>(pHijackBlock) + kHijackCodeSize)
			{
				break;
			}
			Sleep(5);
		}

		if (ownsLoaderThread && hLoaderThread)
		{
			CloseHandle(hLoaderThread);
			hLoaderThread = nullptr;
		}
	}
	else
	{
		// Classic path: visible remote thread (fallback when hijacking is off,
		// failed on every candidate, or no candidates existed).

		// Preserve the legacy behavior of waking a caller-provided suspended
		// main thread before the loader runs - but only if the candidate loop
		// never touched it (a failed hijack attempt already restored it).
		if (mainThread && !callerThreadTouched)
		{
			ResumeThread(mainThread);
		}

		const HANDLE hThread = CreateRemoteThread(hTarget, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoader), pMapData, 0, nullptr);
		if (!hThread)
		{
			VirtualFreeEx(hTarget, pLoader, 0, MEM_RELEASE);
			VirtualFreeEx(hTarget, pMapData, 0, MEM_RELEASE);
			VirtualFreeEx(hTarget, pTargetBase, 0, MEM_RELEASE);
			throw std::runtime_error("Failed to create the remote thread");
		}

		// Wait for the library loader
		Log::Info("MM: Waiting for target thread...");
		if (WaitForSingleObject(hThread, LOADER_TIMEOUT_MS) != WAIT_OBJECT_0)
		{
			CloseHandle(hThread);
			throw std::runtime_error("Timeout while waiting for thread");
		}

		CloseHandle(hThread);

		ManualMapData resultData{};
		ReadProcessMemory(hTarget, pMapData, &resultData, sizeof(resultData), nullptr);
		result = resultData.Result;
	}

	// Check injection result
	{
		DWORD exitCode = 0;
		GetExitCodeProcess(hTarget, &exitCode);
		if (exitCode != STILL_ACTIVE)
		{
			throw std::runtime_error(std::format("Process crashed with exit code: {:X}", exitCode));
		}

		if (result == MapResult::NoData)
		{
			VirtualFreeEx(hTarget, pLoader, 0, MEM_RELEASE);
			VirtualFreeEx(hTarget, pMapData, 0, MEM_RELEASE);
			VirtualFreeEx(hTarget, pTargetBase, 0, MEM_RELEASE);
			if (pHijackBlock) { VirtualFreeEx(hTarget, pHijackBlock, 0, MEM_RELEASE); }
			throw std::runtime_error("Manual map data was invalid");
		}

		Log::Debug("MM: LibraryLoader result was: {:d}", static_cast<int>(result));
	}

	// (Optional) Adjust the section protection
	if (ADJUST_PROTECTION)
	{
		Log::Info("MM: Adjusting protections...");

		pSectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
		for (UINT i = 0; i != fileHeader->NumberOfSections; i++, pSectionHeader++)
		{
			if (pSectionHeader->Misc.VirtualSize == 0) { continue; }

			const auto sectionName = reinterpret_cast<LPCSTR>(pSectionHeader->Name);
			const DWORD flNewProtect = GetSectionProtection(pSectionHeader->Characteristics);
			if (flNewProtect == PAGE_NOACCESS)
			{
				// Decommit NO_ACCESS pages
				if (!VirtualFreeEx(hTarget, pTargetBase + pSectionHeader->VirtualAddress, pSectionHeader->Misc.VirtualSize, MEM_DECOMMIT))
				{
					Log::Warn("MM: Failed to free NO_ACCESS section '{}'", sectionName);
				}
			}
			else
			{
				// Change protection
				if (!VirtualProtectEx(hTarget, pTargetBase + pSectionHeader->VirtualAddress, pSectionHeader->Misc.VirtualSize, flNewProtect, &flOldProtect))
				{
					Log::Warn("MM: Failed to set section '{}' to {}", sectionName, flNewProtect);
				}
			}
		}
	}

	// (Optional) Clear unneeded sections
	if (CLEAR_SECTIONS)
	{
		Log::Info("MM: Clearing sections...");

		pSectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
		for (UINT i = 0; i < fileHeader->NumberOfSections; ++i, ++pSectionHeader)
		{
			const DWORD discardable = pSectionHeader->Characteristics & IMAGE_SCN_MEM_DISCARDABLE;
			if (pSectionHeader->Misc.VirtualSize && discardable)
			{
				const auto sectionName = reinterpret_cast<LPCSTR>(pSectionHeader->Name);
				Log::Debug("MM: Discarding section '{}'...", sectionName);
				if (!EraseMemory(hTarget, pTargetBase + pSectionHeader->VirtualAddress, pSectionHeader->Misc.VirtualSize))
				{
					Log::Warn("MM: Failed to discard section '{}'", sectionName);
				}
			}
		}
	}

	// (Optional) Clear PE header
	if (ERASE_PEH)
	{
		Log::Info("MM: Erasing PE header...");

		if (!EraseMemory(hTarget, pTargetBase, optHeader->SizeOfHeaders))
		{
			Log::Warn("MM: Failed to erase PE header");
		}
	}

	// Cleanup
	VirtualFreeEx(hTarget, pLoader, 0, MEM_RELEASE);
	VirtualFreeEx(hTarget, pMapData, 0, MEM_RELEASE);
	if (pHijackBlock) { VirtualFreeEx(hTarget, pHijackBlock, 0, MEM_RELEASE); }

	Log::Info("MM: Done!");
	return true;
}

#pragma warning(pop)

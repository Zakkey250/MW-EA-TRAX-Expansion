#pragma once
#include <Windows.h>
#include <string>
#include "StartupText.h"

namespace eatrax::startup {
inline SRWLOCK progressLock = SRWLOCK_INIT;
enum class Stage { Checking, Generating, Verifying, Complete };
inline Stage progressStage = Stage::Checking;
inline std::wstring progress;
inline void Update(Stage stage, std::wstring text = {}) {
    AcquireSRWLockExclusive(&progressLock);
    progressStage = stage;
    progress = std::move(text);
    ReleaseSRWLockExclusive(&progressLock);
}
struct WaitContext { HANDLE ready; HANDLE cancel; ULONGLONG started; ULONGLONG cancelled = 0; };
inline void Cancel(HWND window, WaitContext& context) {
    if (!context.cancelled) {
        context.cancelled = GetTickCount64();
        SetEvent(context.cancel);
        EnableWindow(GetDlgItem(window, IDCANCEL), FALSE);
        SetDlgItemTextW(window, 100, currentText->cancelling);
    }
}
inline INT_PTR CALLBACK Dialog(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto* context = reinterpret_cast<WaitContext*>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        context = reinterpret_cast<WaitContext*>(lp);
        SetWindowLongPtrW(window, DWLP_USER, lp);
        SetWindowTextW(window, (std::wstring(L"EA TRAX Expansion — ")+currentText->title).c_str());
        auto control = [&](const wchar_t* kind, const wchar_t* text, DWORD style,
                           int x, int y, int width, int height, int id) {
            HWND child = CreateWindowW(kind, text, WS_CHILD | WS_VISIBLE | style,
                x,y,width,height,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        };
        RECT client{}; GetClientRect(window,&client);
        control(L"STATIC", currentText->intro,
                0,18,16,client.right-36,48,101);
        control(L"STATIC", L"", 0,18,76,client.right-36,72,100);
        control(L"BUTTON", currentText->cancel, WS_TABSTOP | BS_PUSHBUTTON,
                18,client.bottom-44,client.right-36,28,IDCANCEL);
        RECT rect{}; GetWindowRect(window,&rect);
        SetWindowPos(window, HWND_TOP, (GetSystemMetrics(SM_CXSCREEN)-(rect.right-rect.left))/2,
            (GetSystemMetrics(SM_CYSCREEN)-(rect.bottom-rect.top))/2,0,0,SWP_NOSIZE);
        SetTimer(window,1,100,nullptr);
        return TRUE;
    }
    if (!context) return FALSE;
    if (message == WM_COMMAND && LOWORD(wp) == IDCANCEL) { Cancel(window,*context); return TRUE; }
    if (message == WM_CLOSE) { Cancel(window,*context); return TRUE; }
    if (message == WM_TIMER) {
        if (WaitForSingleObject(context->ready,0) == WAIT_OBJECT_0) {
            EndDialog(window,context->cancelled ? IDCANCEL : IDOK); return TRUE;
        }
        const ULONGLONG now=GetTickCount64();
        if (now-context->started >= 1800000) Cancel(window,*context);
        if (context->cancelled) {
            if(now-context->cancelled >= 5000) EndDialog(window,IDCANCEL);
        } else {
            AcquireSRWLockShared(&progressLock);
            const wchar_t* label=progressStage==Stage::Generating ? currentText->generating :
                progressStage==Stage::Verifying ? currentText->verifying :
                progressStage==Stage::Complete ? currentText->complete : currentText->checking;
            std::wstring text=label;
            if(!progress.empty()) text += L": " + progress;
            ReleaseSRWLockShared(&progressLock);
            text += std::wstring(L"\r\n")+currentText->elapsed+L": " + std::to_wstring((now-context->started)/1000) + L" s";
            SetDlgItemTextW(window,100,text.c_str());
        }
        return TRUE;
    }
    return FALSE;
}
inline bool Wait(HMODULE module,HANDLE ready,HANDLE cancel) {
    if (!ready || !cancel) return false;
    if (WaitForSingleObject(ready,0)==WAIT_OBJECT_0) return true;
    // A modal dialog provides a real Windows message loop during bank preparation.
    // The owner is disabled so input cannot re-enter the unfinished game initialization.
    alignas(DWORD) unsigned char bytes[sizeof(DLGTEMPLATE)+6]{};
    auto* definition=reinterpret_cast<DLGTEMPLATE*>(bytes);
    definition->style=WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    definition->cx=340; definition->cy=108;
    WaitContext context{ready,cancel,GetTickCount64()};
    const auto result=DialogBoxIndirectParamW(module,definition,GetActiveWindow(),Dialog,reinterpret_cast<LPARAM>(&context));
    if(result != IDOK) SetEvent(cancel);
    return result==IDOK;
}
}
